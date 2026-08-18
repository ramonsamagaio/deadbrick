#include "AI/ZombieDirectorSubsystem.h"
#include "Engine/World.h"

void UDeadbrickZombieDirectorSubsystem::PruneExpired()
{
    if (!GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    NoiseEvents.RemoveAll([Now](const FDeadbrickNoiseEvent& Event)
    {
        return Event.ExpireAt <= Now;
    });
}

void UDeadbrickZombieDirectorSubsystem::ReportNoise(const FVector& Location, float RadiusCm, float Intensity, float LifetimeSeconds)
{
    if (!GetWorld() || RadiusCm <= 0.0f || Intensity <= 0.0f) return;
    PruneExpired();

    FDeadbrickNoiseEvent Event;
    Event.Location = Location;
    Event.RadiusCm = RadiusCm;
    Event.Intensity = Intensity;
    Event.ExpireAt = GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, LifetimeSeconds);
    NoiseEvents.Add(Event);

    constexpr int32 MaxBufferedNoiseEvents = 128;
    if (NoiseEvents.Num() > MaxBufferedNoiseEvents)
    {
        NoiseEvents.RemoveAt(0, NoiseEvents.Num() - MaxBufferedNoiseEvents, EAllowShrinking::No);
    }
}

bool UDeadbrickZombieDirectorSubsystem::FindStrongestNoise(const FVector& ListenerLocation, float MaxListenRadiusCm, FVector& OutLocation, float& OutScore)
{
    PruneExpired();
    OutScore = 0.0f;
    bool bFound = false;

    for (const FDeadbrickNoiseEvent& Event : NoiseEvents)
    {
        const float EffectiveRadius = FMath::Min(MaxListenRadiusCm, Event.RadiusCm);
        if (EffectiveRadius <= 0.0f) continue;

        const float Distance = FVector::Dist(ListenerLocation, Event.Location);
        if (Distance > EffectiveRadius) continue;

        const float DistanceFactor = 1.0f - FMath::Clamp(Distance / EffectiveRadius, 0.0f, 1.0f);
        const float Score = Event.Intensity * DistanceFactor;
        if (!bFound || Score > OutScore)
        {
            bFound = true;
            OutScore = Score;
            OutLocation = Event.Location;
        }
    }

    return bFound;
}
