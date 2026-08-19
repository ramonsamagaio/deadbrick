#include "Physics/DeadbrickPhysXSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEADBRICK_PHYSX5
THIRD_PARTY_INCLUDES_START
#include "PxPhysicsAPI.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
#if WITH_DEADBRICK_PHYSX5
    using namespace physx;

    struct FDeadbrickPhysXBinding
    {
        PxRigidDynamic* Body = nullptr;
        TWeakObjectPtr<AActor> VisualActor;
        FVector LocalCenter = FVector::ZeroVector;
    };

    struct FDeadbrickPhysXState
    {
        PxDefaultAllocator Allocator;
        PxDefaultErrorCallback ErrorCallback;
        PxFoundation* Foundation = nullptr;
        PxPhysics* Physics = nullptr;
        PxDefaultCpuDispatcher* Dispatcher = nullptr;
        PxScene* Scene = nullptr;
        PxMaterial* Material = nullptr;
        PxRigidStatic* Ground = nullptr;
        TMap<int64, FDeadbrickPhysXBinding> Bodies;
        int64 NextHandle = 1;
        float Accumulator = 0.0f;
        float GroundHeightCm = 0.0f;

        static constexpr float FixedStepSeconds = 1.0f / 60.0f;
        static constexpr int32 MaxSubstepsPerFrame = 4;
    };

    static PxVec3 ToPxVector(const FVector& V)
    {
        return PxVec3((PxReal)V.X, (PxReal)V.Y, (PxReal)V.Z);
    }

    static FVector ToUnrealVector(const PxVec3& V)
    {
        return FVector((double)V.x, (double)V.y, (double)V.z);
    }

    static PxQuat ToPxQuat(const FQuat& Q)
    {
        return PxQuat((PxReal)Q.X, (PxReal)Q.Y, (PxReal)Q.Z, (PxReal)Q.W);
    }

    static FQuat ToUnrealQuat(const PxQuat& Q)
    {
        return FQuat((double)Q.x, (double)Q.y, (double)Q.z, (double)Q.w).GetNormalized();
    }

    static void ReleaseGround(FDeadbrickPhysXState& State)
    {
        if (State.Ground)
        {
            State.Ground->release();
            State.Ground = nullptr;
        }
    }

    static void CreateGround(FDeadbrickPhysXState& State, float GroundHeightCm)
    {
        if (!State.Physics || !State.Scene || !State.Material) return;

        ReleaseGround(State);
        State.GroundHeightCm = GroundHeightCm;

        // A huge static slab is deliberately used instead of a PhysX infinite plane. It matches the
        // Z-up centimetre convention used by Unreal and gives detached voxel macro bodies a stable base.
        constexpr float GroundHalfThickness = 5000.0f;
        constexpr float GroundHalfSpan = 10000000.0f;
        const PxTransform Pose(PxVec3(0.0f, 0.0f, GroundHeightCm - GroundHalfThickness));
        State.Ground = State.Physics->createRigidStatic(Pose);
        if (!State.Ground) return;

        PxShape* Shape = State.Physics->createShape(
            PxBoxGeometry(GroundHalfSpan, GroundHalfSpan, GroundHalfThickness),
            *State.Material,
            true);
        if (!Shape)
        {
            State.Ground->release();
            State.Ground = nullptr;
            return;
        }

        State.Ground->attachShape(*Shape);
        Shape->release();
        State.Scene->addActor(*State.Ground);
    }
#endif
}

bool UDeadbrickPhysXSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickPhysXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

#if WITH_DEADBRICK_PHYSX5
    using namespace physx;

    FDeadbrickPhysXState* State = new FDeadbrickPhysXState();
    PhysXState = State;

    State->Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, State->Allocator, State->ErrorCallback);
    if (!State->Foundation)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5: PxCreateFoundation failed."));
        return;
    }

    PxTolerancesScale Scale;
    Scale.length = 100.0f;   // Unreal unit: centimetre.
    Scale.speed = 1000.0f;   // Roughly ten metres per second in centimetres.

    State->Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *State->Foundation, Scale, false, nullptr);
    if (!State->Physics)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5: PxCreatePhysics failed."));
        return;
    }

    PxSceneDesc SceneDesc(State->Physics->getTolerancesScale());
    SceneDesc.gravity = PxVec3(0.0f, 0.0f, -980.665f);
    SceneDesc.solverType = PxSolverType::eTGS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    State->Dispatcher = PxDefaultCpuDispatcherCreate(4);
    SceneDesc.cpuDispatcher = State->Dispatcher;
    SceneDesc.filterShader = PxDefaultSimulationFilterShader;

    if (!SceneDesc.isValid() || !State->Dispatcher)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5: invalid scene descriptor or dispatcher creation failed."));
        return;
    }

    State->Scene = State->Physics->createScene(SceneDesc);
    if (!State->Scene)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5: scene creation failed."));
        return;
    }

    State->Material = State->Physics->createMaterial(0.80f, 0.72f, 0.08f);
    if (!State->Material)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5: default rubble material creation failed."));
        return;
    }

    CreateGround(*State, 0.0f);
    bPhysXReady = true;
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK PHYSX 5.8 READY | native voxel rigid-body scene | TGS | CCD | fixed 60 Hz"));
#else
    UE_LOG(LogTemp, Warning, TEXT("DEADBRICK PhysX5 SDK is not installed. Voxel rigid bodies will use the compatibility fallback."));
#endif
}

void UDeadbrickPhysXSubsystem::Deinitialize()
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (State)
    {
        for (TPair<int64, FDeadbrickPhysXBinding>& Pair : State->Bodies)
        {
            if (Pair.Value.Body)
            {
                Pair.Value.Body->release();
                Pair.Value.Body = nullptr;
            }
        }
        State->Bodies.Reset();
        ReleaseGround(*State);

        if (State->Material) { State->Material->release(); State->Material = nullptr; }
        if (State->Scene) { State->Scene->release(); State->Scene = nullptr; }
        if (State->Dispatcher) { State->Dispatcher->release(); State->Dispatcher = nullptr; }
        if (State->Physics) { State->Physics->release(); State->Physics = nullptr; }
        if (State->Foundation) { State->Foundation->release(); State->Foundation = nullptr; }

        delete State;
    }
#endif

    PhysXState = nullptr;
    bPhysXReady = false;
    Super::Deinitialize();
}

void UDeadbrickPhysXSubsystem::Tick(float DeltaTime)
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (!bPhysXReady || !State || !State->Scene) return;

    State->Accumulator += FMath::Clamp(DeltaTime, 0.0f, 0.10f);
    int32 Steps = 0;
    while (State->Accumulator >= FDeadbrickPhysXState::FixedStepSeconds &&
           Steps < FDeadbrickPhysXState::MaxSubstepsPerFrame)
    {
        State->Scene->simulate(FDeadbrickPhysXState::FixedStepSeconds);
        State->Scene->fetchResults(true);
        State->Accumulator -= FDeadbrickPhysXState::FixedStepSeconds;
        ++Steps;
    }

    // Do not let a long hitch create a physics death spiral on the next frame.
    if (Steps == FDeadbrickPhysXState::MaxSubstepsPerFrame)
        State->Accumulator = FMath::Min(State->Accumulator, FDeadbrickPhysXState::FixedStepSeconds);

    TArray<int64> StaleHandles;
    for (TPair<int64, FDeadbrickPhysXBinding>& Pair : State->Bodies)
    {
        FDeadbrickPhysXBinding& Binding = Pair.Value;
        AActor* VisualActor = Binding.VisualActor.Get();
        if (!VisualActor || !Binding.Body)
        {
            StaleHandles.Add(Pair.Key);
            continue;
        }

        const physx::PxTransform Pose = Binding.Body->getGlobalPose();
        const FQuat Rotation = ToUnrealQuat(Pose.q);
        const FVector BodyCenter = ToUnrealVector(Pose.p);
        const FVector ActorOrigin = BodyCenter - Rotation.RotateVector(Binding.LocalCenter);
        VisualActor->SetActorLocationAndRotation(ActorOrigin, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    for (const int64 Handle : StaleHandles) DestroyBody(Handle);
#endif
}

TStatId UDeadbrickPhysXSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDeadbrickPhysXSubsystem, STATGROUP_Tickables);
}

int64 UDeadbrickPhysXSubsystem::CreateDynamicBox(
    AActor* VisualActor,
    const FVector& LocalCenter,
    const FVector& HalfExtentsCm,
    float MassKg,
    float LinearDamping,
    float AngularDamping)
{
#if WITH_DEADBRICK_PHYSX5
    using namespace physx;

    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (!bPhysXReady || !State || !State->Physics || !State->Scene || !State->Material || !VisualActor)
        return INDEX_NONE;

    const FVector SafeHalfExtent(
        FMath::Max(0.5, HalfExtentsCm.X),
        FMath::Max(0.5, HalfExtentsCm.Y),
        FMath::Max(0.5, HalfExtentsCm.Z));
    const float SafeMass = FMath::Clamp(MassKg, 1.0f, 500000.0f);

    const FTransform ActorTransform = VisualActor->GetActorTransform();
    const FVector WorldCenter = ActorTransform.TransformPosition(LocalCenter);
    PxRigidDynamic* Body = State->Physics->createRigidDynamic(
        PxTransform(ToPxVector(WorldCenter), ToPxQuat(ActorTransform.GetRotation())));
    if (!Body) return INDEX_NONE;

    PxShape* Shape = State->Physics->createShape(
        PxBoxGeometry((PxReal)SafeHalfExtent.X, (PxReal)SafeHalfExtent.Y, (PxReal)SafeHalfExtent.Z),
        *State->Material,
        true);
    if (!Shape)
    {
        Body->release();
        return INDEX_NONE;
    }

    Body->attachShape(*Shape);
    Shape->release();

    Body->setMass(SafeMass);
    const float Ixx = SafeMass / 3.0f * (SafeHalfExtent.Y * SafeHalfExtent.Y + SafeHalfExtent.Z * SafeHalfExtent.Z);
    const float Iyy = SafeMass / 3.0f * (SafeHalfExtent.X * SafeHalfExtent.X + SafeHalfExtent.Z * SafeHalfExtent.Z);
    const float Izz = SafeMass / 3.0f * (SafeHalfExtent.X * SafeHalfExtent.X + SafeHalfExtent.Y * SafeHalfExtent.Y);
    Body->setMassSpaceInertiaTensor(PxVec3(FMath::Max(1.0f, Ixx), FMath::Max(1.0f, Iyy), FMath::Max(1.0f, Izz)));
    Body->setLinearDamping(FMath::Max(0.0f, LinearDamping));
    Body->setAngularDamping(FMath::Max(0.0f, AngularDamping));
    Body->setMaxDepenetrationVelocity(4000.0f);
    Body->setSolverIterationCounts(8, 2);
    Body->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    State->Scene->addActor(*Body);

    const int64 Handle = State->NextHandle++;
    FDeadbrickPhysXBinding Binding;
    Binding.Body = Body;
    Binding.VisualActor = VisualActor;
    Binding.LocalCenter = LocalCenter;
    State->Bodies.Add(Handle, MoveTemp(Binding));
    return Handle;
#else
    return INDEX_NONE;
#endif
}

void UDeadbrickPhysXSubsystem::DestroyBody(int64 Handle)
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (!State || Handle == INDEX_NONE) return;

    if (FDeadbrickPhysXBinding* Binding = State->Bodies.Find(Handle))
    {
        if (Binding->Body)
        {
            Binding->Body->release();
            Binding->Body = nullptr;
        }
        State->Bodies.Remove(Handle);
    }
#endif
}

void UDeadbrickPhysXSubsystem::SetGroundHeight(float GroundHeightCm)
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (bPhysXReady && State) CreateGround(*State, GroundHeightCm);
#endif
}
