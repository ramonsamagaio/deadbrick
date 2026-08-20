#include "DeadbrickPhysXBridge.h"

#include "PxPhysicsAPI.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "extensions/PxExtensionsAPI.h"

#include <algorithm>
#include <unordered_map>

using namespace physx;

struct DBPXScene
{
    PxDefaultAllocator Allocator;
    PxDefaultErrorCallback ErrorCallback;
    PxFoundation* Foundation = nullptr;
    PxPhysics* Physics = nullptr;
    PxDefaultCpuDispatcher* Dispatcher = nullptr;
    PxScene* Scene = nullptr;
    PxMaterial* Material = nullptr;
    PxRigidStatic* Ground = nullptr;
    std::unordered_map<int64_t, PxRigidDynamic*> Bodies;
    int64_t NextHandle = 1;
    float Accumulator = 0.0f;
    bool ExtensionsInitialized = false;
};

namespace
{
    constexpr float FixedStepSeconds = 1.0f / 60.0f;
    constexpr int MaxSubstepsPerFrame = 4;

    static PxVec3 ToPxVector(float X, float Y, float Z)
    {
        return PxVec3(X, -Y, Z);
    }

    static PxQuat ToPxQuat(float X, float Y, float Z, float W)
    {
        PxQuat Q(-X, Y, -Z, W);
        Q.normalize();
        return Q;
    }

    static void FromPxTransform(const PxTransform& Pose, DBPXTransform& Out)
    {
        Out.X = Pose.p.x;
        Out.Y = -Pose.p.y;
        Out.Z = Pose.p.z;
        Out.QX = -Pose.q.x;
        Out.QY = Pose.q.y;
        Out.QZ = -Pose.q.z;
        Out.QW = Pose.q.w;
    }

    static void ReleaseGround(DBPXScene& State)
    {
        if (State.Ground)
        {
            State.Ground->release();
            State.Ground = nullptr;
        }
    }

    static bool CreateGround(DBPXScene& State, float GroundHeightCm)
    {
        if (!State.Physics || !State.Scene || !State.Material)
            return false;

        ReleaseGround(State);

        constexpr float GroundHalfThickness = 5000.0f;
        constexpr float GroundHalfSpan = 10000000.0f;
        const PxTransform Pose(PxVec3(0.0f, 0.0f, GroundHeightCm - GroundHalfThickness));
        State.Ground = State.Physics->createRigidStatic(Pose);
        if (!State.Ground)
            return false;

        PxShape* Shape = State.Physics->createShape(
            PxBoxGeometry(GroundHalfSpan, GroundHalfSpan, GroundHalfThickness),
            *State.Material,
            true);
        if (!Shape)
        {
            State.Ground->release();
            State.Ground = nullptr;
            return false;
        }

        State.Ground->attachShape(*Shape);
        Shape->release();
        State.Scene->addActor(*State.Ground);
        return true;
    }

    static void DestroyState(DBPXScene* State)
    {
        if (!State)
            return;

        for (auto& Pair : State->Bodies)
        {
            if (Pair.second)
                Pair.second->release();
        }
        State->Bodies.clear();

        ReleaseGround(*State);
        if (State->Scene) { State->Scene->release(); State->Scene = nullptr; }
        if (State->Material) { State->Material->release(); State->Material = nullptr; }
        if (State->Dispatcher) { State->Dispatcher->release(); State->Dispatcher = nullptr; }
        if (State->ExtensionsInitialized)
        {
            PxCloseExtensions();
            State->ExtensionsInitialized = false;
        }
        if (State->Physics) { State->Physics->release(); State->Physics = nullptr; }
        if (State->Foundation) { State->Foundation->release(); State->Foundation = nullptr; }
        delete State;
    }
}

extern "C" DBPXScene* DBPX_CreateScene(float GroundHeightCm)
{
    DBPXScene* State = new DBPXScene();

    State->Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, State->Allocator, State->ErrorCallback);
    if (!State->Foundation)
    {
        DestroyState(State);
        return nullptr;
    }

    PxTolerancesScale Scale;
    Scale.length = 100.0f;
    Scale.speed = 1000.0f;

    State->Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *State->Foundation, Scale, false, nullptr);
    if (!State->Physics)
    {
        DestroyState(State);
        return nullptr;
    }

    if (!PxInitExtensions(*State->Physics, nullptr))
    {
        DestroyState(State);
        return nullptr;
    }
    State->ExtensionsInitialized = true;

    PxSceneDesc SceneDesc(State->Physics->getTolerancesScale());
    SceneDesc.gravity = PxVec3(0.0f, 0.0f, -980.665f);
    SceneDesc.solverType = PxSolverType::eTGS;
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
    State->Dispatcher = PxDefaultCpuDispatcherCreate(4);
    SceneDesc.cpuDispatcher = State->Dispatcher;
    SceneDesc.filterShader = PxDefaultSimulationFilterShader;

    if (!State->Dispatcher || !SceneDesc.isValid())
    {
        DestroyState(State);
        return nullptr;
    }

    State->Scene = State->Physics->createScene(SceneDesc);
    if (!State->Scene)
    {
        DestroyState(State);
        return nullptr;
    }

    State->Material = State->Physics->createMaterial(0.80f, 0.72f, 0.08f);
    if (!State->Material || !CreateGround(*State, GroundHeightCm))
    {
        DestroyState(State);
        return nullptr;
    }

    return State;
}

extern "C" void DBPX_DestroyScene(DBPXScene* Scene)
{
    DestroyState(Scene);
}

extern "C" int32_t DBPX_IsReady(const DBPXScene* Scene)
{
    return Scene && Scene->Foundation && Scene->Physics && Scene->Scene && Scene->Material ? 1 : 0;
}

extern "C" void DBPX_Simulate(DBPXScene* Scene, float DeltaSeconds)
{
    if (!DBPX_IsReady(Scene))
        return;

    Scene->Accumulator += std::clamp(DeltaSeconds, 0.0f, 0.10f);
    int Steps = 0;
    while (Scene->Accumulator >= FixedStepSeconds && Steps < MaxSubstepsPerFrame)
    {
        Scene->Scene->simulate(FixedStepSeconds);
        Scene->Scene->fetchResults(true);
        Scene->Accumulator -= FixedStepSeconds;
        ++Steps;
    }

    if (Steps == MaxSubstepsPerFrame)
        Scene->Accumulator = std::min(Scene->Accumulator, FixedStepSeconds);
}

extern "C" void DBPX_SetGroundHeight(DBPXScene* Scene, float GroundHeightCm)
{
    if (DBPX_IsReady(Scene))
        CreateGround(*Scene, GroundHeightCm);
}

extern "C" int64_t DBPX_CreateDynamicBox(
    DBPXScene* Scene,
    const DBPXTransform* InitialTransform,
    float HalfExtentX,
    float HalfExtentY,
    float HalfExtentZ,
    float MassKg,
    float LinearDamping,
    float AngularDamping)
{
    if (!DBPX_IsReady(Scene) || !InitialTransform)
        return -1;

    const float HX = std::max(0.5f, HalfExtentX);
    const float HY = std::max(0.5f, HalfExtentY);
    const float HZ = std::max(0.5f, HalfExtentZ);
    const float SafeMass = std::clamp(MassKg, 1.0f, 500000.0f);

    PxRigidDynamic* Body = Scene->Physics->createRigidDynamic(PxTransform(
        ToPxVector(InitialTransform->X, InitialTransform->Y, InitialTransform->Z),
        ToPxQuat(InitialTransform->QX, InitialTransform->QY, InitialTransform->QZ, InitialTransform->QW)));
    if (!Body)
        return -1;

    PxShape* Shape = Scene->Physics->createShape(PxBoxGeometry(HX, HY, HZ), *Scene->Material, true);
    if (!Shape)
    {
        Body->release();
        return -1;
    }

    Body->attachShape(*Shape);
    Shape->release();

    Body->setMass(SafeMass);
    const float Ixx = SafeMass / 3.0f * (HY * HY + HZ * HZ);
    const float Iyy = SafeMass / 3.0f * (HX * HX + HZ * HZ);
    const float Izz = SafeMass / 3.0f * (HX * HX + HY * HY);
    Body->setMassSpaceInertiaTensor(PxVec3(std::max(1.0f, Ixx), std::max(1.0f, Iyy), std::max(1.0f, Izz)));
    Body->setLinearDamping(std::max(0.0f, LinearDamping));
    Body->setAngularDamping(std::max(0.0f, AngularDamping));
    Body->setMaxDepenetrationVelocity(4000.0f);
    Body->setSolverIterationCounts(8, 2);
    Body->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    Scene->Scene->addActor(*Body);

    const int64_t Handle = Scene->NextHandle++;
    Scene->Bodies.emplace(Handle, Body);
    return Handle;
}

extern "C" void DBPX_DestroyBody(DBPXScene* Scene, int64_t Handle)
{
    if (!Scene || Handle < 0)
        return;

    const auto It = Scene->Bodies.find(Handle);
    if (It == Scene->Bodies.end())
        return;

    if (It->second)
        It->second->release();
    Scene->Bodies.erase(It);
}

extern "C" int32_t DBPX_GetBodyTransform(DBPXScene* Scene, int64_t Handle, DBPXTransform* OutTransform)
{
    if (!Scene || !OutTransform)
        return 0;

    const auto It = Scene->Bodies.find(Handle);
    if (It == Scene->Bodies.end() || !It->second)
        return 0;

    FromPxTransform(It->second->getGlobalPose(), *OutTransform);
    return 1;
}
