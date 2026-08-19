#include "World/DestructibleVoxelWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"

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
    if (Chunk.Voxels.Num() != Required)
    {
        Chunk.Voxels.SetNum(Required);
    }
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

void ADestructibleVoxelWorld::SetVoxel(const FIntVector& Voxel, EDeadbrickVoxelMaterial Material, uint8 Integrity)
{
    const FIntVector ChunkCoord = ToChunkCoord(Voxel);
    FDeadbrickVoxelChunk& Chunk = FindOrCreateChunk(ChunkCoord);
    FDeadbrickVoxel& Cell = Chunk.Voxels[ToIndex(ToLocalCoord(Voxel))];
    Cell.Material = Material;
    Cell.Integrity = Material == EDeadbrickVoxelMaterial::Air ? 0 : (Integrity == 255 ? DefaultIntegrityFor(Material) : Integrity);
    MarkDirty(ChunkCoord);
}

void ADestructibleVoxelWorld::FillBox(const FIntVector& MinVoxel, const FIntVector& MaxVoxel, EDeadbrickVoxelMaterial Material, uint8 Integrity)
{
    BeginBulkEdit();
    for (int32 Z = MinVoxel.Z; Z <= MaxVoxel.Z; ++Z)
    for (int32 Y = MinVoxel.Y; Y <= MaxVoxel.Y; ++Y)
    for (int32 X = MinVoxel.X; X <= MaxVoxel.X; ++X)
    {
        SetVoxel(FIntVector(X, Y, Z), Material, Integrity);
    }
    EndBulkEdit();
}

void ADestructibleVoxelWorld::BeginBulkEdit()
{
    ++BulkEditDepth;
}

void ADestructibleVoxelWorld::EndBulkEdit()
{
    BulkEditDepth = FMath::Max(0, BulkEditDepth - 1);
    if (BulkEditDepth == 0)
    {
        const TArray<FIntVector> Pending = DirtyChunks.Array();
        DirtyChunks.Reset();
        for (const FIntVector& ChunkCoord : Pending)
        {
            RebuildChunk(ChunkCoord);
        }
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
            SetVoxel(Coord, EDeadbrickVoxelMaterial::Air, 0);
            ++Destroyed;
        }
        else
        {
            SetVoxel(Coord, Existing.Material, (uint8)(Existing.Integrity - EffectiveDamage));
        }
    }
    EndBulkEdit();
    return Destroyed;
}

UProceduralMeshComponent* ADestructibleVoxelWorld::FindOrCreateChunkMesh(const FIntVector& ChunkCoord)
{
    if (TObjectPtr<UProceduralMeshComponent>* Found = ChunkMeshes.Find(ChunkCoord))
    {
        return Found->Get();
    }

    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Mesh->SetRelativeLocation(FVector((double)ChunkCoord.X, (double)ChunkCoord.Y, (double)ChunkCoord.Z) * (ChunkSize * VoxelSizeCm));
    Mesh->bUseComplexAsSimpleCollision = true;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    ChunkMeshes.Add(ChunkCoord, Mesh);
    return Mesh;
}

void ADestructibleVoxelWorld::RebuildChunk(const FIntVector& ChunkCoord)
{
    FDeadbrickVoxelChunk* Chunk = Chunks.Find(ChunkCoord);
    UProceduralMeshComponent* Mesh = FindOrCreateChunkMesh(ChunkCoord);
    if (!Chunk || Chunk->Voxels.Num() == 0)
    {
        Mesh->ClearAllMeshSections();
        return;
    }

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const FIntVector Directions[6] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };
    const FVector FaceNormals[6] = {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0),
        FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)
    };

    auto AddFace = [&](const FVector& Center, int32 Face)
    {
        const FVector N = FaceNormals[Face];
        const FVector AxisA = FMath::Abs(N.Z) > 0.5f ? FVector(1,0,0) : FVector(0,0,1);
        const FVector AxisB = FVector::CrossProduct(N, AxisA);
        const float H = VoxelSizeCm * 0.5f;
        const FVector FaceCenter = Center + N * H;
        const int32 Base = Vertices.Num();

        Vertices.Add(FaceCenter + (-AxisA - AxisB) * H);
        Vertices.Add(FaceCenter + ( AxisA - AxisB) * H);
        Vertices.Add(FaceCenter + ( AxisA + AxisB) * H);
        Vertices.Add(FaceCenter + (-AxisA + AxisB) * H);

        Triangles.Add(Base);
        Triangles.Add(Base + 1);
        Triangles.Add(Base + 2);
        Triangles.Add(Base);
        Triangles.Add(Base + 2);
        Triangles.Add(Base + 3);

        for (int32 I = 0; I < 4; ++I)
        {
            Normals.Add(N);
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(AxisA, false));
        }

        UVs.Add(FVector2D(0,0));
        UVs.Add(FVector2D(1,0));
        UVs.Add(FVector2D(1,1));
        UVs.Add(FVector2D(0,1));
    };

    for (int32 Z = 0; Z < ChunkSize; ++Z)
    for (int32 Y = 0; Y < ChunkSize; ++Y)
    for (int32 X = 0; X < ChunkSize; ++X)
    {
        const FIntVector Local(X,Y,Z);
        const FDeadbrickVoxel& Cell = Chunk->Voxels[ToIndex(Local)];
        if (!Cell.IsSolid()) continue;

        const FIntVector Global = ChunkCoord * ChunkSize + Local;
        const FVector Center((X + 0.5f) * VoxelSizeCm, (Y + 0.5f) * VoxelSizeCm, (Z + 0.5f) * VoxelSizeCm);
        for (int32 Face = 0; Face < 6; ++Face)
        {
            FDeadbrickVoxel Neighbor;
            if (!GetVoxel(Global + Directions[Face], Neighbor))
            {
                AddFace(Center, Face);
            }
        }
    }

    Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true, false);
}
