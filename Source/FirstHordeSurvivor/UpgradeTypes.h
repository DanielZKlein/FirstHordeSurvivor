#pragma once

#include "CoreMinimal.h"
#include "AttributeComponent.h"
#include "UpgradeTypes.generated.h"

class UWeaponDataBase;

/**
 * Stats that weapons can have. Each weapon archetype interprets these differently.
 * Example: "Penetration" means pierce count for projectiles, jump count for chain weapons.
 */
UENUM(BlueprintType)
enum class EWeaponStat : uint8
{
	// Universal (all weapons have these)
	Damage,				// Damage per hit/pulse/jump
	AttackSpeed,		// Fire/pulse/cast rate multiplier

	// Conditional (weapons opt-in)
	Area,				// AoE radius, search radius, pulse radius
	Penetration,		// Pierce count, jump count, bounces
	ProjectileSpeed,	// Projectile travel speed
	ProjectileCount,	// Multi-shot count
	Duration,			// Aura duration, DoT length, chain active time
	Range,				// Max travel distance, max chain range
	Knockback,			// Push force on hit

	COUNT UMETA(Hidden)
};

/**
 * Types of upgrades the player can acquire.
 */
UENUM(BlueprintType)
enum class EUpgradeType : uint8
{
	PlayerStat,			// Direct player attribute boost (health, speed, etc.)
	WeaponStat,			// Boost to a weapon stat (applies to all weapons using it)
	NewWeapon,			// Unlock a new weapon
	WeaponEvolution,	// Upgrade weapon to evolved form (future)
};

/**
 * Definition of an upgrade that can be offered to the player.
 */
USTRUCT(BlueprintType)
struct FUpgradeDefinition
{
	GENERATED_BODY()

public:
	// Unique identifier for this upgrade
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName UpgradeID;

	// Display name shown in UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	// Description text shown in UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText Description;

	// Optional icon for UI display
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UTexture2D> Icon;

	// What type of upgrade this is
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EUpgradeType Type = EUpgradeType::PlayerStat;

	// For PlayerStat type: which attribute to modify (e.g., "MaxHealth", "MaxSpeed")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Type == EUpgradeType::PlayerStat"))
	FName PlayerAttribute;

	// For WeaponStat type: which weapon stat to boost
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Type == EUpgradeType::WeaponStat"))
	EWeaponStat WeaponStat = EWeaponStat::Damage;

	// Flat bonus added to the stat
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float AdditiveBonus = 0.0f;

	// Multiplier applied to the stat (1.1 = +10%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float MultiplicativeBonus = 1.0f;

	// For NewWeapon type: the weapon to grant
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (EditCondition = "Type == EUpgradeType::NewWeapon"))
	TObjectPtr<UWeaponDataBase> WeaponToGrant;

	// Weight for random selection (higher = more common)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1"))
	int32 Weight = 100;

	// Maximum times this upgrade can be chosen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "1"))
	int32 MaxStacks = 5;

	// Minimum player level to offer this upgrade
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "1"))
	int32 MinLevel = 1;

	// Other upgrades that must be acquired first
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	TArray<FName> RequiredUpgrades;

	FUpgradeDefinition()
		: Type(EUpgradeType::PlayerStat)
		, WeaponStat(EWeaponStat::Damage)
		, AdditiveBonus(0.0f)
		, MultiplicativeBonus(1.0f)
		, Weight(100)
		, MaxStacks(5)
		, MinLevel(1)
	{}
};
