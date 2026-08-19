#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;
class USkeleton;
class UStaticMesh;

namespace DeadbrickReferenceAssets
{
    DEADBRICK_API bool HasCookedReferenceAssets();
    DEADBRICK_API USkeletalMesh* FindSkeletalMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath = nullptr);
    DEADBRICK_API TArray<USkeletalMesh*> FindSkeletalMeshes(const TArray<FString>& PreferredKeywords, int32 MaxResults = 8);
    DEADBRICK_API UAnimSequence* FindAnimationForSkeleton(USkeleton* Skeleton, const TArray<FString>& PreferredKeywords, FString* OutObjectPath = nullptr);
    DEADBRICK_API UStaticMesh* FindStaticMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath = nullptr);
    DEADBRICK_API TArray<UStaticMesh*> FindStaticMeshes(const TArray<FString>& PreferredKeywords, int32 MaxResults = 12);
}
