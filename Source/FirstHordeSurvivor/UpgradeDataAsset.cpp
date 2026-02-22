#include "UpgradeDataAsset.h"
#include "WeaponDataBase.h"

bool UUpgradeDataAsset::IsNewWeaponUpgrade() const
{
	return WeaponToGrant != nullptr;
}

bool UUpgradeDataAsset::IsPerWeaponUpgrade() const
{
	return TargetWeapon != nullptr;
}

bool UUpgradeDataAsset::HasWeaponStatEffects() const
{
	for (const FUpgradeEffect& Effect : Effects)
	{
		if (Effect.Type == EUpgradeType::WeaponStat && Effect.HasEffect())
		{
			return true;
		}
	}
	return false;
}

UTexture2D* UUpgradeDataAsset::GetEffectiveIcon() const
{
	if (Icon)
	{
		return Icon;
	}

	// Fall back to target weapon's icon for per-weapon upgrades
	if (TargetWeapon && TargetWeapon->Icon)
	{
		return TargetWeapon->Icon;
	}

	// Fall back to granted weapon's icon for new weapon upgrades
	if (WeaponToGrant && WeaponToGrant->Icon)
	{
		return WeaponToGrant->Icon;
	}

	return nullptr;
}

FText UUpgradeDataAsset::GetCombinedEffectsDescription() const
{
	TArray<FString> EffectStrings;

	for (const FUpgradeEffect& Effect : Effects)
	{
		FText EffectDesc = Effect.GetEffectDescription();
		if (!EffectDesc.IsEmpty())
		{
			EffectStrings.Add(EffectDesc.ToString());
		}
	}

	if (EffectStrings.Num() == 0)
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Join(EffectStrings, TEXT("\n")));
}

TArray<EWeaponStat> UUpgradeDataAsset::GetAffectedWeaponStats() const
{
	TArray<EWeaponStat> Stats;

	for (const FUpgradeEffect& Effect : Effects)
	{
		if (Effect.Type == EUpgradeType::WeaponStat && Effect.HasEffect())
		{
			Stats.AddUnique(Effect.WeaponStat);
		}
	}

	return Stats;
}
