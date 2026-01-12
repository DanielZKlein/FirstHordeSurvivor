#include "WeaponDataBase.h"
#include "SurvivorWeapon.h"

TArray<EWeaponStat> UWeaponDataBase::GetApplicableStats() const
{
	// Base class only has universal stats
	// AttackSpeed is always applicable (handled by weapon actor)
	return { EWeaponStat::Damage, EWeaponStat::AttackSpeed };
}

FText UWeaponDataBase::GetStatDescription(EWeaponStat Stat) const
{
	switch (Stat)
	{
	case EWeaponStat::Damage:
		return FText::FromString(TEXT("Damage dealt"));
	case EWeaponStat::AttackSpeed:
		return FText::FromString(TEXT("Attack rate"));
	default:
		return FText::FromString(TEXT("Unknown stat"));
	}
}

float UWeaponDataBase::GetBaseStatValue(EWeaponStat Stat) const
{
	switch (Stat)
	{
	case EWeaponStat::Damage:
		return BaseDamage;
	case EWeaponStat::AttackSpeed:
		return 1.0f; // AttackSpeed always starts at 1.0 (100%)
	default:
		return 0.0f;
	}
}

TSubclassOf<ASurvivorWeapon> UWeaponDataBase::GetWeaponActorClass() const
{
	// Base class returns the default weapon actor
	return ASurvivorWeapon::StaticClass();
}
