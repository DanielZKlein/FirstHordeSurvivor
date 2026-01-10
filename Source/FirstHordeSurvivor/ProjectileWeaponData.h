#pragma once

#include "CoreMinimal.h"
#include "WeaponDataBase.h"
#include "ProjectileWeaponData.generated.h"

class ASurvivorProjectile;

/**
 * Weapon data for projectile-based weapons (missiles, arrows, bullets).
 * Supports penetration (pierce), explosion (AoE), multi-shot, and range.
 */
UCLASS(BlueprintType)
class FIRSTHORDESURVIVOR_API UProjectileWeaponData : public UWeaponDataBase
{
	GENERATED_BODY()

public:
	// ===== Projectile Config =====

	// The projectile actor to spawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<ASurvivorProjectile> ProjectileClass;

	// Base fire rate in rounds per minute
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float BaseRPM = 60.0f;

	// ===== Projectile Stats =====

	// Projectile travel speed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute ProjectileSpeed = FGameplayAttribute(1000.0f);

	// Maximum travel distance before despawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute Range = FGameplayAttribute(2000.0f);

	// Number of enemies the projectile can pierce through (0 = stop on first hit)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute Penetration = FGameplayAttribute(0.0f);

	// Explosion radius on impact (0 = no explosion, single target only)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute Area = FGameplayAttribute(0.0f);

	// Number of projectiles per shot (for multi-shot weapons)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute ProjectileCount = FGameplayAttribute(1.0f);

	// Knockback force applied on hit
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Projectile")
	FGameplayAttribute Knockback = FGameplayAttribute(0.0f);

	// ===== Targeting =====

	// Cone angle in degrees for random spread
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	float Precision = 5.0f;

	// Weight for distance scoring (negative = prefer closer targets)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	float RangeWeight = -1.0f;

	// Weight for targets in front of player velocity
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	float InFrontWeight = 1000.0f;

	// ===== Explosion Visuals =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	// ===== UWeaponDataBase Interface =====

	virtual TArray<EWeaponStat> GetApplicableStats() const override;
	virtual FText GetStatDescription(EWeaponStat Stat) const override;
	virtual float GetStatValue(EWeaponStat Stat) const override;
	virtual FGameplayAttribute* GetStatAttribute(EWeaponStat Stat) override;
	virtual float GetBaseRPM() const override { return BaseRPM; }
};
