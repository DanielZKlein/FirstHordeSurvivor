#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UpgradeTypes.h"
#include "AttributeComponent.h"
#include "SurvivorWeapon.generated.h"

class UWeaponDataBase;
class UProjectileWeaponData;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorWeapon : public AActor
{
	GENERATED_BODY()

public:
	ASurvivorWeapon();

protected:
	virtual void BeginPlay() override;

public:
	// The weapon data asset defining base stats and behavior
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TObjectPtr<UWeaponDataBase> WeaponData;

	// Start/stop automatic firing
	void StartShooting();
	void StopShooting();

	// ===== Stat System =====

	/**
	 * Get the effective value of a weapon stat (base + modifiers).
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetStat(EWeaponStat Stat) const;

	/**
	 * Apply an upgrade to a specific stat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ApplyStatUpgrade(EWeaponStat Stat, float Additive, float Multiplicative);

	/**
	 * Check if this weapon uses a particular stat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool UsesStat(EWeaponStat Stat) const;

	/**
	 * Get all stats this weapon uses.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	TArray<EWeaponStat> GetApplicableStats() const;

protected:
	FTimerHandle TimerHandle_Attack;
	FTimerHandle TimerHandle_Burst;

	// Runtime modifiers applied on top of base weapon stats
	UPROPERTY()
	TMap<EWeaponStat, FGameplayAttribute> StatModifiers;

	// Barrage mode state
	int32 RemainingBurstProjectiles = 0;
	int32 TotalBurstProjectiles = 0;
	FVector BurstBaseDirection;
	FVector BurstSpawnLocation;

	void Fire();
	AActor* FindBestTarget();

	// Helper to get projectile data (returns nullptr if not a projectile weapon)
	UProjectileWeaponData* GetProjectileData() const;

	// Calculate effective fire rate considering modifiers
	float GetEffectiveRPM() const;

	// Fire a single projectile with given index in the spread pattern
	void FireSingleProjectile(int32 ProjectileIndex, int32 TotalProjectiles);

	// Continue firing burst projectiles (Barrage mode)
	void ContinueBurst();

	// Schedule the next attack after cooldown
	void ScheduleNextAttack();
};
