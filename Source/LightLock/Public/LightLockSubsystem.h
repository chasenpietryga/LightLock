// Copyright (c) 2025 Chasen Pietryga. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LightLockCore.h"
#include "LightLockSubsystem.generated.h"

UCLASS()
class LIGHTLOCK_API ULightLockSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    bool QueryLighting(FVector Position, FVector Normal, FLinearColor& OutColor, float& OutWeight);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void StoreLighting(FVector Position, FVector Normal, FLinearColor Color, float Weight = 1.0f, bool bIsStatic = false, int32 BounceCount = 1, float Confidence = 1.0f);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void InvalidateRegion(FBox Region);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void InvalidateSphere(FVector Center, float Radius);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void UpdateCamera(FVector CameraPosition, FVector CameraForward, float FOV, float FarPlane, float DeltaTime);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void CullDistantEntries(FVector CameraPosition, float MaxDistance = 50000.0f);
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void FlushCache();
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void ClearDynamicCache();
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void ClearAllCaches();
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    FLightLockStats GetStatistics() const;
    
    UFUNCTION(BlueprintCallable, Category = "LightLock")
    void ResetStatistics();
    
    FLightLockCore* GetCore() const { return Core.Get(); }
    
protected:
    UPROPERTY(EditDefaultsOnly, Category = "LightLock")
    FLightLockConfig Configuration;
    
private:
    TUniquePtr<FLightLockCore> Core;
};
