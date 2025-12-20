#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "XPGem.h"
#include "XPGemVisualConfig.generated.h"

/**
 * DataAsset to configure visual appearance of XP gems per tier value.
 * Create instances in the Content Browser and assign to GameMode or Subsystem.
 */
UCLASS(BlueprintType)
class FIRSTHORDESURVIVOR_API UXPGemVisualConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// Map of gem value (1, 5, 20, 50, 100) to visual data
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gem Visuals")
	TMap<int32, FXPGemData> GemVisuals;

	// Default visual for any gem value not explicitly defined
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gem Visuals")
	FXPGemData DefaultVisual;

	// Get visual data for a specific gem value, falls back to default if not found
	UFUNCTION(BlueprintCallable, Category = "Gem Visuals")
	FXPGemData GetVisualForValue(int32 Value) const;
};
