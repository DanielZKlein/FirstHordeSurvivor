#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UpgradePanelWidget.generated.h"

class UUpgradeDataAsset;
class UPanelWidget;

// Delegate for when player selects an upgrade
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradeSelected, UUpgradeDataAsset*, SelectedUpgrade);

/**
 * C++ base class for the upgrade selection panel.
 * Create a Blueprint subclass (WBP_UpgradePanel) for visual styling.
 *
 * This widget displays 3 upgrade choices and handles selection.
 * The Blueprint subclass should:
 * - Implement BP_PopulateOptions to create visual option widgets
 * - Call OnOptionSelected when a button is clicked
 */
UCLASS(Abstract)
class FIRSTHORDESURVIVOR_API UUpgradePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Initialize the panel with upgrade choices
	UFUNCTION(BlueprintCallable, Category = "Upgrade Panel")
	void ShowUpgradeChoices(const TArray<UUpgradeDataAsset*>& Choices);

	// Called when selection is complete (hides panel)
	UFUNCTION(BlueprintCallable, Category = "Upgrade Panel")
	void ClosePanel();

	// Delegate for external listeners (e.g., HUD to apply upgrade and unpause)
	UPROPERTY(BlueprintAssignable, Category = "Upgrade Panel")
	FOnUpgradeSelected OnUpgradeSelected;

	// Get the current upgrade choices
	UFUNCTION(BlueprintPure, Category = "Upgrade Panel")
	const TArray<UUpgradeDataAsset*>& GetCurrentChoices() const { return CurrentChoices; }

protected:
	virtual void NativeConstruct() override;

	// Container for upgrade option widgets (bind in Blueprint to any panel type)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> UpgradeOptionsContainer;

	// The current upgrade choices
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade Panel")
	TArray<UUpgradeDataAsset*> CurrentChoices;

	// Blueprint-implementable: Create and add option widgets to UpgradeOptionsContainer
	// Called automatically by ShowUpgradeChoices
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade Panel")
	void BP_PopulateOptions(const TArray<UUpgradeDataAsset*>& Choices);

	// Blueprint-implementable: Called when the panel is about to be shown
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade Panel")
	void BP_OnPanelShown();

	// Blueprint-implementable: Called when the panel is about to be hidden
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade Panel")
	void BP_OnPanelHidden();

	// Called when an upgrade button is clicked (Blueprint should call this)
	UFUNCTION(BlueprintCallable, Category = "Upgrade Panel")
	void OnOptionSelected(int32 OptionIndex);
};
