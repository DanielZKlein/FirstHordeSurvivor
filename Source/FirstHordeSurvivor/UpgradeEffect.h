#pragma once

#include "CoreMinimal.h"
#include "UpgradeTypes.h"
#include "UpgradeEffect.generated.h"

/**
 * A single effect within an upgrade.
 * Upgrades can have multiple effects, allowing for mixed bonuses and penalties
 * (e.g., +30% Damage, -20% MaxHealth).
 */
USTRUCT(BlueprintType)
struct FIRSTHORDESURVIVOR_API FUpgradeEffect
{
	GENERATED_BODY()

public:
	// What type of effect this is (PlayerStat or WeaponStat)
	// Note: NewWeapon and WeaponEvolution are handled at the upgrade level, not effect level
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EUpgradeType Type = EUpgradeType::PlayerStat;

	// For PlayerStat type: which attribute to modify
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Type == EUpgradeType::PlayerStat"))
	EPlayerStat PlayerStat = EPlayerStat::MaxHealth;

	// For WeaponStat type: which weapon stat to boost
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Type == EUpgradeType::WeaponStat"))
	EWeaponStat WeaponStat = EWeaponStat::Damage;

	// Flat bonus added to the stat (can be negative for penalties)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float AdditiveBonus = 0.0f;

	// Multiplier applied to the stat (0.8 = -20%, 1.1 = +10%)
	// Only applied if != 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float MultiplicativeBonus = 1.0f;

	FUpgradeEffect()
		: Type(EUpgradeType::PlayerStat)
		, PlayerStat(EPlayerStat::MaxHealth)
		, WeaponStat(EWeaponStat::Damage)
		, AdditiveBonus(0.0f)
		, MultiplicativeBonus(1.0f)
	{}

	// Helper to check if this effect has any actual modification
	bool HasEffect() const
	{
		return AdditiveBonus != 0.0f || MultiplicativeBonus != 1.0f;
	}

	// Helper to get a formatted description of this effect for UI
	FText GetEffectDescription() const;
};
