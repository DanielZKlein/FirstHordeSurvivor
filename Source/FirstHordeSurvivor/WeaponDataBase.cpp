#include "WeaponDataBase.h"
#include "SurvivorWeapon.h"

TArray<EWeaponStat> UWeaponDataBase::GetApplicableStats() const
{
	// Base class only has universal stats
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

float UWeaponDataBase::GetStatValue(EWeaponStat Stat) const
{
	switch (Stat)
	{
	case EWeaponStat::Damage:
		return Damage.GetCurrentValue();
	case EWeaponStat::AttackSpeed:
		return AttackSpeed.GetCurrentValue();
	default:
		return 0.0f;
	}
}

FGameplayAttribute* UWeaponDataBase::GetStatAttribute(EWeaponStat Stat)
{
	switch (Stat)
	{
	case EWeaponStat::Damage:
		return &Damage;
	case EWeaponStat::AttackSpeed:
		return &AttackSpeed;
	default:
		return nullptr;
	}
}

TSubclassOf<ASurvivorWeapon> UWeaponDataBase::GetWeaponActorClass() const
{
	// Base class returns the default weapon actor
	return ASurvivorWeapon::StaticClass();
}
