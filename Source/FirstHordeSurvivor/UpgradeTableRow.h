#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UpgradeTableRow.generated.h"

class UUpgradeDataAsset;

/**
 * Row struct for the upgrade registry DataTable.
 * Each row references an UpgradeDataAsset and can override/control its availability.
 *
 * Usage:
 * 1. Create individual UUpgradeDataAsset files for each upgrade
 * 2. Create a DataTable using this row type
 * 3. Add rows referencing each upgrade asset
 * 4. Set the DataTable on your GameMode's UpgradeDataTable property
 */
USTRUCT(BlueprintType)
struct FIRSTHORDESURVIVOR_API FUpgradeTableRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// The upgrade data asset for this row
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UUpgradeDataAsset> UpgradeAsset;

	// Whether this upgrade is enabled in this table
	// Useful for quickly disabling upgrades without removing them
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	bool bEnabled = true;

	FUpgradeTableRow()
		: bEnabled(true)
	{}
};
