#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UpgradeTypes.h"
#include "AttributeComponent.h"
#include "WeaponDataBase.generated.h"

class ASurvivorWeapon;
class UNiagaraSystem;
class USoundBase;

/**
 * Abstract base class for all weapon data assets.
 * Each weapon archetype (Projectile, Aura, Chain, etc.) inherits from this
 * and defines which stats it uses and how they're interpreted.
 */
UCLASS(Abstract, BlueprintType)
class FIRSTHORDESURVIVOR_API UWeaponDataBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ===== Identity =====

	// Unique identifier for this weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FName WeaponID;

	// Display name shown in UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	// Optional icon for UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UTexture2D> Icon;

	// ===== Universal Stats (all weapons have these) =====

	// Base damage per hit/pulse/jump
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FGameplayAttribute Damage = FGameplayAttribute(10.0f);

	// Attack speed multiplier (affects fire rate, pulse rate, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FGameplayAttribute AttackSpeed = FGameplayAttribute(1.0f);

	// ===== Audio/Visual =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> AttackVFX;

	// ===== Virtual Interface =====

	/**
	 * Returns the list of stats this weapon archetype supports.
	 * Used by the upgrade system to determine which upgrades apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual TArray<EWeaponStat> GetApplicableStats() const;

	/**
	 * Returns a description of what a stat means for this weapon type.
	 * E.g., "Penetration" -> "Enemies pierced" for projectiles, "Jump count" for chain.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual FText GetStatDescription(EWeaponStat Stat) const;

	/**
	 * Gets the current value of a stat (base * modifiers).
	 * Override in archetypes that have additional stats.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetStatValue(EWeaponStat Stat) const;

	/**
	 * Gets the gameplay attribute for a stat (for modification).
	 * Returns nullptr if the stat isn't applicable to this archetype.
	 */
	virtual FGameplayAttribute* GetStatAttribute(EWeaponStat Stat);

	/**
	 * Returns the base RPM (rounds per minute) before AttackSpeed modifier.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetBaseRPM() const { return 60.0f; }

	/**
	 * Returns the effective fire rate considering AttackSpeed modifier.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetEffectiveRPM() const { return GetBaseRPM() * AttackSpeed.GetCurrentValue(); }

	/**
	 * Spawns the appropriate weapon actor for this archetype.
	 * Override in derived classes to spawn the correct weapon type.
	 */
	virtual TSubclassOf<ASurvivorWeapon> GetWeaponActorClass() const;
};
