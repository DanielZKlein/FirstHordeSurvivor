#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UpgradeEffect.h"
#include "UpgradeDataAsset.generated.h"

class UWeaponDataBase;

/**
 * Data asset defining a single upgrade that can be offered to the player.
 * Supports multiple effects per upgrade (allowing mixed bonuses and penalties),
 * weapon targeting (global vs per-weapon), and selection rules.
 *
 * Create individual assets for each upgrade, then register them via DataTable.
 */
UCLASS(BlueprintType)
class FIRSTHORDESURVIVOR_API UUpgradeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ===== Identity =====

	// Unique identifier for this upgrade (used for tracking stacks and prerequisites)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FName UpgradeID;

	// Display name shown in UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	// Description text shown in UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FText Description;

	// Optional icon for UI display
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UTexture2D> Icon;

	// ===== Effects =====

	// All effects this upgrade applies (can have multiple - supports mixed bonuses/penalties)
	// Example: Glass Cannon could have [+30% Damage, -20% MaxHealth]
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<FUpgradeEffect> Effects;

	// ===== New Weapon (if this upgrade grants a weapon) =====

	// For NewWeapon type: the weapon to grant when this upgrade is selected
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "New Weapon")
	TObjectPtr<UWeaponDataBase> WeaponToGrant;

	// ===== Weapon Targeting =====

	// For per-weapon upgrades: which weapon this upgrade applies to.
	// If null, weapon stat effects apply globally (all weapons).
	// If set, weapon stat effects only apply to this specific weapon.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<UWeaponDataBase> TargetWeapon;

	// If true, this upgrade only appears if player owns TargetWeapon
	// Useful for per-weapon upgrades that shouldn't appear before you have the weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (EditCondition = "TargetWeapon != nullptr"))
	bool bRequiresWeaponOwnership = false;

	// ===== Selection Rules =====

	// Weight for random selection (higher = more likely to appear)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1"))
	int32 Weight = 100;

	// Maximum times this upgrade can be chosen (per run)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1"))
	int32 MaxStacks = 5;

	// Minimum player level required for this upgrade to appear
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1"))
	int32 MinLevel = 1;

	// Other upgrade IDs that must be acquired first (prerequisites)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection")
	TArray<FName> RequiredUpgrades;

	// ===== Helper Functions =====

	// Returns true if this upgrade grants a new weapon
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	bool IsNewWeaponUpgrade() const;

	// Returns true if this is a per-weapon upgrade (has TargetWeapon set)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	bool IsPerWeaponUpgrade() const;

	// Returns true if this upgrade has any weapon stat effects
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	bool HasWeaponStatEffects() const;

	// Returns the effective icon (falls back to weapon icon if upgrade has none)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	UTexture2D* GetEffectiveIcon() const;

	// Returns a combined description of all effects for UI display
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	FText GetCombinedEffectsDescription() const;

	// Get all weapon stats affected by this upgrade
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	TArray<EWeaponStat> GetAffectedWeaponStats() const;
};
