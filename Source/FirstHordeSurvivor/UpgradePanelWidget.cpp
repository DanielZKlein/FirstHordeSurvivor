#include "UpgradePanelWidget.h"
#include "UpgradeDataAsset.h"
#include "Components/PanelWidget.h"

void UUpgradePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

void UUpgradePanelWidget::ShowUpgradeChoices(const TArray<UUpgradeDataAsset*>& Choices)
{
	CurrentChoices = Choices;

	// Clear existing options
	if (UpgradeOptionsContainer)
	{
		UpgradeOptionsContainer->ClearChildren();
	}

	// Let Blueprint create the visual option widgets
	BP_PopulateOptions(Choices);

	// Notify Blueprint
	BP_OnPanelShown();

	// Show the panel
	SetVisibility(ESlateVisibility::Visible);
}

void UUpgradePanelWidget::OnOptionSelected(int32 OptionIndex)
{
	if (CurrentChoices.IsValidIndex(OptionIndex))
	{
		UUpgradeDataAsset* Selected = CurrentChoices[OptionIndex];

		UE_LOG(LogTemp, Log, TEXT("UpgradePanelWidget: Option %d selected ('%s')"),
			OptionIndex, Selected ? *Selected->UpgradeID.ToString() : TEXT("null"));

		OnUpgradeSelected.Broadcast(Selected);
		ClosePanel();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UpgradePanelWidget: Invalid option index %d (choices: %d)"),
			OptionIndex, CurrentChoices.Num());
	}
}

void UUpgradePanelWidget::ClosePanel()
{
	// Notify Blueprint
	BP_OnPanelHidden();

	// Hide the panel
	SetVisibility(ESlateVisibility::Collapsed);

	// Clear choices
	CurrentChoices.Empty();
}
