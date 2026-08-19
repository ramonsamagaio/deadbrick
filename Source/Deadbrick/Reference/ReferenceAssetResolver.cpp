#include "Reference/ReferenceAssetResolver.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"

namespace
{
    bool bRegistryScanned = false;

    bool IsReferencePath(const FString& Input)
    {
        const FString Path = Input.ToLower();
        return Path.Contains(TEXT("layoftheland")) ||
               Path.Contains(TEXT("lay_of_the_land")) ||
               Path.Contains(TEXT("lotl")) ||
               Path.Contains(TEXT("referenceimported")) ||
               Path.Contains(TEXT("reference_imported"));
    }

    int32 ScoreString(const FString& Input, const TArray<FString>& PreferredKeywords)
    {
        const FString Haystack = Input.ToLower();
        int32 Score = 0;
        bool bMatchedKeyword = PreferredKeywords.Num() == 0;

        for (int32 Index = 0; Index < PreferredKeywords.Num(); ++Index)
        {
            const FString Keyword = PreferredKeywords[Index].ToLower();
            if (!Keyword.IsEmpty() && Haystack.Contains(Keyword))
            {
                bMatchedKeyword = true;
                Score += 1000 - Index * 25;
            }
        }

        if (!bMatchedKeyword) return -100000;
        if (IsReferencePath(Haystack)) Score += 500;
        if (Haystack.Contains(TEXT("deadbrick"))) Score -= 600;
        if (Haystack.Contains(TEXT("preview"))) Score -= 80;
        if (Haystack.Contains(TEXT("lod"))) Score -= 30;
        if (Haystack.Contains(TEXT("editor"))) Score -= 60;
        return Score;
    }

    int32 ScoreAsset(const FAssetData& Asset, const TArray<FString>& PreferredKeywords)
    {
        return ScoreString(Asset.AssetName.ToString() + TEXT(" ") + Asset.PackageName.ToString(), PreferredKeywords);
    }

    bool PackageNameBefore(const FAssetData& A, const FAssetData& B)
    {
        return A.PackageName.ToString() < B.PackageName.ToString();
    }

    void EnsureRegistryScanned()
    {
        if (bRegistryScanned) return;
        bRegistryScanned = true;

        FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        TArray<FString> Paths;
        Paths.Add(TEXT("/Game"));
        RegistryModule.Get().ScanPathsSynchronous(Paths, true, false);
    }

    void GetAssetsByClass(const FTopLevelAssetPath& ClassPath, TArray<FAssetData>& OutAssets)
    {
        EnsureRegistryScanned();
        FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.ClassPaths.Add(ClassPath);
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
        RegistryModule.Get().GetAssets(Filter, OutAssets, true);
    }

    template <typename TObjectType>
    TArray<TObjectType*> LoadRankedAssets(const FTopLevelAssetPath& ClassPath, const TArray<FString>& Keywords, int32 MaxResults)
    {
        TArray<FAssetData> Assets;
        GetAssetsByClass(ClassPath, Assets);
        Assets.Sort([&](const FAssetData& A, const FAssetData& B)
        {
            const int32 ScoreA = ScoreAsset(A, Keywords);
            const int32 ScoreB = ScoreAsset(B, Keywords);
            if (ScoreA == ScoreB) return PackageNameBefore(A, B);
            return ScoreA > ScoreB;
        });

        TArray<TObjectType*> Result;
        for (const FAssetData& Asset : Assets)
        {
            if (ScoreAsset(Asset, Keywords) < 0) continue;
            if (TObjectType* Loaded = Cast<TObjectType>(Asset.GetAsset()))
            {
                Result.AddUnique(Loaded);
                if (Result.Num() >= MaxResults) break;
            }
        }
        return Result;
    }
}

bool DeadbrickReferenceAssets::HasCookedReferenceAssets()
{
    TArray<FAssetData> SkeletalMeshes;
    GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), SkeletalMeshes);
    for (const FAssetData& Asset : SkeletalMeshes)
        if (IsReferencePath(Asset.PackageName.ToString())) return true;

    TArray<FAssetData> StaticMeshes;
    GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), StaticMeshes);
    for (const FAssetData& Asset : StaticMeshes)
        if (IsReferencePath(Asset.PackageName.ToString())) return true;

    return false;
}

TArray<USkeletalMesh*> DeadbrickReferenceAssets::FindSkeletalMeshes(const TArray<FString>& PreferredKeywords, int32 MaxResults)
{
    return LoadRankedAssets<USkeletalMesh>(USkeletalMesh::StaticClass()->GetClassPathName(), PreferredKeywords, FMath::Max(1, MaxResults));
}

USkeletalMesh* DeadbrickReferenceAssets::FindSkeletalMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    TArray<USkeletalMesh*> Assets = FindSkeletalMeshes(PreferredKeywords, 1);
    if (Assets.Num() == 0) return nullptr;
    USkeletalMesh* Mesh = Assets[0];
    if (OutObjectPath) *OutObjectPath = Mesh->GetPathName();
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference skeletal mesh: %s"), *Mesh->GetPathName());
    return Mesh;
}

UAnimSequence* DeadbrickReferenceAssets::FindAnimationForSkeleton(USkeleton* Skeleton, const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    if (!Skeleton) return nullptr;

    TArray<FAssetData> Assets;
    GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
    Assets.Sort([&](const FAssetData& A, const FAssetData& B)
    {
        const int32 ScoreA = ScoreAsset(A, PreferredKeywords);
        const int32 ScoreB = ScoreAsset(B, PreferredKeywords);
        if (ScoreA == ScoreB) return PackageNameBefore(A, B);
        return ScoreA > ScoreB;
    });

    for (const FAssetData& Asset : Assets)
    {
        if (ScoreAsset(Asset, PreferredKeywords) < 0) continue;
        UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
        if (!Sequence || Sequence->GetSkeleton() != Skeleton) continue;

        if (OutObjectPath) *OutObjectPath = Sequence->GetPathName();
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference animation: %s"), *Sequence->GetPathName());
        return Sequence;
    }
    return nullptr;
}

TArray<UStaticMesh*> DeadbrickReferenceAssets::FindStaticMeshes(const TArray<FString>& PreferredKeywords, int32 MaxResults)
{
    return LoadRankedAssets<UStaticMesh>(UStaticMesh::StaticClass()->GetClassPathName(), PreferredKeywords, FMath::Max(1, MaxResults));
}

UStaticMesh* DeadbrickReferenceAssets::FindStaticMesh(const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    TArray<UStaticMesh*> Assets = FindStaticMeshes(PreferredKeywords, 1);
    if (Assets.Num() == 0) return nullptr;
    UStaticMesh* Mesh = Assets[0];
    if (OutObjectPath) *OutObjectPath = Mesh->GetPathName();
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference static mesh: %s"), *Mesh->GetPathName());
    return Mesh;
}

UMaterialInterface* DeadbrickReferenceAssets::FindMaterial(const TArray<FString>& PreferredKeywords, FString* OutObjectPath)
{
    TArray<UMaterialInterface*> Materials = LoadRankedAssets<UMaterialInterface>(
        UMaterialInterface::StaticClass()->GetClassPathName(), PreferredKeywords, 1);
    if (Materials.Num() == 0) return nullptr;

    UMaterialInterface* Material = Materials[0];
    if (OutObjectPath) *OutObjectPath = Material->GetPathName();
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference material: %s"), *Material->GetPathName());
    return Material;
}
