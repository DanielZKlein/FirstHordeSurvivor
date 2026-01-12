#include "ProjectileWeaponData.h"

TArray<EWeaponStat> UProjectileWeaponData::GetApplicableStats() const
{
	return {
		EWeaponStat::Damage,
		EWeaponStat::AttackSpeed,
		EWeaponStat::ProjectileSpeed,
		EWeaponStat::Range,
		EWeaponStat::Penetration,
		EWeaponStat::Area,
		EWeaponStat::ProjectileCount,
		EWeaponStat::Knockback
	};
}

FText UProjectileWeaponData::GetStatDescription(EWeaponStat Stat) const
{
	switch (Stat)
	{
	case EWeaponStat::Damage:
		return FText::FromString(TEXT("Damage per hit"));
	case EWeaponStat::AttackSpeed:
		return FText::FromString(TEXT("Fire rate"));
	case EWeaponStat::ProjectileSpeed:
		return FText::FromString(TEXT("Projectile speed"));
	case EWeaponStat::Range:
		return FText::FromString(TEXT("Max travel distance"));
	case EWeaponStat::Penetration:
		return FText::FromString(TEXT("Enemies pierced"));
	case EWeaponStat::Area:
		return FText::FromString(TEXT("Explosion radius"));
	case EWeaponStat::ProjectileCount:
		return FText::FromString(TEXT("Projectiles per shot"));
	case EWeaponStat::Knockback:
		return FText::FromString(TEXT("Knockback force"));
	default:
		return Super::GetStatDescription(Stat);
	}
}

float UProjectileWeaponData::GetBaseStatValue(EWeaponStat Stat) const
{
	switch (Stat)
	{
	case EWeaponStat::ProjectileSpeed:
		return ProjectileSpeed;
	case EWeaponStat::Range:
		return Range;
	case EWeaponStat::Penetration:
		return static_cast<float>(Penetration);
	case EWeaponStat::Area:
		return Area;
	case EWeaponStat::ProjectileCount:
		return static_cast<float>(ProjectileCount);
	case EWeaponStat::Knockback:
		return Knockback;
	default:
		return Super::GetBaseStatValue(Stat);
	}
}
