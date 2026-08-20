#include "Physics/DeadbrickPhysXSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if WITH_DEADBRICK_PHYSX5
THIRD_PARTY_INCLUDES_START
#include "DeadbrickPhysXBridge.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
#if WITH_DEADBRICK_PHYSX5
    struct FDeadbrickPhysXBinding
    {
        TWeakObjectPtr<AActor> VisualActor;
        FVector LocalCenter = FVector::ZeroVector;
    };

    struct FDeadbrickPhysXState
    {
        DBPXScene* Scene = nullptr;
        TMap<int64, FDeadbrickPhysXBinding> Bodies;
    };

    static bool EnsureDeadbrickPhysXRuntimeLoaded()
    {
        static bool bAttempted = false;
        static bool bLoaded = false;
        static TArray<void*> RuntimeHandles;

        if (bAttempted)
            return bLoaded;

        bAttempted = true;

        const FString RuntimeDir = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/PhysX5/SDK/bin/Win64")));

        const TCHAR* RuntimeDlls[] =
        {
            TEXT("PhysXFoundation_64.dll"),
            TEXT("PhysXCommon_64.dll"),
            TEXT("PhysX_64.dll")
        };

        for (const TCHAR* DllName : RuntimeDlls)
        {
            const FString DllPath = FPaths::Combine(RuntimeDir, DllName);
            if (!FPaths::FileExists(DllPath))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("DEADBRICK PhysX5 runtime missing: %s. Run REBUILD_AND_OPEN_UE58.bat."),
                    *DllPath);
                return false;
            }

            void* Handle = FPlatformProcess::GetDllHandle(*DllPath);
            if (!Handle)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("DEADBRICK PhysX5 could not load runtime DLL: %s. PIE PhysX initialization aborted safely."),
                    *DllPath);
                return false;
            }

            RuntimeHandles.Add(Handle);
            UE_LOG(LogTemp, Display, TEXT("DEADBRICK PhysX5 runtime loaded: %s"), *DllPath);
        }

        bLoaded = RuntimeHandles.Num() == UE_ARRAY_COUNT(RuntimeDlls);
        return bLoaded;
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
    if (!EnsureDeadbrickPhysXRuntimeLoaded())
    {
        bPhysXReady = false;
        UE_LOG(LogTemp, Error,
            TEXT("DEADBRICK PHYSX 5.8 NOT STARTED | native runtime DLL loading failed; bridge was not called."));
        return;
    }

    FDeadbrickPhysXState* State = new FDeadbrickPhysXState();
    PhysXState = State;

    State->Scene = DBPX_CreateScene(0.0f);
    if (!State->Scene || !DBPX_IsReady(State->Scene))
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK PhysX5 bridge: scene creation failed."));
        if (State->Scene)
        {
            DBPX_DestroyScene(State->Scene);
            State->Scene = nullptr;
        }
        delete State;
        PhysXState = nullptr;
        return;
    }

    bPhysXReady = true;
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK PHYSX 5.8 READY | isolated native bridge | TGS | CCD | fixed 60 Hz"));
#else
    UE_LOG(LogTemp, Warning, TEXT("DEADBRICK PhysX5 bridge is not installed. Voxel rigid bodies will use the compatibility fallback."));
#endif
}

void UDeadbrickPhysXSubsystem::Deinitialize()
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (State)
    {
        State->Bodies.Reset();
        if (State->Scene)
        {
            DBPX_DestroyScene(State->Scene);
            State->Scene = nullptr;
        }
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
    if (!bPhysXReady || !State || !State->Scene)
        return;

    DBPX_Simulate(State->Scene, DeltaTime);

    TArray<int64> StaleHandles;
    for (TPair<int64, FDeadbrickPhysXBinding>& Pair : State->Bodies)
    {
        FDeadbrickPhysXBinding& Binding = Pair.Value;
        AActor* VisualActor = Binding.VisualActor.Get();
        if (!VisualActor)
        {
            StaleHandles.Add(Pair.Key);
            continue;
        }

        DBPXTransform Pose{};
        if (!DBPX_GetBodyTransform(State->Scene, Pair.Key, &Pose))
        {
            StaleHandles.Add(Pair.Key);
            continue;
        }

        const FQuat Rotation((double)Pose.QX, (double)Pose.QY, (double)Pose.QZ, (double)Pose.QW);
        const FQuat SafeRotation = Rotation.GetNormalized();
        const FVector BodyCenter((double)Pose.X, (double)Pose.Y, (double)Pose.Z);
        const FVector ActorOrigin = BodyCenter - SafeRotation.RotateVector(Binding.LocalCenter);
        VisualActor->SetActorLocationAndRotation(ActorOrigin, SafeRotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    for (const int64 Handle : StaleHandles)
        DestroyBody(Handle);
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
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (!bPhysXReady || !State || !State->Scene || !VisualActor)
        return INDEX_NONE;

    const FVector SafeHalfExtent(
        FMath::Max(0.5, HalfExtentsCm.X),
        FMath::Max(0.5, HalfExtentsCm.Y),
        FMath::Max(0.5, HalfExtentsCm.Z));
    const float SafeMass = FMath::Clamp(MassKg, 1.0f, 500000.0f);

    const FTransform ActorTransform = VisualActor->GetActorTransform();
    const FVector WorldCenter = ActorTransform.TransformPosition(LocalCenter);
    const FQuat Rotation = ActorTransform.GetRotation().GetNormalized();

    DBPXTransform InitialTransform{};
    InitialTransform.X = (float)WorldCenter.X;
    InitialTransform.Y = (float)WorldCenter.Y;
    InitialTransform.Z = (float)WorldCenter.Z;
    InitialTransform.QX = (float)Rotation.X;
    InitialTransform.QY = (float)Rotation.Y;
    InitialTransform.QZ = (float)Rotation.Z;
    InitialTransform.QW = (float)Rotation.W;

    const int64 Handle = DBPX_CreateDynamicBox(
        State->Scene,
        &InitialTransform,
        (float)SafeHalfExtent.X,
        (float)SafeHalfExtent.Y,
        (float)SafeHalfExtent.Z,
        SafeMass,
        FMath::Max(0.0f, LinearDamping),
        FMath::Max(0.0f, AngularDamping));

    if (Handle < 0)
        return INDEX_NONE;

    FDeadbrickPhysXBinding Binding;
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
    if (!State || !State->Scene || Handle == INDEX_NONE)
        return;

    DBPX_DestroyBody(State->Scene, Handle);
    State->Bodies.Remove(Handle);
#endif
}

void UDeadbrickPhysXSubsystem::AddImpulse(int64 Handle, const FVector& Impulse)
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (!bPhysXReady || !State || !State->Scene || Handle == INDEX_NONE)
        return;

    DBPX_AddImpulse(State->Scene, Handle, (float)Impulse.X, (float)Impulse.Y, (float)Impulse.Z);
#endif
}

void UDeadbrickPhysXSubsystem::SetGroundHeight(float GroundHeightCm)
{
#if WITH_DEADBRICK_PHYSX5
    FDeadbrickPhysXState* State = static_cast<FDeadbrickPhysXState*>(PhysXState);
    if (bPhysXReady && State && State->Scene)
        DBPX_SetGroundHeight(State->Scene, GroundHeightCm);
#endif
}
