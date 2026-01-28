#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AttributeComponent.h"
#include "EnemyData.h"
#include "SurvivorEnemy.generated.h"

class ASurvivorCharacter;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class UDataTable;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ASurvivorEnemy();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAttributeComponent* AttributeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* EnemyMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* AttackOverlapComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* HealthBarComp;

	// Configuration - DataTable lookup
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	UDataTable* EnemyDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName EnemyRowName;

	// Cached row data (set in InitializeFromData)
	const FEnemyTableRow* EnemyData = nullptr;

	// State
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ASurvivorCharacter* TargetPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsOverlappingPlayer;

	FTimerHandle TimerHandle_Attack;

	// Hit flash
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	float HitFlashIntensity = 0.0f;
	float HitFlashDecayRate = 5.0f;
	float HitFlashHoldDuration = 0.08f;  // Hold at full intensity for ~5 frames at 60fps
	float HitFlashHoldTimer = 0.0f;
	float LastKnownHealth = 0.0f;

	// Knockback state
	UPROPERTY()
	FVector KnockbackVelocity = FVector::ZeroVector;

	// Enemies we've already transferred momentum to this knockback (prevents double-hits)
	UPROPERTY()
	TSet<ASurvivorEnemy*> KnockbackHitEnemies;

	// Functions
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnDeath(UAttributeComponent* Component, bool bIsResultOfEditorChange);

	UFUNCTION()
	void OnHealthChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange);

	void AttackPlayer();
	void StartAttackTimer();
	void StopAttackTimer();

	// Helper to apply stats from DataTable row
	void InitializeFromData();

	// Knockback system - applies impulse and handles momentum transfer on collision
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyKnockback(FVector Impulse);

	// Get mass for knockback calculations (uses MaxHealth as proxy)
	float GetKnockbackMass() const;

	// Get knockback multiplier based on HP (lighter enemies get knocked back more)
	// Returns 1.0 for light enemies, scales down to minimum for heavy enemies
	float GetKnockbackResistance() const;

	// Pooling support
	void Deactivate();
	void Reinitialize(UDataTable* DataTable, FName RowName, FVector Location);

protected:
	// Process knockback movement and collisions
	void ProcessKnockback(float DeltaTime);
};
