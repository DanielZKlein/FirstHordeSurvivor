#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "XPGem.h"
#include "XPGemSubsystem.generated.h"

class UXPGemVisualConfig;

/**
 * Subsystem to manage XP Gem pooling and spawning
 */
UCLASS()
class FIRSTHORDESURVIVOR_API UXPGemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Spawns a gem at location with value
    UFUNCTION(BlueprintCallable, Category = "XP Gems")
    void SpawnGem(FVector Location, int32 Value);

    // Returns a gem to the pool
    void ReturnGem(AXPGem* Gem);

    // Configure the Gem Class to spawn
    UFUNCTION(BlueprintCallable, Category = "XP Gems")
    void RegisterGemClass(TSubclassOf<AXPGem> InGemClass);

    // Configure the visual settings via DataAsset
    UFUNCTION(BlueprintCallable, Category = "XP Gems")
    void RegisterVisualConfig(UXPGemVisualConfig* InConfig);

    // Get visual data for a gem value (uses config or falls back to code defaults)
    FXPGemData GetVisualDataForValue(int32 Value) const;

protected:
    // Creates hardcoded default visuals (fallback when no DataAsset configured)
    void InitializeDefaultVisuals();

    // The pool of inactive gems
    UPROPERTY()
    TArray<AXPGem*> GemPool;

    // Class to spawn
    UPROPERTY()
    TSubclassOf<AXPGem> GemClass;

    // Visual configuration DataAsset
    UPROPERTY()
    UXPGemVisualConfig* VisualConfig;

    // Hardcoded default visuals (used when no DataAsset)
    TMap<int32, FXPGemData> DefaultVisuals;
    FXPGemData DefaultFallback;
};
