// Copyright (c) 2025 Chasen Pietryga. Licensed under MIT License.

#include "LightLockCore.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Serialization/Archive.h"
#include "Async/Async.h"

static constexpr uint32 LIGHTLOCK_MAGIC = 0x4C4C434B;
static constexpr uint32 LIGHTLOCK_VERSION = 4;

FLightPath::FLightPath()
    : Color(FLinearColor::Black)
    , Weight(1.0f)
    , BounceCount(1)
    , Flags(0)
    , Confidence(1.0f)
    , PositionValidation(0, 0, 0)
    , NormalValidation(0, 0, 0)
    , IncidentDirection(FVector::UpVector)
    , Roughness(0.5f)
{}

FLightPath FLightPath::Create(const FLinearColor& InColor, float InWeight, const FVector& Position, const FVector& Normal, uint8 Bounces, float Conf)
{
    FLightPath Result;
    Result.Color = InColor;
    Result.Weight = InWeight;
    Result.BounceCount = Bounces;
    Result.Confidence = Conf;
    Result.PositionValidation = FIntVector(FMath::RoundToInt(Position.X * 0.1f), FMath::RoundToInt(Position.Y * 0.1f), FMath::RoundToInt(Position.Z * 0.1f));
    Result.NormalValidation = FIntVector(FMath::RoundToInt(Normal.X * 1000.0f), FMath::RoundToInt(Normal.Y * 1000.0f), FMath::RoundToInt(Normal.Z * 1000.0f));
    Result.IncidentDirection = -Normal;
    return Result;
}

bool FLightPath::ValidatePosition(const FVector& Position) const
{
    FIntVector Quantized(FMath::RoundToInt(Position.X * 0.1f), FMath::RoundToInt(Position.Y * 0.1f), FMath::RoundToInt(Position.Z * 0.1f));
    return FMath::Abs(PositionValidation.X - Quantized.X) <= 1 && FMath::Abs(PositionValidation.Y - Quantized.Y) <= 1 && FMath::Abs(PositionValidation.Z - Quantized.Z) <= 1;
}

bool FLightPath::ValidateNormal(const FVector& Normal) const
{
    FIntVector Quantized(FMath::RoundToInt(Normal.X * 1000.0f), FMath::RoundToInt(Normal.Y * 1000.0f), FMath::RoundToInt(Normal.Z * 1000.0f));
    return FMath::Abs(NormalValidation.X - Quantized.X) <= 10 && FMath::Abs(NormalValidation.Y - Quantized.Y) <= 10 && FMath::Abs(NormalValidation.Z - Quantized.Z) <= 10;
}

FSpatialGrid::FSpatialGrid()
{
    Grid.reserve(1024);
}

FSpatialGrid::~FSpatialGrid()
{
    Clear();
}

uint64 FSpatialGrid::GetCellKey(const FVector& Position) const
{
    int32 CX = FMath::FloorToInt(Position.X / CELL_SIZE);
    int32 CY = FMath::FloorToInt(Position.Y / CELL_SIZE);
    int32 CZ = FMath::FloorToInt(Position.Z / CELL_SIZE);
    uint64 Key = 0;
    Key |= (static_cast<uint64>(CX & 0x1FFFFF) << 42);
    Key |= (static_cast<uint64>(CY & 0x1FFFFF) << 21);
    Key |= static_cast<uint64>(CZ & 0x1FFFFF);
    return Key;
}

void FSpatialGrid::Insert(const FVector& Position, uint32 Hash)
{
    FScopeLock Lock(&Mutex);
    uint64 Key = GetCellKey(Position);
    Grid[Key].Add(Hash);
}

void FSpatialGrid::Remove(const FVector& Position, uint32 Hash)
{
    FScopeLock Lock(&Mutex);
    uint64 Key = GetCellKey(Position);
    auto It = Grid.find(Key);
    if (It != Grid.end())
    {
        It->second.Remove(Hash);
        if (It->second.Num() == 0)
        {
            Grid.erase(It);
        }
    }
}

TArray<uint32> FSpatialGrid::QueryRegion(const FBox& Region) const
{
    FScopeLock Lock(&Mutex);
    TArray<uint32> Result;
    Result.Reserve(256);
    int32 MinX = FMath::FloorToInt(Region.Min.X / CELL_SIZE);
    int32 MaxX = FMath::FloorToInt(Region.Max.X / CELL_SIZE);
    int32 MinY = FMath::FloorToInt(Region.Min.Y / CELL_SIZE);
    int32 MaxY = FMath::FloorToInt(Region.Max.Y / CELL_SIZE);
    int32 MinZ = FMath::FloorToInt(Region.Min.Z / CELL_SIZE);
    int32 MaxZ = FMath::FloorToInt(Region.Max.Z / CELL_SIZE);
    for (int32 X = MinX; X <= MaxX; ++X)
    {
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 Z = MinZ; Z <= MaxZ; ++Z)
            {
                FVector CellPos(X * CELL_SIZE, Y * CELL_SIZE, Z * CELL_SIZE);
                uint64 Key = GetCellKey(CellPos);
                auto It = Grid.find(Key);
                if (It != Grid.end())
                {
                    Result.Append(It->second);
                }
            }
        }
    }
    return Result;
}

void FSpatialGrid::Clear()
{
    FScopeLock Lock(&Mutex);
    Grid.clear();
}

SIZE_T FSpatialGrid::GetMemoryUsage() const
{
    FScopeLock Lock(&Mutex);
    SIZE_T Total = 0;
    for (const auto& Pair : Grid)
    {
        Total += sizeof(Pair.first) + Pair.second.GetAllocatedSize();
    }
    return Total;
}

uint32 FLightLockHasher::HashWorldSpace(const FVector& Position, const FVector& Normal, float Precision)
{
    int32 QX = FMath::RoundToInt(Position.X / Precision);
    int32 QY = FMath::RoundToInt(Position.Y / Precision);
    int32 QZ = FMath::RoundToInt(Position.Z / Precision);
    int32 QNX = FMath::RoundToInt(Normal.X * 1000.0f);
    int32 QNY = FMath::RoundToInt(Normal.Y * 1000.0f);
    int32 QNZ = FMath::RoundToInt(Normal.Z * 1000.0f);
    uint32 Hash = 2166136261u;
    Hash = (Hash ^ static_cast<uint32>(QX)) * 16777619u;
    Hash = (Hash ^ static_cast<uint32>(QY)) * 16777619u;
    Hash = (Hash ^ static_cast<uint32>(QZ)) * 16777619u;
    Hash = (Hash ^ static_cast<uint32>(QNX)) * 16777619u;
    Hash = (Hash ^ static_cast<uint32>(QNY)) * 16777619u;
    Hash = (Hash ^ static_cast<uint32>(QNZ)) * 16777619u;
    return Hash;
}

uint32 FLightLockHasher::HashLightmapSpace(uint32 MeshID, const FVector2D& UV, uint32 LightmapResolution)
{
    uint32 IU = FMath::RoundToInt(UV.X * LightmapResolution);
    uint32 IV = FMath::RoundToInt(UV.Y * LightmapResolution);
    uint32 Hash = 2166136261u;
    Hash = (Hash ^ MeshID) * 16777619u;
    Hash = (Hash ^ IU) * 16777619u;
    Hash = (Hash ^ IV) * 16777619u;
    return Hash;
}

FLightLockCore::FLightLockCore(const FLightLockConfig& InConfig) : Config(InConfig), CurrentFrame(0)
{
    SpatialIndex = MakeUnique<FSpatialGrid>();
    StaticCache.reserve(Config.StaticCapacity);
    DynamicCache.reserve(Config.DynamicCapacity);
    Load();
    UE_LOG(LogTemp, Log, TEXT("LightLock: Initialized"));
}

FLightLockCore::~FLightLockCore()
{
    Save();
}

bool FLightLockCore::Query(uint32 Hash, const FVector& Position, const FVector& Normal, FLinearColor& OutColor, float& OutWeight)
{
    Stats.TotalQueries++;
    bool bHit = false;
    FLinearColor RawColor = FLinearColor::Black;
    
    {
        FScopeLock Lock(&StaticMutex);
        auto It = StaticCache.find(Hash);
        if (It != StaticCache.end())
        {
            const FLightPath& Path = It->second;
            if (Path.ValidatePosition(Position) && Path.ValidateNormal(Normal))
            {
                RawColor = Path.Color;
                OutWeight = Path.Weight;
                Stats.StaticHits++;
                bHit = true;
            }
            else
            {
                Stats.CollisionsDetected++;
            }
        }
    }
    
    if (!bHit)
    {
        FScopeLock Lock(&DynamicMutex);
        auto It = DynamicCache.find(Hash);
        if (It != DynamicCache.end())
        {
            DynamicEntry& Entry = It->second;
            if (Entry.Path.ValidatePosition(Position) && Entry.Path.ValidateNormal(Normal))
            {
                RawColor = Entry.Path.Color;
                OutWeight = Entry.Path.Weight;
                Entry.LastAccessFrame = CurrentFrame.load();
                Stats.DynamicHits++;
                bHit = true;
                uint32 Age = CurrentFrame.load() - Entry.LastAccessFrame;
                if (Age > static_cast<uint32>(Config.PromotionFrameThreshold))
                {
                    PromoteToStatic(Hash, Entry.Path);
                }
            }
            else
            {
                Stats.CollisionsDetected++;
            }
        }
    }
    
    if (!bHit) Stats.Misses++;
    OutColor = ApplyTemporalSmoothing(Hash, RawColor, !bHit);
    return bHit;
}

void FLightLockCore::Store(uint32 Hash, const FLinearColor& Color, float Weight, const FVector& Position, const FVector& Normal, bool bIsStatic, uint8 BounceCount, float Confidence)
{
    FLightPath Path = FLightPath::Create(Color, Weight, Position, Normal, BounceCount, Confidence);
    if (bIsStatic)
    {
        FScopeLock Lock(&StaticMutex);
        if (StaticCache.size() >= static_cast<size_t>(Config.StaticCapacity))
        {
            EvictLowestConfidenceStatic();
        }
        StaticCache[Hash] = Path;
    }
    else
    {
        FScopeLock Lock(&DynamicMutex);
        if (DynamicCache.size() >= static_cast<size_t>(Config.DynamicCapacity))
        {
            EvictLRUOrLowConfidenceDynamic();
        }
        DynamicEntry Entry;
        Entry.Path = Path;
        Entry.LastAccessFrame = CurrentFrame.load();
        Entry.WorldPosition = Position;
        DynamicCache[Hash] = Entry;
        SpatialIndex->Insert(Position, Hash);
    }
}

void FLightLockCore::InvalidateRegion(const FBox& Region)
{
    TArray<uint32> Affected = SpatialIndex->QueryRegion(Region);
    {
        FScopeLock Lock(&DynamicMutex);
        for (uint32 Hash : Affected)
        {
            auto It = DynamicCache.find(Hash);
            if (It != DynamicCache.end())
            {
                FVector Pos = It->second.WorldPosition;
                SpatialIndex->Remove(Pos, Hash);
                DynamicCache.erase(It);
            }
        }
    }
    Stats.SpatialInvalidations++;
}

void FLightLockCore::InvalidateSphere(const FVector& Center, float Radius)
{
    FBox Region(Center - FVector(Radius), Center + FVector(Radius));
    InvalidateRegion(Region);
}

void FLightLockCore::UpdateCamera(const FVector& CameraPosition, const FVector& CameraForward, float FOV, float FarPlane, float DeltaTime)
{
    if (Config.bEnablePredictiveLoading && DeltaTime > 0.0f)
    {
        FVector Velocity = (CameraPosition - PrevCameraPos) / DeltaTime;
        FVector PredictedPos = CameraPosition + Velocity * 0.5f;
        PrevCameraPos = CameraPosition;
        PrevCameraDir = CameraForward;
    }
}

void FLightLockCore::CullDistantEntries(const FVector& CameraPosition, float MaxDistance)
{
    FScopeLock Lock(&DynamicMutex);
    TArray<uint32> ToRemove;
    ToRemove.Reserve(DynamicCache.size() / 10);
    for (const auto& Pair : DynamicCache)
    {
        float Distance = FVector::Dist(CameraPosition, Pair.second.WorldPosition);
        if (Distance > MaxDistance)
        {
            ToRemove.Add(Pair.first);
        }
    }
    for (uint32 Hash : ToRemove)
    {
        auto It = DynamicCache.find(Hash);
        if (It != DynamicCache.end())
        {
            SpatialIndex->Remove(It->second.WorldPosition, Hash);
            DynamicCache.erase(It);
        }
    }
}

void FLightLockCore::AdvanceFrame() { CurrentFrame++; }
void FLightLockCore::Flush() { Save(); }

void FLightLockCore::ClearDynamic()
{
    { FScopeLock Lock(&DynamicMutex); DynamicCache.clear(); }
    SpatialIndex->Clear();
    PreviousColors.Empty();
}

void FLightLockCore::ClearAll()
{
    { FScopeLock Lock(&StaticMutex); StaticCache.clear(); }
    ClearDynamic();
}

FLightLockStats FLightLockCore::GetStats() const
{
    FLightLockStats Result;
    Result.StaticCount = StaticCache.size();
    Result.DynamicCount = DynamicCache.size();
    Result.TotalQueries = Stats.TotalQueries.load();
    Result.Misses = Stats.Misses.load();
    Result.Collisions = Stats.CollisionsDetected.load();
    Result.Promotions = Stats.Promotions.load();
    Result.SpatialInvalidations = Stats.SpatialInvalidations.load();
    if (Result.TotalQueries > 0)
    {
        uint64 Hits = Stats.StaticHits.load() + Stats.DynamicHits.load();
        Result.HitRate = static_cast<float>(Hits) / static_cast<float>(Result.TotalQueries);
    }
    return Result;
}

void FLightLockCore::ResetStats()
{
    Stats.TotalQueries = 0;
    Stats.StaticHits = 0;
    Stats.DynamicHits = 0;
    Stats.Misses = 0;
    Stats.Promotions = 0;
    Stats.CollisionsDetected = 0;
    Stats.SpatialInvalidations = 0;
}

void FLightLockCore::Load()
{
    if (Config.bEnableAsyncLoading)
    {
        Async(EAsyncExecution::ThreadPool, [this]() { LoadSync(); });
    }
    else
    {
        LoadSync();
    }
}

void FLightLockCore::LoadSync()
{
    FString FullPath = FPaths::ProjectSavedDir() / Config.CachePath;
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FullPath));
    if (!Reader) return;
    
    uint32 Magic, Version, Count;
    *Reader << Magic << Version << Count;
    if (Magic != LIGHTLOCK_MAGIC || Version != LIGHTLOCK_VERSION) return;
    
    FScopeLock Lock(&StaticMutex);
    StaticCache.reserve(FMath::Min(Count, static_cast<uint32>(Config.StaticCapacity)));
    
    for (uint32 i = 0; i < Count && StaticCache.size() < static_cast<size_t>(Config.StaticCapacity); ++i)
    {
        uint32 Hash;
        FLightPath Path;
        *Reader << Hash;
        *Reader << Path.Color.R << Path.Color.G << Path.Color.B << Path.Color.A;
        *Reader << Path.Weight << Path.BounceCount << Path.Flags << Path.Confidence;
        *Reader << Path.PositionValidation.X << Path.PositionValidation.Y << Path.PositionValidation.Z;
        *Reader << Path.NormalValidation.X << Path.NormalValidation.Y << Path.NormalValidation.Z;
        *Reader << Path.IncidentDirection.X << Path.IncidentDirection.Y << Path.IncidentDirection.Z;
        *Reader << Path.Roughness;
        if (!Reader->IsError())
        {
            StaticCache[Hash] = Path;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("LightLock: Loaded %zu entries"), StaticCache.size());
}

void FLightLockCore::Save() const
{
    FString FullPath = FPaths::ProjectSavedDir() / Config.CachePath;
    FString Directory = FPaths::GetPath(FullPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectoryTree(*Directory);
    }
    
    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FullPath));
    if (!Writer) return;
    
    FScopeLock Lock(&StaticMutex);
    uint32 Magic = LIGHTLOCK_MAGIC;
    uint32 Version = LIGHTLOCK_VERSION;
    uint32 Count = StaticCache.size();
    *Writer << Magic << Version << Count;
    
    for (const auto& Pair : StaticCache)
    {
        uint32 Hash = Pair.first;
        const FLightPath& Path = Pair.second;
        *Writer << Hash;
        *Writer << Path.Color.R << Path.Color.G << Path.Color.B << Path.Color.A;
        *Writer << Path.Weight << Path.BounceCount << Path.Flags << Path.Confidence;
        *Writer << Path.PositionValidation.X << Path.PositionValidation.Y << Path.PositionValidation.Z;
        *Writer << Path.NormalValidation.X << Path.NormalValidation.Y << Path.NormalValidation.Z;
        *Writer << Path.IncidentDirection.X << Path.IncidentDirection.Y << Path.IncidentDirection.Z;
        *Writer << Path.Roughness;
    }
    UE_LOG(LogTemp, Log, TEXT("LightLock: Saved %u entries"), Count);
}

void FLightLockCore::EvictLowestConfidenceStatic()
{
    if (StaticCache.empty()) return;
    auto WorstIt = StaticCache.begin();
    float LowestConfidence = 1.0f;
    for (auto It = StaticCache.begin(); It != StaticCache.end(); ++It)
    {
        if (It->second.Confidence < LowestConfidence)
        {
            LowestConfidence = It->second.Confidence;
            WorstIt = It;
        }
    }
    StaticCache.erase(WorstIt);
}

void FLightLockCore::EvictLRUOrLowConfidenceDynamic()
{
    if (DynamicCache.empty()) return;
    auto WorstIt = DynamicCache.begin();
    float WorstScore = FLT_MAX;
    uint32 CurrentFrameVal = CurrentFrame.load();
    for (auto It = DynamicCache.begin(); It != DynamicCache.end(); ++It)
    {
        uint32 Age = CurrentFrameVal - It->second.LastAccessFrame;
        float Recency = 1.0f / (1.0f + Age);
        float Score = Recency * It->second.Path.Confidence;
        if (Score < WorstScore)
        {
            WorstScore = Score;
            WorstIt = It;
        }
    }
    FVector Pos = WorstIt->second.WorldPosition;
    uint32 Hash = WorstIt->first;
    SpatialIndex->Remove(Pos, Hash);
    DynamicCache.erase(WorstIt);
}

void FLightLockCore::PromoteToStatic(uint32 Hash, const FLightPath& Path)
{
    FScopeLock Lock(&StaticMutex);
    if (StaticCache.size() >= static_cast<size_t>(Config.StaticCapacity))
    {
        EvictLowestConfidenceStatic();
    }
    StaticCache[Hash] = Path;
    Stats.Promotions++;
}

FLinearColor FLightLockCore::ApplyTemporalSmoothing(uint32 Hash, const FLinearColor& NewColor, bool bIsMiss)
{
    FLinearColor* PrevColor = PreviousColors.Find(Hash);
    if (bIsMiss && PrevColor) return *PrevColor;
    if (!PrevColor)
    {
        PreviousColors.Add(Hash, NewColor);
        return NewColor;
    }
    FLinearColor Smoothed = FMath::Lerp(*PrevColor, NewColor, 0.1f);
    PreviousColors.Add(Hash, Smoothed);
    return Smoothed;
}
