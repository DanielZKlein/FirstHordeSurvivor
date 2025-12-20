#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AttributeComponent.h"
#include "SurvivorEnemy.generated.h"

class UEnemyData;
class ASurvivorCharacter;

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

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	UEnemyData* EnemyData;

	// State
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ASurvivorCharacter* TargetPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsOverlappingPlayer;

	FTimerHandle TimerHandle_Attack;

	// Functions
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnDeath(UAttributeComponent* Component, bool bIsResultOfEditorChange);

	void AttackPlayer();
	void StartAttackTimer();
	void StopAttackTimer();

	// Helper to apply DataAsset stats
	void InitializeFromData();
};
