#include "SurvivorGameMode.h"
#include "XPGemSubsystem.h"
#include "XPGemVisualConfig.h"
#include "UpgradeSubsystem.h"

ASurvivorGameMode::ASurvivorGameMode()
{
}

void ASurvivorGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Register XP Gem configuration with subsystem
	if (UXPGemSubsystem* GemSubsystem = GetWorld()->GetSubsystem<UXPGemSubsystem>())
	{
		if (XPGemVisualConfig)
		{
			GemSubsystem->RegisterVisualConfig(XPGemVisualConfig);
		}

		if (XPGemClass)
		{
			GemSubsystem->RegisterGemClass(XPGemClass);
		}
	}

	// Register Upgrade DataTable with subsystem
	if (UUpgradeSubsystem* UpgradeSubsystem = GetWorld()->GetSubsystem<UUpgradeSubsystem>())
	{
		if (UpgradeDataTable)
		{
			UpgradeSubsystem->RegisterUpgradeTable(UpgradeDataTable);
		}
	}
}
