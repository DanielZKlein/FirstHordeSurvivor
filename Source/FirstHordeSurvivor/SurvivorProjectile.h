#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivorProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASurvivorProjectile();

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * Initialize projectile with full stats.
	 * @param Speed - Travel speed
	 * @param DamageAmount - Damage per hit
	 * @param Range - Max travel distance
	 * @param PierceCount - Number of enemies to pierce through (0 = stop on first)
	 * @param ExplosionRadius - AoE radius on impact (0 = single target)
	 * @param KnockbackForce - Push force on hit
	 * @param InImpactSound - Sound to play on single-target hit (overrides Blueprint default)
	 * @param InImpactVFX - VFX to spawn on single-target hit (overrides Blueprint default)
	 * @param InExplosionSound - Sound to play on explosion
	 * @param InExplosionVFX - VFX to spawn on explosion
	 */
	void Initialize(
		float Speed,
		float DamageAmount,
		float Range,
		int32 PierceCount = 0,
		float ExplosionRadius = 0.0f,
		float KnockbackForce = 0.0f,
		USoundBase* InImpactSound = nullptr,
		UNiagaraSystem* InImpactVFX = nullptr,
		USoundBase* InExplosionSound = nullptr,
		UNiagaraSystem* InExplosionVFX = nullptr
	);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* MovementComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* TrailComp;

	// ===== Properties =====

	float Damage;
	float MaxRange;
	FVector StartLocation;

	// Pierce count remaining (0 = destroy on next hit)
	int32 RemainingPierces;

	// Explosion radius (0 = no explosion)
	float AoERadius;

	// Knockback force
	float Knockback;

	// Track hit enemies to avoid double-hits during pierce
	UPROPERTY()
	TSet<AActor*> HitEnemies;

	// ===== Visuals =====

	UPROPERTY(EditDefaultsOnly, Category = "0 - Visuals")
	USoundBase* HitSound;

	UPROPERTY(EditDefaultsOnly, Category = "0 - Visuals")
	UNiagaraSystem* HitVFX;

	// Explosion effects (passed from weapon data)
	UPROPERTY()
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	// ===== Internal Functions =====

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Apply damage to a single target with knockback. */
	void DamageTarget(AActor* Target);

	/** Trigger explosion, damaging all enemies in radius except the direct-hit actor. */
	void Explode(AActor* DirectHitActor = nullptr);

	/** Apply knockback force to an actor. */
	void ApplyKnockback(AActor* Target);

public:
	virtual void Tick(float DeltaTime) override;
};
