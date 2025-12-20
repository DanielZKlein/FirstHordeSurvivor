#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyData.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS()
class FIRSTHORDESURVIVOR_API UEnemyData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> EnemyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UMaterialInterface> EnemyMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float BaseHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float MoveSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards")
	int32 MinXP = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards")
    int32 MaxXP = 20;
};
