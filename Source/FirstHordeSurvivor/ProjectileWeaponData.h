#pragma once

#include "CoreMinimal.h"
#include "WeaponDataBase.h"
#include "ProjectileWeaponData.generated.h"

class ASurvivorProjectile;

/**
 * How multiple projectiles are fired when ProjectileCount > 1.
 */
UENUM(BlueprintType)
enum class EMultiShotMode : uint8
{
	Volley,		// All projectiles fire simultaneously in a fan
	Barrage,	// Projectiles fire sequentially, then cooldown starts
};

/**
 * Weapon data for projectile-based weapons (missiles, arrows, bullets).
 * Supports penetration (pierce), explosion (AoE), multi-shot, and range.
 *
 * All values are BASE stats. Runtime modifiers from upgrades are
 * tracked separately in the weapon actor.
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	float BaseRPM = 60.0f;

	// ===== Projectile Stats =====

	// Projectile travel speed (units/second)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float ProjectileSpeed = 1000.0f;

	// Maximum travel distance before despawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Range = 2000.0f;

	// Number of enemies the projectile can pierce through (0 = stop on first hit)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	int32 Penetration = 0;

	// Explosion radius on impact (0 = no explosion, single target only)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Area = 0.0f;

	// Number of projectiles per shot (for multi-shot weapons)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1"))
	int32 ProjectileCount = 1;

	// ===== Multi-Shot Settings (only used when ProjectileCount > 1) =====

	// How multiple projectiles are fired
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multi-Shot", meta = (EditCondition = "ProjectileCount > 1"))
	EMultiShotMode MultiShotMode = EMultiShotMode::Volley;

	// Total spread angle in degrees for the projectile fan
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multi-Shot", meta = (EditCondition = "ProjectileCount > 1", ClampMin = "0", ClampMax = "360"))
	float SpreadAngle = 30.0f;

	// Internal fire rate for Barrage mode (RPM between sequential shots within a burst)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multi-Shot", meta = (EditCondition = "ProjectileCount > 1 && MultiShotMode == EMultiShotMode::Barrage", ClampMin = "1"))
	float BarrageRPM = 120.0f;

	// Knockback force applied on hit
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Knockback = 0.0f;

	// ===== Targeting =====

	// Cone angle in degrees for random spread
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "180"))
	float Precision = 5.0f;

	// Weight for distance scoring (negative = prefer closer targets)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	float RangeWeight = -1.0f;

	// Weight for targets in front of player velocity
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float InFrontWeight = 1000.0f;

	// ===== Explosion Visuals =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	// ===== UWeaponDataBase Interface =====

	virtual TArray<EWeaponStat> GetApplicableStats() const override;
	virtual FText GetStatDescription(EWeaponStat Stat) const override;
	virtual float GetBaseStatValue(EWeaponStat Stat) const override;
	virtual float GetBaseRPM() const override { return BaseRPM; }
};
