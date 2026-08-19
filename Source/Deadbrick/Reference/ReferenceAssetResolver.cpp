#include "Reference/ReferenceAssetResolver.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    bool bRegistryScanned = false;
    bool bFilesystemIndexed = false;
    TArray<FString> FilesystemObjectPaths;

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

        if (Haystack.Contains(TEXT("layoftheland"))) Score += 300;
        if (Haystack.Contains(TEXT("lotl"))) Score += 220;
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

    void EnsureFilesystemIndex()
    {
        if (bFilesystemIndexed) return;
        bFilesystemIndexed = true;
        FilesystemObjectPaths.Reset();

        const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
        TArray<FString> Files;
        IFileManager::Get().FindFilesRecursive(Files, *ContentDir, TEXT("*.uasset"), true, false, false);

        FilesystemObjectPaths.Reserve(Files.Num());
        for (FString Filename : Files)
        {
            Filename = FPaths::ConvertRelativePathToFull(Filename);
            FString Relative = Filename;
            if (!FPaths::MakePathRelativeTo(Relative, *ContentDir)) continue;

            Relative = FPaths::ChangeExtension(Relative, TEXT(""));
            Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
            while (Relative.StartsWith(TEXT("/"))) Relative.RightChopInline(1);
            if (Relative.IsEmpty()) continue;

            const FString AssetName = FPaths::GetBaseFilename(Relative);
            if (AssetName.IsEmpty()) continue;
            FilesystemObjectPaths.Add(FString::Printf(TEXT("/Game/%s.%s"), *Relative, *AssetName));
        }

        UE_LOG(LogTemp, Display, TEXT("DEADBRICK cooked filesystem index: %d package candidates under Content"), FilesystemObjectPaths.Num());
    }

    template <typename TObjectType>
    TArray<TObjectType*> LoadFilesystemAssets(const TArray<FString>& Keywords, int32 MaxResults)
    {
        EnsureFilesystemIndex();

        TArray<FString> Ranked = FilesystemObjectPaths;
        Ranked.Sort([&](const FString& A, const FString& B)
        {
            const int32 ScoreA = ScoreString(A, Keywords);
            const int32 ScoreB = ScoreString(B, Keywords);
            if (ScoreA == ScoreB) return A < B;
            return ScoreA > ScoreB;
        });

        TArray<TObjectType*> Result;
        int32 Attempted = 0;
        const int32 MaxAttempts = FMath::Max(MaxResults * 40, 120);
        for (const FString& ObjectPath : Ranked)
        {
            if (ScoreString(ObjectPath, Keywords) < 0) continue;
            if (++Attempted > MaxAttempts) break;

            UObject* LoadedObject = StaticLoadObject(TObjectType::StaticClass(), nullptr, *ObjectPath);
            if (TObjectType* Loaded = Cast<TObjectType>(LoadedObject))
            {
                Result.AddUnique(Loaded);
                UE_LOG(LogTemp, Display, TEXT("DEADBRICK direct cooked load: %s"), *ObjectPath);
                if (Result.Num() >= MaxResults) break;
            }
        }
        return Result;
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
                if (Result.Num() >= MaxResults) return Result;
            }
        }

        for (TObjectType* Loaded : LoadFilesystemAssets<TObjectType>(Keywords, MaxResults))
        {
            Result.AddUnique(Loaded);
            if (Result.Num() >= MaxResults) break;
        }
        return Result;
    }
}

bool DeadbrickReferenceAssets::HasCookedReferenceAssets()
{
    EnsureFilesystemIndex();

    if (FilesystemObjectPaths.Num() > 100) return true;

    TArray<FAssetData> SkeletalMeshes;
    GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), SkeletalMeshes);
    for (const FAssetData& Asset : SkeletalMeshes)
    {
        const FString Path = Asset.PackageName.ToString().ToLower();
        if (!Path.Contains(TEXT("deadbrick"))) return true;
    }

    TArray<FAssetData> StaticMeshes;
    GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), StaticMeshes);
    for (const FAssetData& Asset : StaticMeshes)
    {
        const FString Path = Asset.PackageName.ToString().ToLower();
        if (!Path.Contains(TEXT("deadbrick"))) return true;
    }
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

    for (UAnimSequence* Sequence : LoadFilesystemAssets<UAnimSequence>(PreferredKeywords, 160))
    {
        if (!Sequence || Sequence->GetSkeleton() != Skeleton) continue;
        if (OutObjectPath) *OutObjectPath = Sequence->GetPathName();
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK direct reference animation: %s"), *Sequence->GetPathName());
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
