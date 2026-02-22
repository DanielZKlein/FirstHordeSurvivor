#include "UpgradeEffect.h"

FText FUpgradeEffect::GetEffectDescription() const
{
	FString StatName;

	if (Type == EUpgradeType::PlayerStat)
	{
		// Convert enum to readable name
		switch (PlayerStat)
		{
		case EPlayerStat::MaxHealth: StatName = TEXT("Max Health"); break;
		case EPlayerStat::HealthRegen: StatName = TEXT("Health Regen"); break;
		case EPlayerStat::MaxSpeed: StatName = TEXT("Max Speed"); break;
		case EPlayerStat::MaxAcceleration: StatName = TEXT("Acceleration"); break;
		case EPlayerStat::MovementControl: StatName = TEXT("Movement Control"); break;
		case EPlayerStat::Armor: StatName = TEXT("Armor"); break;
		case EPlayerStat::Impact: StatName = TEXT("Impact"); break;
		default: StatName = TEXT("Unknown"); break;
		}
	}
	else if (Type == EUpgradeType::WeaponStat)
	{
		// Convert enum to readable name
		switch (WeaponStat)
		{
		case EWeaponStat::Damage: StatName = TEXT("Damage"); break;
		case EWeaponStat::AttackSpeed: StatName = TEXT("Attack Speed"); break;
		case EWeaponStat::Area: StatName = TEXT("Area"); break;
		case EWeaponStat::Penetration: StatName = TEXT("Penetration"); break;
		case EWeaponStat::ProjectileSpeed: StatName = TEXT("Projectile Speed"); break;
		case EWeaponStat::ProjectileCount: StatName = TEXT("Projectile Count"); break;
		case EWeaponStat::Duration: StatName = TEXT("Duration"); break;
		case EWeaponStat::Range: StatName = TEXT("Range"); break;
		case EWeaponStat::Knockback: StatName = TEXT("Knockback"); break;
		default: StatName = TEXT("Unknown"); break;
		}
	}

	// Build description string
	FString Result;

	if (AdditiveBonus != 0.0f)
	{
		if (AdditiveBonus > 0.0f)
		{
			Result = FString::Printf(TEXT("+%.0f %s"), AdditiveBonus, *StatName);
		}
		else
		{
			Result = FString::Printf(TEXT("%.0f %s"), AdditiveBonus, *StatName);
		}
	}

	if (MultiplicativeBonus != 1.0f)
	{
		float Percent = (MultiplicativeBonus - 1.0f) * 100.0f;
		if (!Result.IsEmpty())
		{
			Result += TEXT(", ");
		}
		if (Percent > 0.0f)
		{
			Result += FString::Printf(TEXT("+%.0f%% %s"), Percent, *StatName);
		}
		else
		{
			Result += FString::Printf(TEXT("%.0f%% %s"), Percent, *StatName);
		}
	}

	return FText::FromString(Result);
}
