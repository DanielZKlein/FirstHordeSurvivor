#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SurvivorGameMode.generated.h"

class UXPGemVisualConfig;
class AXPGem;

/**
 * Game Mode for First Horde Survivor
 * Handles game-wide configuration and subsystem setup
 */
UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASurvivorGameMode();

protected:
	virtual void BeginPlay() override;

	// XP Gem configuration
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "XP Gems")
	UXPGemVisualConfig* XPGemVisualConfig;

	// Optional: Override the gem class to spawn (leave null for default AXPGem)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "XP Gems")
	TSubclassOf<AXPGem> XPGemClass;
};
