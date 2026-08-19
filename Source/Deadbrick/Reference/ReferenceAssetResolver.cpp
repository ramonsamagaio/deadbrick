#include "Reference/ReferenceAssetResolver.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Modules/ModuleManager.h"

namespace
{
    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& PreferredKeywords)
    {
        const FString Haystack = (Asset.AssetName.ToString() + TEXT(" ") + Asset.PackageName.ToString()).ToLower();
        int32 Score = 0;

        for (int32 Index = 0; Index < PreferredKeywords.Num(); ++Index)
        {
            const FString Keyword = PreferredKeywords[Index].ToLower();
            if (!Keyword.IsEmpty() && Haystack.Contains(Keyword))
            {
                Score += 1000 - Index * 25;
            }
        }

        // Prefer supplied /Game content over anything generated for DEADBRICK itself.
        if (Haystack.Contains(TEXT("layoftheland"))) Score += 200;
        if (Haystack.Contains(TEXT("deadbrick"))) Score -= 300;
        if (Haystack.Contains(TEXT("preview"))) Score -= 80;
        if (Haystack.Contains(TEXT("lod"))) Score -= 30;
        return Score;
    }

    void GetAssetsByClass(const FTopLevelAssetPath& ClassPath, TArray<FAssetData>& OutAssets)
    {
        FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.ClassPaths.Add(ClassPath);
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
        RegistryModule.Get().GetAssets(Filter, OutAssets, true);
    }
}

bool DeadbrickReferenceAssets::HasCookedReferenceAssets()
{
    TArray<FAssetData> SkeletalMeshes;
    GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), SkeletalMeshes);

    for (const FAssetData& Asset : SkeletalMeshes)
    {
        const FString Path = Asset.PackageName.ToString().ToLower();
        if (!Path.Contains(TEXT("deadbrick")))
        {
            return true;
        }
    }
    return false;
}

USkeletalMesh* DeadbrickReferenceAssets::FindSkeletalMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    TArray<FAssetData> Assets;
    GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Assets);
    Assets.Sort([&](const FAssetData& A, const FAssetData& B)
    {
        return ScoreAsset(A, PreferredKeywords) > ScoreAsset(B, PreferredKeywords);
    });

    for (const FAssetData& Asset : Assets)
    {
        if (ScoreAsset(Asset, PreferredKeywords) < 0) continue;
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
        {
            if (OutObjectPath) *OutObjectPath = Asset.GetSoftObjectPath().ToString();
            UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference skeletal mesh: %s"), *Asset.GetSoftObjectPath().ToString());
            return Mesh;
        }
    }
    return nullptr;
}

UAnimSequence* DeadbrickReferenceAssets::FindAnimationForSkeleton(USkeleton* Skeleton, const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    if (!Skeleton) return nullptr;

    TArray<FAssetData> Assets;
    GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
    Assets.Sort([&](const FAssetData& A, const FAssetData& B)
    {
        return ScoreAsset(A, PreferredKeywords) > ScoreAsset(B, PreferredKeywords);
    });

    for (const FAssetData& Asset : Assets)
    {
        UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
        if (!Sequence || Sequence->GetSkeleton() != Skeleton) continue;

        if (OutObjectPath) *OutObjectPath = Asset.GetSoftObjectPath().ToString();
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference animation: %s"), *Asset.GetSoftObjectPath().ToString());
        return Sequence;
    }
    return nullptr;
}

UStaticMesh* DeadbrickReferenceAssets::FindStaticMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    TArray<FAssetData> Assets;
    GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), Assets);
    Assets.Sort([&](const FAssetData& A, const FAssetData& B)
    {
        return ScoreAsset(A, PreferredKeywords) > ScoreAsset(B, PreferredKeywords);
    });

    for (const FAssetData& Asset : Assets)
    {
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
        {
            if (OutObjectPath) *OutObjectPath = Asset.GetSoftObjectPath().ToString();
            UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference static mesh: %s"), *Asset.GetSoftObjectPath().ToString());
            return Mesh;
        }
    }
    return nullptr;
}
