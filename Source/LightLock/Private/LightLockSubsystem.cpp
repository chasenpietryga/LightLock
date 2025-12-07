// Copyright (c) 2025 Chasen Pietryga. Licensed under MIT License.

#include "LightLockSubsystem.h"

void ULightLockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Core = MakeUnique<FLightLockCore>(Configuration);
    UE_LOG(LogTemp, Log, TEXT("LightLock Subsystem initialized"));
}

void ULightLockSubsystem::Deinitialize()
{
    if (Core.IsValid())
    {
        Core->Flush();
        Core.Reset();
    }
    Super::Deinitialize();
}

bool ULightLockSubsystem::QueryLighting(FVector Position, FVector Normal, FLinearColor& OutColor, float& OutWeight)
{
    if (!Core.IsValid()) return false;
    uint32 Hash = FLightLockHasher::HashWorldSpace(Position, Normal, Configuration.WorldSpacePrecision);
    return Core->Query(Hash, Position, Normal, OutColor, OutWeight);
}

void ULightLockSubsystem::StoreLighting(FVector Position, FVector Normal, FLinearColor Color, float Weight, bool bIsStatic, int32 BounceCount, float Confidence)
{
    if (!Core.IsValid()) return;
    uint32 Hash = FLightLockHasher::HashWorldSpace(Position, Normal, Configuration.WorldSpacePrecision);
    Core->Store(Hash, Color, Weight, Position, Normal, bIsStatic, static_cast<uint8>(BounceCount), Confidence);
}

void ULightLockSubsystem::InvalidateRegion(FBox Region)
{
    if (Core.IsValid()) Core->InvalidateRegion(Region);
}

void ULightLockSubsystem::InvalidateSphere(FVector Center, float Radius)
{
    if (Core.IsValid()) Core->InvalidateSphere(Center, Radius);
}

void ULightLockSubsystem::UpdateCamera(FVector CameraPosition, FVector CameraForward, float FOV, float FarPlane, float DeltaTime)
{
    if (Core.IsValid()) Core->UpdateCamera(CameraPosition, CameraForward, FOV, FarPlane, DeltaTime);
}

void ULightLockSubsystem::CullDistantEntries(FVector CameraPosition, float MaxDistance)
{
    if (Core.IsValid()) Core->CullDistantEntries(CameraPosition, MaxDistance);
}

void ULightLockSubsystem::FlushCache()
{
    if (Core.IsValid()) Core->Flush();
}

void ULightLockSubsystem::ClearDynamicCache()
{
    if (Core.IsValid()) Core->ClearDynamic();
}

void ULightLockSubsystem::ClearAllCaches()
{
    if (Core.IsValid()) Core->ClearAll();
}

FLightLockStats ULightLockSubsystem::GetStatistics() const
{
    if (Core.IsValid()) return Core->GetStats();
    return FLightLockStats();
}

void ULightLockSubsystem::ResetStatistics()
{
    if (Core.IsValid()) Core->ResetStats();
}
