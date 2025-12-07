// Copyright (c) 2025 Chasen Pietryga. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include <unordered_map>
#include <atomic>
#include "LightLockCore.generated.h"

USTRUCT(BlueprintType)
struct FLightLockConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    int32 StaticCapacity = 2097152;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    int32 DynamicCapacity = 524288;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    FString CachePath = TEXT("LightLock/cache.bin");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    float WorldSpacePrecision = 0.01f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    bool bEnableAsyncLoading = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    bool bEnablePredictiveLoading = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    float ConfidenceThreshold = 0.3f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightLock")
    int32 PromotionFrameThreshold = 300;
};

USTRUCT(BlueprintType)
struct FLightLockStats
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 StaticCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 DynamicCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    float HitRate = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 TotalQueries = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 Misses = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 Collisions = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 Promotions = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "LightLock")
    int64 SpatialInvalidations = 0;
};

struct FLightPath
{
    FLinearColor Color;
    float Weight;
    uint8 BounceCount;
    uint8 Flags;
    float Confidence;
    FIntVector PositionValidation;
    FIntVector NormalValidation;
    FVector IncidentDirection;
    float Roughness;
    
    FLightPath();
    static FLightPath Create(const FLinearColor& Color, float Weight, const FVector& Position, const FVector& Normal, uint8 Bounces = 1, float Confidence = 1.0f);
    bool ValidatePosition(const FVector& Position) const;
    bool ValidateNormal(const FVector& Normal) const;
};

class FSpatialGrid
{
public:
    FSpatialGrid();
    ~FSpatialGrid();
    
    void Insert(const FVector& Position, uint32 Hash);
    void Remove(const FVector& Position, uint32 Hash);
    TArray<uint32> QueryRegion(const FBox& Region) const;
    void Clear();
    SIZE_T GetMemoryUsage() const;
    
private:
    static constexpr float CELL_SIZE = 1000.0f;
    uint64 GetCellKey(const FVector& Position) const;
    
    mutable FCriticalSection Mutex;
    std::unordered_map<uint64, TArray<uint32>> Grid;
};

class FLightLockHasher
{
public:
    static uint32 HashWorldSpace(const FVector& Position, const FVector& Normal, float Precision = 0.01f);
    static uint32 HashLightmapSpace(uint32 MeshID, const FVector2D& UV, uint32 LightmapResolution = 1024);
};

class LIGHTLOCK_API FLightLockCore
{
public:
    explicit FLightLockCore(const FLightLockConfig& Config);
    ~FLightLockCore();
    
    bool Query(uint32 Hash, const FVector& Position, const FVector& Normal, FLinearColor& OutColor, float& OutWeight);
    void Store(uint32 Hash, const FLinearColor& Color, float Weight, const FVector& Position, const FVector& Normal, bool bIsStatic, uint8 BounceCount = 1, float Confidence = 1.0f);
    
    void InvalidateRegion(const FBox& Region);
    void InvalidateSphere(const FVector& Center, float Radius);
    
    void UpdateCamera(const FVector& CameraPosition, const FVector& CameraForward, float FOV, float FarPlane, float DeltaTime);
    void CullDistantEntries(const FVector& CameraPosition, float MaxDistance);
    
    void AdvanceFrame();
    void Flush();
    void ClearDynamic();
    void ClearAll();
    
    FLightLockStats GetStats() const;
    void ResetStats();
    
private:
    struct DynamicEntry
    {
        FLightPath Path;
        uint32 LastAccessFrame;
        FVector WorldPosition;
    };
    
    FLightLockConfig Config;
    std::atomic<uint32> CurrentFrame;
    
    std::unordered_map<uint32, FLightPath> StaticCache;
    std::unordered_map<uint32, DynamicEntry> DynamicCache;
    TUniquePtr<FSpatialGrid> SpatialIndex;
    
    mutable FCriticalSection StaticMutex;
    mutable FCriticalSection DynamicMutex;
    
    struct AtomicStats
    {
        std::atomic<uint64> TotalQueries{0};
        std::atomic<uint64> StaticHits{0};
        std::atomic<uint64> DynamicHits{0};
        std::atomic<uint64> Misses{0};
        std::atomic<uint64> Promotions{0};
        std::atomic<uint64> CollisionsDetected{0};
        std::atomic<uint64> SpatialInvalidations{0};
    } Stats;
    
    TMap<uint32, FLinearColor> PreviousColors;
    FVector PrevCameraPos;
    FVector PrevCameraDir;
    
    void Load();
    void LoadSync();
    void Save() const;
    void EvictLowestConfidenceStatic();
    void EvictLRUOrLowConfidenceDynamic();
    void PromoteToStatic(uint32 Hash, const FLightPath& Path);
    FLinearColor ApplyTemporalSmoothing(uint32 Hash, const FLinearColor& NewColor, bool bIsMiss);
};
