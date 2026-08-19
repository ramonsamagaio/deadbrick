#include "World/DestructibleVoxelWorld.h"

#include "Items/DeadbrickPickupItem.h"
#include "Reference/ReferenceAssetResolver.h"
#include "World/VoxelPhysicsIsland.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    struct FDeadbrickMeshBuffers
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
    };
}

ADestructibleVoxelWorld::ADestructibleVoxelWorld()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

int32 ADestructibleVoxelWorld::FloorDiv(int32 Value, int32 Divisor)
{
    const int32 Quotient = Value / Divisor;
    const int32 Remainder = Value % Divisor;
    return (Remainder != 0 && ((Remainder < 0) != (Divisor < 0))) ? Quotient - 1 : Quotient;
}

int32 ADestructibleVoxelWorld::PositiveMod(int32 Value, int32 Divisor)
{
    const int32 Remainder = Value % Divisor;
    return Remainder < 0 ? Remainder + Divisor : Remainder;
}

FIntVector ADestructibleVoxelWorld::WorldToVoxel(const FVector& WorldPosition) const
{
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition) / VoxelSizeCm;
    return FIntVector(FMath::FloorToInt(Local.X), FMath::FloorToInt(Local.Y), FMath::FloorToInt(Local.Z));
}

FVector ADestructibleVoxelWorld::VoxelToWorld(const FIntVector& Voxel) const
{
    const FVector Local((Voxel.X + 0.5f) * VoxelSizeCm, (Voxel.Y + 0.5f) * VoxelSizeCm, (Voxel.Z + 0.5f) * VoxelSizeCm);
    return GetActorTransform().TransformPosition(Local);
}

FIntVector ADestructibleVoxelWorld::ToChunkCoord(const FIntVector& Voxel) const
{
    return FIntVector(FloorDiv(Voxel.X, ChunkSize), FloorDiv(Voxel.Y, ChunkSize), FloorDiv(Voxel.Z, ChunkSize));
}

FIntVector ADestructibleVoxelWorld::ToLocalCoord(const FIntVector& Voxel) const
{
    return FIntVector(PositiveMod(Voxel.X, ChunkSize), PositiveMod(Voxel.Y, ChunkSize), PositiveMod(Voxel.Z, ChunkSize));
}

int32 ADestructibleVoxelWorld::ToIndex(const FIntVector& Local) const
{
    return Local.X + ChunkSize * (Local.Y + ChunkSize * Local.Z);
}

FDeadbrickVoxelChunk& ADestructibleVoxelWorld::FindOrCreateChunk(const FIntVector& ChunkCoord)
{
    FDeadbrickVoxelChunk& Chunk = Chunks.FindOrAdd(ChunkCoord);
    const int32 Required = ChunkSize * ChunkSize * ChunkSize;
    if (Chunk.Voxels.Num() != Required) Chunk.Voxels.SetNum(Required);
    return Chunk;
}

bool ADestructibleVoxelWorld::GetVoxel(const FIntVector& Voxel, FDeadbrickVoxel& OutVoxel) const
{
    const FIntVector ChunkCoord = ToChunkCoord(Voxel);
    const FDeadbrickVoxelChunk* Chunk = Chunks.Find(ChunkCoord);
    if (!Chunk || Chunk->Voxels.Num() == 0)
    {
        OutVoxel = FDeadbrickVoxel();
        return false;
    }

    OutVoxel = Chunk->Voxels[ToIndex(ToLocalCoord(Voxel))];
    return OutVoxel.IsSolid();
}

uint8 ADestructibleVoxelWorld::DefaultIntegrityFor(EDeadbrickVoxelMaterial Material) const
{
    switch (Material)
    {
        case EDeadbrickVoxelMaterial::Glass: return 40;
        case EDeadbrickVoxelMaterial::Wood: return 90;
        case EDeadbrickVoxelMaterial::Brick: return 150;
        case EDeadbrickVoxelMaterial::Concrete: return 210;
        case EDeadbrickVoxelMaterial::Metal: return 255;
        case EDeadbrickVoxelMaterial::Asphalt: return 180;
        case EDeadbrickVoxelMaterial::Soil: return 80;
        default: return 0;
    }
}

UMaterialInterface* ADestructibleVoxelWorld::ResolveSurfaceMaterial(EDeadbrickVoxelMaterial Material)
{
    if (MaterialResolutionAttempted.Contains(Material))
    {
        if (const TObjectPtr<UMaterialInterface>* Found = MaterialCache.Find(Material)) return Found->Get();
        return nullptr;
    }

    MaterialResolutionAttempted.Add(Material);
    TArray<FString> Keywords;
    switch (Material)
    {
        case EDeadbrickVoxelMaterial::Asphalt: Keywords = {TEXT("asphalt"), TEXT("road"), TEXT("path")}; break;
        case EDeadbrickVoxelMaterial::Concrete: Keywords = {TEXT("concrete"), TEXT("cement"), TEXT("stone")}; break;
        case EDeadbrickVoxelMaterial::Brick: Keywords = {TEXT("brick"), TEXT("masonry"), TEXT("stone")}; break;
        case EDeadbrickVoxelMaterial::Glass: Keywords = {TEXT("glass"), TEXT("window")}; break;
        case EDeadbrickVoxelMaterial::Wood: Keywords = {TEXT("wood"), TEXT("plank"), TEXT("timber")}; break;
        case EDeadbrickVoxelMaterial::Metal: Keywords = {TEXT("metal"), TEXT("iron"), TEXT("steel")}; break;
        case EDeadbrickVoxelMaterial::Soil: Keywords = {TEXT("soil"), TEXT("dirt"), TEXT("ground"), TEXT("earth")}; break;
        default: return nullptr;
    }

    UMaterialInterface* Resolved = DeadbrickReferenceAssets::FindMaterial(Keywords);
    MaterialCache.Add(Material, Resolved);
    return Resolved;
}

void ADestructibleVoxelWorld::SetVoxel(const FIntVector& Voxel, EDeadbrickVoxelMaterial Material, uint8 Integrity)
{
    const FIntVector ChunkCoord = ToChunkCoord(Voxel);
    FDeadbrickVoxelChunk& Chunk = FindOrCreateChunk(ChunkCoord);
    FDeadbrickVoxel& Cell = Chunk.Voxels[ToIndex(ToLocalCoord(Voxel))];
    Cell.Material = Material;
    Cell.Integrity = Material == EDeadbrickVoxelMaterial::Air ? 0 : (Integrity == 255 ? DefaultIntegrityFor(Material) : Integrity);
    if (bRecordRuntimeEdits) RuntimeEdits.Add(Voxel, Cell);
    MarkDirty(ChunkCoord);
}

void ADestructibleVoxelWorld::FillBox(const FIntVector& MinVoxel, const FIntVector& MaxVoxel, EDeadbrickVoxelMaterial Material, uint8 Integrity)
{
    BeginBulkEdit();
    for (int32 Z = MinVoxel.Z; Z <= MaxVoxel.Z; ++Z)
    for (int32 Y = MinVoxel.Y; Y <= MaxVoxel.Y; ++Y)
    for (int32 X = MinVoxel.X; X <= MaxVoxel.X; ++X)
        SetVoxel(FIntVector(X, Y, Z), Material, Integrity);
    EndBulkEdit();
}

void ADestructibleVoxelWorld::StartRuntimePersistence()
{
    RuntimeEdits.Reset();
    bRecordRuntimeEdits = true;
}

void ADestructibleVoxelWorld::ExportRuntimeEdits(TArray<FDeadbrickVoxelEditRecord>& OutEdits) const
{
    OutEdits.Reset();
    OutEdits.Reserve(RuntimeEdits.Num());
    for (const TPair<FIntVector, FDeadbrickVoxel>& Pair : RuntimeEdits)
    {
        FDeadbrickVoxelEditRecord Record;
        Record.Coord = Pair.Key;
        Record.Voxel = Pair.Value;
        OutEdits.Add(Record);
    }
}

void ADestructibleVoxelWorld::ApplyRuntimeEdits(const TArray<FDeadbrickVoxelEditRecord>& Edits)
{
    const bool bWasRecording = bRecordRuntimeEdits;
    bRecordRuntimeEdits = false;
    BeginBulkEdit();
    for (const FDeadbrickVoxelEditRecord& Record : Edits)
        SetVoxel(Record.Coord, Record.Voxel.Material, Record.Voxel.Integrity);
    EndBulkEdit();

    RuntimeEdits.Reset();
    for (const FDeadbrickVoxelEditRecord& Record : Edits) RuntimeEdits.Add(Record.Coord, Record.Voxel);
    bRecordRuntimeEdits = bWasRecording || true;
}

void ADestructibleVoxelWorld::BeginBulkEdit() { ++BulkEditDepth; }

void ADestructibleVoxelWorld::EndBulkEdit()
{
    BulkEditDepth = FMath::Max(0, BulkEditDepth - 1);
    if (BulkEditDepth == 0)
    {
        const TArray<FIntVector> Pending = DirtyChunks.Array();
        DirtyChunks.Reset();
        for (const FIntVector& ChunkCoord : Pending) RebuildChunk(ChunkCoord);
    }
}

void ADestructibleVoxelWorld::MarkDirty(const FIntVector& ChunkCoord)
{
    DirtyChunks.Add(ChunkCoord);
    if (BulkEditDepth == 0)
    {
        RebuildChunk(ChunkCoord);
        DirtyChunks.Remove(ChunkCoord);
    }
}

int32 ADestructibleVoxelWorld::ApplySphereDamage(const FVector& WorldCenter, float RadiusCm, float Damage)
{
    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RadiusVoxels = FMath::CeilToInt(RadiusCm / VoxelSizeCm);
    int32 Destroyed = 0;
    TMap<EDeadbrickVoxelMaterial, int32> DestroyedByMaterial;

    BeginBulkEdit();
    for (int32 Z = -RadiusVoxels; Z <= RadiusVoxels; ++Z)
    for (int32 Y = -RadiusVoxels; Y <= RadiusVoxels; ++Y)
    for (int32 X = -RadiusVoxels; X <= RadiusVoxels; ++X)
    {
        const FIntVector Coord = Center + FIntVector(X, Y, Z);
        const FVector Delta((float)X, (float)Y, (float)Z);
        if (Delta.SizeSquared() > FMath::Square((float)RadiusVoxels)) continue;

        FDeadbrickVoxel Existing;
        if (!GetVoxel(Coord, Existing)) continue;

        const float Falloff = 1.0f - FMath::Clamp(Delta.Size() / FMath::Max(1.0f, (float)RadiusVoxels), 0.0f, 1.0f);
        const int32 EffectiveDamage = FMath::RoundToInt(Damage * (0.35f + 0.65f * Falloff));
        if (EffectiveDamage >= Existing.Integrity)
        {
            DestroyedByMaterial.FindOrAdd(Existing.Material) += 1;
            SetVoxel(Coord, EDeadbrickVoxelMaterial::Air, 0);
            ++Destroyed;
        }
        else
        {
            SetVoxel(Coord, Existing.Material, (uint8)(Existing.Integrity - EffectiveDamage));
        }
    }
    EndBulkEdit();

    if (Destroyed > 0 && bSpawnSalvageDrops) SpawnSalvageDrops(WorldCenter, DestroyedByMaterial);
    if (Destroyed > 0 && bEnableStructuralGravity)
        ResolveStructuralGravityNear(WorldCenter, FMath::Max(RadiusCm * 4.0f, VoxelSizeCm * 8.0f));
    return Destroyed;
}

void ADestructibleVoxelWorld::SpawnSalvageDrops(const FVector& WorldCenter, const TMap<EDeadbrickVoxelMaterial, int32>& DestroyedByMaterial)
{
    if (!GetWorld()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const TPair<EDeadbrickVoxelMaterial, int32>& Pair : DestroyedByMaterial)
    {
        if (Pair.Key == EDeadbrickVoxelMaterial::Air || Pair.Value <= 0) continue;
        const int32 Quantity = FMath::Max(1, FMath::CeilToInt(Pair.Value / 3.0f));
        const FVector Offset(FMath::FRandRange(-18.0f, 18.0f), FMath::FRandRange(-18.0f, 18.0f), FMath::FRandRange(20.0f, 55.0f));
        if (ADeadbrickPickupItem* Pickup = GetWorld()->SpawnActor<ADeadbrickPickupItem>(
            ADeadbrickPickupItem::StaticClass(), WorldCenter + Offset, FRotator::ZeroRotator, SpawnParams))
        {
            Pickup->InitializeFromVoxelMaterial(Pair.Key, Quantity);
        }
    }
}

void ADestructibleVoxelWorld::ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm)
{
    if (!GetWorld() || MaxStructuralScanVoxels <= 0 || MaxPhysicsIslandVoxels <= 0) return;

    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RadiusV = FMath::Clamp(FMath::CeilToInt(RadiusCm / VoxelSizeCm), 2, 64);
    const FIntVector Directions[6] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };

    TSet<FIntVector> Visited;
    const int32 CandidateRadiusSq = RadiusV * RadiusV;
    for (int32 Z = -RadiusV; Z <= RadiusV; ++Z)
    for (int32 Y = -RadiusV; Y <= RadiusV; ++Y)
    for (int32 X = -RadiusV; X <= RadiusV; ++X)
    {
        if (X * X + Y * Y + Z * Z > CandidateRadiusSq) continue;
        const FIntVector Start = Center + FIntVector(X, Y, Z);
        if (Visited.Contains(Start)) continue;

        FDeadbrickVoxel StartVoxel;
        if (!GetVoxel(Start, StartVoxel)) continue;

        TArray<FIntVector> Queue;
        TArray<FIntVector> Component;
        Queue.Reserve(FMath::Min(MaxStructuralScanVoxels, 1024));
        Component.Reserve(FMath::Min(MaxPhysicsIslandVoxels, 512));
        Queue.Add(Start);
        Visited.Add(Start);
        int32 ReadIndex = 0;
        bool bGrounded = false;
        bool bTooLarge = false;

        while (ReadIndex < Queue.Num())
        {
            const FIntVector Current = Queue[ReadIndex++];
            FDeadbrickVoxel CurrentVoxel;
            if (!GetVoxel(Current, CurrentVoxel)) continue;
            Component.Add(Current);

            if (Current.Z <= 0 || CurrentVoxel.Material == EDeadbrickVoxelMaterial::Soil || CurrentVoxel.Material == EDeadbrickVoxelMaterial::Asphalt)
            {
                bGrounded = true;
                break;
            }
            if (Component.Num() >= MaxStructuralScanVoxels)
            {
                bTooLarge = true;
                break;
            }

            for (const FIntVector& Direction : Directions)
            {
                const FIntVector Neighbor = Current + Direction;
                if (Visited.Contains(Neighbor)) continue;
                FDeadbrickVoxel NeighborVoxel;
                if (!GetVoxel(Neighbor, NeighborVoxel)) continue;
                Visited.Add(Neighbor);
                Queue.Add(Neighbor);
            }
        }

        if (bGrounded || bTooLarge || Component.Num() == 0 || Component.Num() > MaxPhysicsIslandVoxels) continue;

        BeginBulkEdit();
        for (const FIntVector& Cell : Component) SetVoxel(Cell, EDeadbrickVoxelMaterial::Air, 0);
        EndBulkEdit();

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AVoxelPhysicsIsland* Island = GetWorld()->SpawnActor<AVoxelPhysicsIsland>(
            AVoxelPhysicsIsland::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
        {
            Island->InitializeFromVoxels(this, Component);
        }
    }
}

UProceduralMeshComponent* ADestructibleVoxelWorld::FindOrCreateChunkMesh(const FIntVector& ChunkCoord)
{
    if (TObjectPtr<UProceduralMeshComponent>* Found = ChunkMeshes.Find(ChunkCoord)) return Found->Get();

    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Mesh->SetRelativeLocation(FVector((double)ChunkCoord.X, (double)ChunkCoord.Y, (double)ChunkCoord.Z) * (ChunkSize * VoxelSizeCm));
    Mesh->bUseComplexAsSimpleCollision = true;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetCanEverAffectNavigation(true);
    ChunkMeshes.Add(ChunkCoord, Mesh);
    return Mesh;
}

void ADestructibleVoxelWorld::RebuildChunk(const FIntVector& ChunkCoord)
{
    FDeadbrickVoxelChunk* Chunk = Chunks.Find(ChunkCoord);
    UProceduralMeshComponent* Mesh = FindOrCreateChunkMesh(ChunkCoord);
    Mesh->ClearAllMeshSections();
    if (!Chunk || Chunk->Voxels.Num() == 0) return;

    TArray<FDeadbrickMeshBuffers> Buffers;
    Buffers.SetNum(8);

    const FIntVector Directions[6] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };
    const FVector FaceNormals[6] = {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0),
        FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)
    };

    auto AddFace = [&](FDeadbrickMeshBuffers& Buffer, const FVector& CenterPos, int32 Face)
    {
        const FVector N = FaceNormals[Face];
        const FVector AxisA = FMath::Abs(N.Z) > 0.5f ? FVector(1,0,0) : FVector(0,0,1);
        const FVector AxisB = FVector::CrossProduct(N, AxisA);
        const float H = VoxelSizeCm * 0.5f;
        const FVector FaceCenter = CenterPos + N * H;
        const int32 Base = Buffer.Vertices.Num();

        Buffer.Vertices.Add(FaceCenter + (-AxisA - AxisB) * H);
        Buffer.Vertices.Add(FaceCenter + ( AxisA - AxisB) * H);
        Buffer.Vertices.Add(FaceCenter + ( AxisA + AxisB) * H);
        Buffer.Vertices.Add(FaceCenter + (-AxisA + AxisB) * H);
        Buffer.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
        for (int32 I = 0; I < 4; ++I)
        {
            Buffer.Normals.Add(N);
            Buffer.Colors.Add(FLinearColor::White);
            Buffer.Tangents.Add(FProcMeshTangent(AxisA, false));
        }
        Buffer.UVs.Append({FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1)});
    };

    for (int32 Z = 0; Z < ChunkSize; ++Z)
    for (int32 Y = 0; Y < ChunkSize; ++Y)
    for (int32 X = 0; X < ChunkSize; ++X)
    {
        const FIntVector Local(X,Y,Z);
        const FDeadbrickVoxel& Cell = Chunk->Voxels[ToIndex(Local)];
        if (!Cell.IsSolid()) continue;

        const int32 MaterialIndex = (int32)Cell.Material;
        if (!Buffers.IsValidIndex(MaterialIndex)) continue;
        FDeadbrickMeshBuffers& Buffer = Buffers[MaterialIndex];

        const FIntVector Global = ChunkCoord * ChunkSize + Local;
        const FVector CenterPos((X + 0.5f) * VoxelSizeCm, (Y + 0.5f) * VoxelSizeCm, (Z + 0.5f) * VoxelSizeCm);
        for (int32 Face = 0; Face < 6; ++Face)
        {
            FDeadbrickVoxel Neighbor;
            if (!GetVoxel(Global + Directions[Face], Neighbor)) AddFace(Buffer, CenterPos, Face);
        }
    }

    int32 SectionIndex = 0;
    for (int32 MaterialIndex = 1; MaterialIndex < Buffers.Num(); ++MaterialIndex)
    {
        FDeadbrickMeshBuffers& Buffer = Buffers[MaterialIndex];
        if (Buffer.Vertices.Num() == 0) continue;

        Mesh->CreateMeshSection_LinearColor(
            SectionIndex,
            Buffer.Vertices,
            Buffer.Triangles,
            Buffer.Normals,
            Buffer.UVs,
            Buffer.Colors,
            Buffer.Tangents,
            true,
            false);

        const EDeadbrickVoxelMaterial Material = (EDeadbrickVoxelMaterial)MaterialIndex;
        if (UMaterialInterface* SurfaceMaterial = ResolveSurfaceMaterial(Material)) Mesh->SetMaterial(SectionIndex, SurfaceMaterial);
        ++SectionIndex;
    }
}
