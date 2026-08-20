#include "Physics/DeadbrickRubbleInteractionSubsystem.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "World/VoxelPhysicsIsland.h"

bool UDeadbrickRubbleInteractionSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickRubbleInteractionSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) return;

    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
    if (!Player) return;

    FVector HorizontalVelocity = Player->GetVelocity();
    HorizontalVelocity.Z = 0.0f;
    const float Speed = HorizontalVelocity.Size();
    if (Speed < 80.0f) return;

    const FVector PlayerLocation = Player->GetActorLocation();
    const float PushRadiusSquared = FMath::Square(125.0f);
    const FVector PushDirection = HorizontalVelocity.GetSafeNormal();
    const float ImpulsePerFrame = FMath::Clamp(Speed * 0.42f * FMath::Max(DeltaTime, 1.0f / 120.0f), 2.0f, 14.0f);

    for (TActorIterator<AVoxelPhysicsIsland> It(World); It; ++It)
    {
        AVoxelPhysicsIsland* Island = *It;
        if (!Island) continue;
        if (FVector::DistSquared(PlayerLocation, Island->GetActorLocation()) > PushRadiusSquared) continue;

        Island->PushFromGameplay(PushDirection * ImpulsePerFrame * 120.0f);
    }
}

TStatId UDeadbrickRubbleInteractionSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDeadbrickRubbleInteractionSubsystem, STATGROUP_Tickables);
}
