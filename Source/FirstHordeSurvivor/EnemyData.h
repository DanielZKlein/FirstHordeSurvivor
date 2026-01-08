#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemyData.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * Row structure for the Enemy DataTable.
 * Each row defines a complete enemy type.
 */
USTRUCT(BlueprintType)
struct FEnemyTableRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// Visuals
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> EnemyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UMaterialInterface> EnemyMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	float MeshScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor EnemyColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0.0"))
	float EmissiveStrength = 0.0f;

	// Stats
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MoveSpeed = 400.0f;

	// Rewards
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	int32 MinXP = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	int32 MaxXP = 20;
};
