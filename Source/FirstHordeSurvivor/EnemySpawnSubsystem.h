#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/DataTable.h"
#include "EnemySpawnSubsystem.generated.h"

class ASurvivorEnemy;
class UDataTable;

/**
 * Row structure for spawn configuration DataTable.
 * Each row defines an enemy type that can spawn.
 */
USTRUCT(BlueprintType)
struct FEnemySpawnEntry : public FTableRowBase
{
	GENERATED_BODY()

public:
	// Row name in DT_Enemies to spawn
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FName EnemyRowName;

	// Relative spawn weight (higher = more common)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.1"))
	float Weight = 1.0f;

	// Minutes into game before this enemy can spawn
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0"))
	float MinuteUnlock = 0.0f;
};

/**
 * WorldSubsystem that manages enemy spawning and pooling.
 */
UCLASS()
class FIRSTHORDESURVIVOR_API UEnemySpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Pool management
	ASurvivorEnemy* GetEnemyFromPool();
	void ReturnEnemyToPool(ASurvivorEnemy* Enemy);

	// Called by enemies on death
	void OnEnemyDeath(ASurvivorEnemy* Enemy);

	// Start/stop spawning
	void StartSpawning();
	void StopSpawning();

	// Configuration helper (call from GameMode or Level Blueprint)
	UFUNCTION(BlueprintCallable, Category = "Config")
	void Configure(TSubclassOf<ASurvivorEnemy> InEnemyClass, UDataTable* InEnemyDataTable, UDataTable* InSpawnConfigTable);

	// Configuration
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<ASurvivorEnemy> EnemyClass;

	UPROPERTY(EditAnywhere, Category = "Config")
	UDataTable* EnemyDataTable;  // DT_Enemies - enemy stats

	UPROPERTY(EditAnywhere, Category = "Config")
	UDataTable* SpawnConfigTable;  // DT_EnemySpawns - spawn weights/unlocks

	// Spawn Rate Settings
	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float BaseSpawnRate = 30.0f;  // Spawns per minute at game start

	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float SpawnRateGrowth = 5.0f;  // Extra spawns/min per minute elapsed

	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float MaxSpawnRate = 120.0f;  // Hard cap on spawns per minute

	// Responsive System
	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float TargetEnemyCount = 50.0f;  // "Full" enemy count for responsive calc

	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float MaxResponsiveBonus = 60.0f;  // Max extra spawns/min when few enemies

	UPROPERTY(EditAnywhere, Category = "Spawn Rate")
	float Responsiveness = 0.5f;  // 0-1, how aggressively to spawn when empty

	// Caps
	UPROPERTY(EditAnywhere, Category = "Limits")
	int32 MaxEnemiesOnMap = 150;  // Performance cap

	UPROPERTY(EditAnywhere, Category = "Limits")
	int32 PreWarmCount = 20;  // Enemies to pre-spawn inactive

	// Spawn Location
	UPROPERTY(EditAnywhere, Category = "Location")
	float SpawnRadius = 2500.0f;  // Distance from player (outside camera)

	UPROPERTY(EditAnywhere, Category = "Location")
	float SpawnMargin = 500.0f;  // Random variance in spawn distance

protected:
	// Pool storage
	UPROPERTY()
	TArray<ASurvivorEnemy*> EnemyPool;

	UPROPERTY()
	TArray<ASurvivorEnemy*> ActiveEnemies;

	// Spawn timer
	FTimerHandle SpawnTimerHandle;

	// Track if configured
	bool bIsConfigured = false;

	// Internal functions
	void PreWarmPool(int32 Count);
	void SpawnEnemy();
	FVector GetSpawnLocation();
	FName SelectEnemyType();
	float GetCurrentSpawnRate();
	void LoadDefaultAssets();
};
