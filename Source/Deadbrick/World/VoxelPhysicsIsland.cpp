#include "World/VoxelPhysicsIsland.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Items/DeadbrickPickupItem.h"
#include "Materials/MaterialInterface.h"
#include "Physics/DeadbrickPhysXSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "World/DestructibleVoxelWorld.h"

namespace
{
    struct FIslandMeshBuffers
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
    };

    float DensityKgPerM3(EDeadbrickVoxelMaterial Material)
    {
        switch (Material)
        {
            case EDeadbrickVoxelMaterial::Wood: return 700.0f;
            case EDeadbrickVoxelMaterial::Brick: return 1800.0f;
            case EDeadbrickVoxelMaterial::Concrete: return 2400.0f;
            case EDeadbrickVoxelMaterial::Glass: return 2500.0f;
            case EDeadbrickVoxelMaterial::Metal: return 7800.0f;
            case EDeadbrickVoxelMaterial::Asphalt: return 2300.0f;
            case EDeadbrickVoxelMaterial::Soil: return 1600.0f;
            default: return 1000.0f;
        }
    }

    int32 FloorDivSmall(int32 Value, int32 Divisor)
    {
        return Value >= 0 ? Value / Divisor : -(((-Value) + Divisor - 1) / Divisor);
    }

    void AddBoxConvex(UProceduralMeshComponent* Mesh, const FBox& Box)
    {
        if (!Mesh || !Box.IsValid) return;
        const FVector Min = Box.Min;
        const FVector Max = Box.Max;
        TArray<FVector> Convex;
        Convex.Reserve(8);
        Convex.Add(FVector(Min.X, Min.Y, Min.Z));
        Convex.Add(FVector(Max.X, Min.Y, Min.Z));
        Convex.Add(FVector(Max.X, Max.Y, Min.Z));
        Convex.Add(FVector(Min.X, Max.Y, Min.Z));
        Convex.Add(FVector(Min.X, Min.Y, Max.Z));
        Convex.Add(FVector(Max.X, Min.Y, Max.Z));
        Convex.Add(FVector(Max.X, Max.Y, Max.Z));
        Convex.Add(FVector(Min.X, Max.Y, Max.Z));
        Mesh->AddCollisionConvexMesh(Convex);
    }
}

AVoxelPhysicsIsland::AVoxelPhysicsIsland()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(true);

    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VoxelPhysicsMesh"));
    SetRootComponent(MeshComponent);
    MeshComponent->bUseComplexAsSimpleCollision = false;
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetCanEverAffectNavigation(false);
    MeshComponent->SetLinearDamping(0.08f);
    MeshComponent->SetAngularDamping(0.25f);
    MeshComponent->SetNotifyRigidBodyCollision(true);
}

void AVoxelPhysicsIsland::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PhysXBodyHandle != INDEX_NONE && GetWorld())
    {
        if (UDeadbrickPhysXSubsystem* PhysX = GetWorld()->GetSubsystem<UDeadbrickPhysXSubsystem>())
            PhysX->DestroyBody(PhysXBodyHandle);
    }
    PhysXBodyHandle = INDEX_NONE;
    bUsingPhysX = false;
    Super::EndPlay(EndPlayReason);
}

void AVoxelPhysicsIsland::InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels, bool bStartSimulating)
{
    if (!SourceWorld || Voxels.Num() == 0 || !MeshComponent) return;

    SourceVoxelWorld = SourceWorld;
    SourceVoxelCount = Voxels.Num();
    RubbleDurability = 55.0f + FMath::Sqrt((float)SourceVoxelCount) * 18.0f;

    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->ClearAllMeshSections();
    MeshComponent->ClearCollisionConvexMeshes();
    SetActorHiddenInGame(!bStartSimulating);

    const float VoxelSize = SourceWorld->VoxelSizeCm;
    const float VoxelSizeMeters = VoxelSize / 100.0f;
    const float VoxelVolumeM3 = VoxelSizeMeters * VoxelSizeMeters * VoxelSizeMeters;
    const FVector Origin = SourceWorld->VoxelToWorld(Voxels[0]);
    SetActorLocation(Origin);

    TSet<FIntVector> CellSet;
    CellSet.Reserve(Voxels.Num());
    for (const FIntVector& Voxel : Voxels) CellSet.Add(Voxel);

    TArray<FIslandMeshBuffers> Buffers;
    Buffers.SetNum(8);
    TArray<FVector> AllVertices;
    TMap<EDeadbrickVoxelMaterial, int32> MaterialCounts;
    TMap<FIntVector, FBox> QueryCollisionBuckets;
    float EstimatedMassKg = 0.0f;

    const FIntVector Directions[6] =
    {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };
    const FVector FaceNormals[6] =
    {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0),
        FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)
    };

    auto AddFace = [&](FIslandMeshBuffers& Buffer, const FVector& Center, int32 Face)
    {
        const FVector N = FaceNormals[Face];
        const FVector AxisA = FMath::Abs(N.Z) > 0.5f ? FVector(1,0,0) : FVector(0,0,1);
        const FVector AxisB = FVector::CrossProduct(N, AxisA);
        const float H = VoxelSize * 0.5f;
        const FVector FaceCenter = Center + N * H;
        const int32 Base = Buffer.Vertices.Num();

        const FVector V0 = FaceCenter + (-AxisA - AxisB) * H;
        const FVector V1 = FaceCenter + ( AxisA - AxisB) * H;
        const FVector V2 = FaceCenter + ( AxisA + AxisB) * H;
        const FVector V3 = FaceCenter + (-AxisA + AxisB) * H;
        Buffer.Vertices.Append({V0, V1, V2, V3});
        AllVertices.Append({V0, V1, V2, V3});
        Buffer.Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
        for (int32 I = 0; I < 4; ++I)
        {
            Buffer.Normals.Add(N);
            Buffer.Colors.Add(FLinearColor::White);
            Buffer.Tangents.Add(FProcMeshTangent(AxisA, false));
        }
        Buffer.UVs.Append({FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1)});
    };

    for (const FIntVector& Cell : Voxels)
    {
        FDeadbrickVoxel SourceVoxel;
        if (!SourceWorld->GetVoxel(Cell, SourceVoxel)) continue;

        const int32 MaterialIndex = (int32)SourceVoxel.Material;
        if (!Buffers.IsValidIndex(MaterialIndex) || MaterialIndex <= 0) continue;

        MaterialCounts.FindOrAdd(SourceVoxel.Material) += 1;
        EstimatedMassKg += DensityKgPerM3(SourceVoxel.Material) * VoxelVolumeM3;

        FIslandMeshBuffers& Buffer = Buffers[MaterialIndex];
        const FVector Center = SourceWorld->VoxelToWorld(Cell) - Origin;
        for (int32 Face = 0; Face < 6; ++Face)
        {
            if (!CellSet.Contains(Cell + Directions[Face])) AddFace(Buffer, Center, Face);
        }

        const FIntVector BucketKey(
            FloorDivSmall(Cell.X, 4),
            FloorDivSmall(Cell.Y, 4),
            FloorDivSmall(Cell.Z, 4));
        FBox* Bucket = QueryCollisionBuckets.Find(BucketKey);
        if (!Bucket)
        {
            QueryCollisionBuckets.Add(BucketKey, FBox(EForceInit::ForceInit));
            Bucket = QueryCollisionBuckets.Find(BucketKey);
        }
        if (Bucket)
        {
            const FVector H(VoxelSize * 0.5f);
            *Bucket += Center - H;
            *Bucket += Center + H;
        }
    }

    int32 HighestMaterialCount = 0;
    for (const TPair<EDeadbrickVoxelMaterial, int32>& Pair : MaterialCounts)
    {
        if (Pair.Value > HighestMaterialCount)
        {
            HighestMaterialCount = Pair.Value;
            DominantMaterial = Pair.Key;
        }
    }

    int32 SectionIndex = 0;
    for (int32 MaterialIndex = 1; MaterialIndex < Buffers.Num(); ++MaterialIndex)
    {
        FIslandMeshBuffers& Buffer = Buffers[MaterialIndex];
        if (Buffer.Vertices.Num() == 0) continue;

        MeshComponent->CreateMeshSection_LinearColor(
            SectionIndex,
            Buffer.Vertices,
            Buffer.Triangles,
            Buffer.Normals,
            Buffer.UVs,
            Buffer.Colors,
            Buffer.Tangents,
            false,
            false);

        if (UMaterialInterface* Material = SourceWorld->GetSurfaceMaterialForVoxel((EDeadbrickVoxelMaterial)MaterialIndex))
            MeshComponent->SetMaterial(SectionIndex, Material);
        ++SectionIndex;
    }

    if (AllVertices.Num() > 0)
    {
        FBox LocalBounds(EForceInit::ForceInit);
        for (const FVector& Vertex : AllVertices) LocalBounds += Vertex;
        PreparedLocalCenter = LocalBounds.GetCenter();
        PreparedHalfExtents = LocalBounds.GetExtent().ComponentMax(FVector(0.5f));

        int32 AddedConvexes = 0;
        for (const TPair<FIntVector, FBox>& Pair : QueryCollisionBuckets)
        {
            AddBoxConvex(MeshComponent, Pair.Value);
            if (++AddedConvexes >= 24) break;
        }

        if (AddedConvexes == 0)
            AddBoxConvex(MeshComponent, LocalBounds);
    }

    PreparedMassKg = FMath::Clamp(EstimatedMassKg, 1.0f, 500000.0f);
    bPreparedForPhysics = AllVertices.Num() > 0;

    if (bStartSimulating) ActivatePhysics();
}

void AVoxelPhysicsIsland::ActivatePhysics()
{
    if (!bPreparedForPhysics || !MeshComponent) return;

    SetActorHiddenInGame(false);

    if (GetWorld())
    {
        if (UDeadbrickPhysXSubsystem* PhysX = GetWorld()->GetSubsystem<UDeadbrickPhysXSubsystem>())
        {
            if (PhysX->IsPhysXReady())
            {
                MeshComponent->SetSimulatePhysics(false);
                MeshComponent->SetEnableGravity(false);
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

                PhysXBodyHandle = PhysX->CreateDynamicBox(
                    this,
                    PreparedLocalCenter,
                    PreparedHalfExtents,
                    PreparedMassKg,
                    0.12f,
                    0.32f);

                if (PhysXBodyHandle != INDEX_NONE)
                {
                    bUsingPhysX = true;
                    UE_LOG(LogTemp, VeryVerbose,
                        TEXT("DEADBRICK interactive rubble activated in PhysX5 | voxels=%d | mass=%.1f kg | halfExtent=%s"),
                        SourceVoxelCount,
                        PreparedMassKg,
                        *PreparedHalfExtents.ToCompactString());
                    return;
                }
            }
        }
    }

    bUsingPhysX = false;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetUseCCD(true, NAME_None);
    MeshComponent->SetMaxDepenetrationVelocity(NAME_None, 1000.0f);
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    MeshComponent->SetMassOverrideInKg(NAME_None, PreparedMassKg, true);
    MeshComponent->WakeAllRigidBodies();
    UE_LOG(LogTemp, Warning, TEXT("DEADBRICK voxel island fell back to Chaos because PhysX5 was unavailable."));
}

void AVoxelPhysicsIsland::PushFromGameplay(const FVector& Impulse)
{
    ApplyGameplayImpulse(Impulse);
}

void AVoxelPhysicsIsland::ApplyGameplayImpulse(const FVector& Impulse)
{
    if (Impulse.IsNearlyZero() || !MeshComponent) return;

    if (bUsingPhysX && PhysXBodyHandle != INDEX_NONE && GetWorld())
    {
        if (UDeadbrickPhysXSubsystem* PhysX = GetWorld()->GetSubsystem<UDeadbrickPhysXSubsystem>())
        {
            PhysX->AddImpulse(PhysXBodyHandle, Impulse);
            return;
        }
    }

    if (MeshComponent->IsSimulatingPhysics())
        MeshComponent->AddImpulse(Impulse, NAME_None, false);
}

float AVoxelPhysicsIsland::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    const float AppliedDamage = FMath::Max(0.0f, DamageAmount);
    if (AppliedDamage <= 0.0f) return 0.0f;

    FVector ImpulseDirection = DamageCauser ? DamageCauser->GetActorForwardVector() : FVector::ZeroVector;
    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
        ImpulseDirection = PointEvent.ShotDirection.GetSafeNormal();
    }

    if (!ImpulseDirection.IsNearlyZero())
    {
        const float ImpulseMagnitude = FMath::Clamp(AppliedDamage * 220.0f, 1800.0f, 16000.0f);
        ApplyGameplayImpulse(ImpulseDirection * ImpulseMagnitude);
    }

    RubbleDurability -= AppliedDamage;
    if (RubbleDurability <= 0.0f)
        BreakIntoSalvage();

    return AppliedDamage;
}

void AVoxelPhysicsIsland::NotifyHit(
    UPrimitiveComponent* MyComp,
    AActor* Other,
    UPrimitiveComponent* OtherComp,
    bool bSelfMoved,
    FVector HitLocation,
    FVector HitNormal,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (!Other || Other == this) return;

    FVector PushVelocity = Other->GetVelocity();
    PushVelocity.Z = 0.0f;
    if (PushVelocity.SizeSquared() < FMath::Square(35.0f)) return;

    const float MassFactor = FMath::Clamp(PreparedMassKg * 0.12f, 12.0f, 80.0f);
    ApplyGameplayImpulse(PushVelocity.GetClampedToMaxSize(850.0f) * MassFactor);
}

void AVoxelPhysicsIsland::BreakIntoSalvage()
{
    if (!GetWorld())
    {
        Destroy();
        return;
    }

    const int32 DropCount = FMath::Clamp(FMath::CeilToInt((float)SourceVoxelCount / 12.0f), 1, 4);
    const int32 QuantityPerDrop = FMath::Max(1, FMath::CeilToInt((float)SourceVoxelCount / (float)(DropCount * 3)));
    const FVector BaseLocation = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);

    for (int32 Index = 0; Index < DropCount; ++Index)
    {
        const FVector Offset(
            FMath::FRandRange(-28.0f, 28.0f),
            FMath::FRandRange(-28.0f, 28.0f),
            FMath::FRandRange(0.0f, 24.0f));

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ADeadbrickPickupItem* Pickup = GetWorld()->SpawnActor<ADeadbrickPickupItem>(
            ADeadbrickPickupItem::StaticClass(), BaseLocation + Offset, FRotator::ZeroRotator, Params))
        {
            Pickup->InitializeFromVoxelMaterial(DominantMaterial, QuantityPerDrop);
        }
    }

    Destroy();
}
