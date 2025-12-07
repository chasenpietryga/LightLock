// Copyright (c) 2025 Chasen Pietryga. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LightLockCore.h"
#include "LightLockBlueprintLibrary.generated.h"

UCLASS()
class LIGHTLOCK_API ULightLockBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    UFUNCTION(BlueprintPure, Category = "LightLock", meta = (WorldContext = "WorldContextObject"))
    static class ULightLockSubsystem* GetLightLockSubsystem(const UObject* WorldContextObject);
    
    UFUNCTION(BlueprintPure, Category = "LightLock")
    static FString FormatStatistics(const FLightLockStats& Stats);
};
