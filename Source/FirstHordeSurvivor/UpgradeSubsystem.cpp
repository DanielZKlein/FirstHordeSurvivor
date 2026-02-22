#include "UpgradeSubsystem.h"
#include "UpgradeDataAsset.h"
#include "UpgradeTableRow.h"
#include "UpgradeEffect.h"
#include "SurvivorCharacter.h"
#include "SurvivorWeapon.h"
#include "WeaponDataBase.h"
#include "AttributeComponent.h"
#include "Engine/DataTable.h"

void UUpgradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentPlayerLevel = 1;
}

void UUpgradeSubsystem::Deinitialize()
{
	AllUpgrades.Empty();
	OwnedUpgradeStacks.Empty();
	WeaponStates.Empty();
	PlayerCharacter.Reset();
	Super::Deinitialize();
}

void UUpgradeSubsystem::RegisterUpgradeTable(UDataTable* InTable)
{
	UpgradeTable = InTable;
	CacheUpgradesFromTable();
}

void UUpgradeSubsystem::RegisterPlayer(ASurvivorCharacter* InPlayer)
{
	PlayerCharacter = InPlayer;
}

void UUpgradeSubsystem::CacheUpgradesFromTable()
{
	AllUpgrades.Empty();

	if (!UpgradeTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpgradeSubsystem: No upgrade table registered"));
		return;
	}

	TArray<FUpgradeTableRow*> Rows;
	UpgradeTable->GetAllRows<FUpgradeTableRow>(TEXT("CacheUpgrades"), Rows);

	for (const FUpgradeTableRow* Row : Rows)
	{
		if (Row && Row->UpgradeAsset && Row->bEnabled)
		{
			AllUpgrades.Add(Row->UpgradeAsset);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UpgradeSubsystem: Cached %d upgrades from table"), AllUpgrades.Num());
}

TArray<UUpgradeDataAsset*> UUpgradeSubsystem::GetRandomUpgradeChoices(int32 Count)
{
	TArray<UUpgradeDataAsset*> AvailablePool;
	TArray<int32> Weights;

	// Build pool of available upgrades
	for (UUpgradeDataAsset* Upgrade : AllUpgrades)
	{
		if (IsUpgradeAvailable(Upgrade))
		{
			AvailablePool.Add(Upgrade);
			Weights.Add(GetEffectiveWeight(Upgrade));
		}
	}

	TArray<UUpgradeDataAsset*> Choices;

	// Weighted random selection without replacement
	while (Choices.Num() < Count && AvailablePool.Num() > 0)
	{
		// Calculate total weight
		int32 TotalWeight = 0;
		for (int32 W : Weights)
		{
			TotalWeight += W;
		}

		if (TotalWeight <= 0)
		{
			break;
		}

		// Pick random point
		int32 RandomPoint = FMath::RandRange(0, TotalWeight - 1);
		int32 AccumulatedWeight = 0;
		int32 SelectedIndex = 0;

		for (int32 i = 0; i < Weights.Num(); ++i)
		{
			AccumulatedWeight += Weights[i];
			if (RandomPoint < AccumulatedWeight)
			{
				SelectedIndex = i;
				break;
			}
		}

		// Add to choices and remove from pool
		Choices.Add(AvailablePool[SelectedIndex]);
		AvailablePool.RemoveAt(SelectedIndex);
		Weights.RemoveAt(SelectedIndex);
	}

	return Choices;
}

bool UUpgradeSubsystem::IsUpgradeAvailable(const UUpgradeDataAsset* Upgrade) const
{
	if (!Upgrade)
	{
		return false;
	}

	// Check all availability conditions
	if (!IsNotMaxedOut(Upgrade))
	{
		return false;
	}

	if (!MeetsLevelRequirement(Upgrade))
	{
		return false;
	}

	if (!MeetsPrerequisites(Upgrade))
	{
		return false;
	}

	if (!MeetsWeaponRequirement(Upgrade))
	{
		return false;
	}

	// NewWeapon upgrades also need an available slot
	if (Upgrade->IsNewWeaponUpgrade() && !HasWeaponSlotAvailable())
	{
		return false;
	}

	return true;
}

bool UUpgradeSubsystem::MeetsPrerequisites(const UUpgradeDataAsset* Upgrade) const
{
	for (const FName& RequiredID : Upgrade->RequiredUpgrades)
	{
		if (!HasUpgrade(RequiredID))
		{
			return false;
		}
	}
	return true;
}

bool UUpgradeSubsystem::MeetsLevelRequirement(const UUpgradeDataAsset* Upgrade) const
{
	return CurrentPlayerLevel >= Upgrade->MinLevel;
}

bool UUpgradeSubsystem::MeetsWeaponRequirement(const UUpgradeDataAsset* Upgrade) const
{
	// If upgrade requires weapon ownership, check if player has that weapon
	if (Upgrade->bRequiresWeaponOwnership && Upgrade->TargetWeapon)
	{
		return HasWeapon(Upgrade->TargetWeapon);
	}

	// NewWeapon upgrades should NOT appear if player already owns that weapon
	if (Upgrade->IsNewWeaponUpgrade() && Upgrade->WeaponToGrant)
	{
		return !HasWeapon(Upgrade->WeaponToGrant);
	}

	return true;
}

bool UUpgradeSubsystem::IsNotMaxedOut(const UUpgradeDataAsset* Upgrade) const
{
	int32 CurrentStacks = GetUpgradeStacks(Upgrade->UpgradeID);
	return CurrentStacks < Upgrade->MaxStacks;
}

bool UUpgradeSubsystem::HasWeaponSlotAvailable() const
{
	return GetCurrentWeaponCount() < GetTotalWeaponSlots();
}

int32 UUpgradeSubsystem::GetEffectiveWeight(const UUpgradeDataAsset* Upgrade) const
{
	return Upgrade->Weight;
}

void UUpgradeSubsystem::ApplyUpgrade(UUpgradeDataAsset* Upgrade)
{
	if (!Upgrade)
	{
		return;
	}

	// Increment stack count
	int32& Stacks = OwnedUpgradeStacks.FindOrAdd(Upgrade->UpgradeID);
	Stacks++;

	// Check if this is a new weapon upgrade
	if (Upgrade->IsNewWeaponUpgrade() && Upgrade->WeaponToGrant)
	{
		ApplyNewWeaponUpgrade(Upgrade->WeaponToGrant);
	}

	// Apply all effects
	for (const FUpgradeEffect& Effect : Upgrade->Effects)
	{
		if (!Effect.HasEffect())
		{
			continue;
		}

		switch (Effect.Type)
		{
		case EUpgradeType::PlayerStat:
			ApplyPlayerStatEffect(Effect);
			break;

		case EUpgradeType::WeaponStat:
			ApplyWeaponStatEffect(Effect, Upgrade->TargetWeapon);
			break;

		default:
			break;
		}
	}

	// If this was a per-weapon upgrade, increment that weapon's level
	if (Upgrade->IsPerWeaponUpgrade() && Upgrade->TargetWeapon)
	{
		IncrementWeaponLevel(Upgrade->TargetWeapon);
	}

	// Broadcast upgrade applied
	OnUpgradeApplied.Broadcast(Upgrade);

	UE_LOG(LogTemp, Log, TEXT("UpgradeSubsystem: Applied upgrade '%s' (stack %d)"),
		*Upgrade->UpgradeID.ToString(), Stacks);
}

void UUpgradeSubsystem::ApplyPlayerStatEffect(const FUpgradeEffect& Effect)
{
	ASurvivorCharacter* Player = PlayerCharacter.Get();
	if (!Player || !Player->AttributeComp)
	{
		return;
	}

	UAttributeComponent* Attr = Player->AttributeComp;

	// Map PlayerStat enum to actual attribute
	FGameplayAttribute* TargetAttr = nullptr;

	switch (Effect.PlayerStat)
	{
	case EPlayerStat::MaxHealth:
		TargetAttr = &Attr->MaxHealth;
		break;
	case EPlayerStat::HealthRegen:
		TargetAttr = &Attr->HealthRegen;
		break;
	case EPlayerStat::MaxSpeed:
		TargetAttr = &Attr->MaxSpeed;
		break;
	case EPlayerStat::MaxAcceleration:
		TargetAttr = &Attr->MaxAcceleration;
		break;
	case EPlayerStat::MovementControl:
		TargetAttr = &Attr->MovementControl;
		break;
	case EPlayerStat::Armor:
		TargetAttr = &Attr->Armor;
		break;
	case EPlayerStat::Impact:
		TargetAttr = &Attr->Impact;
		break;
	default:
		break;
	}

	if (TargetAttr)
	{
		if (Effect.AdditiveBonus != 0.0f)
		{
			Attr->ApplyAdditive(*TargetAttr, Effect.AdditiveBonus);
		}
		if (Effect.MultiplicativeBonus != 1.0f)
		{
			Attr->ApplyMultiplicative(*TargetAttr, Effect.MultiplicativeBonus);
		}
	}
}

void UUpgradeSubsystem::ApplyWeaponStatEffect(const FUpgradeEffect& Effect, UWeaponDataBase* TargetWeapon)
{
	if (TargetWeapon)
	{
		// Per-weapon upgrade - apply to specific weapon
		ASurvivorWeapon* WeaponActor = GetWeaponActor(TargetWeapon);
		if (WeaponActor)
		{
			WeaponActor->ApplyStatUpgrade(Effect.WeaponStat, Effect.AdditiveBonus, Effect.MultiplicativeBonus);
		}
	}
	else
	{
		// Global upgrade - apply to all weapons that use this stat
		for (auto& Pair : WeaponStates)
		{
			ASurvivorWeapon* WeaponActor = Pair.Value.WeaponActor.Get();
			if (WeaponActor && WeaponActor->UsesStat(Effect.WeaponStat))
			{
				WeaponActor->ApplyStatUpgrade(Effect.WeaponStat, Effect.AdditiveBonus, Effect.MultiplicativeBonus);
			}
		}
	}
}

void UUpgradeSubsystem::ApplyNewWeaponUpgrade(UWeaponDataBase* WeaponToGrant)
{
	ASurvivorCharacter* Player = PlayerCharacter.Get();
	if (!Player || !WeaponToGrant)
	{
		return;
	}

	// Tell the character to spawn the new weapon
	Player->AddWeapon(WeaponToGrant);
}

void UUpgradeSubsystem::IncrementWeaponLevel(UWeaponDataBase* WeaponData)
{
	if (!WeaponData)
	{
		return;
	}

	FWeaponUpgradeState* State = WeaponStates.Find(WeaponData->WeaponID);
	if (State)
	{
		State->Level++;
		UE_LOG(LogTemp, Log, TEXT("UpgradeSubsystem: Weapon '%s' leveled up to %d"),
			*WeaponData->WeaponID.ToString(), State->Level);
	}
}

int32 UUpgradeSubsystem::GetUpgradeStacks(FName UpgradeID) const
{
	const int32* Stacks = OwnedUpgradeStacks.Find(UpgradeID);
	return Stacks ? *Stacks : 0;
}

bool UUpgradeSubsystem::HasUpgrade(FName UpgradeID) const
{
	return GetUpgradeStacks(UpgradeID) > 0;
}

int32 UUpgradeSubsystem::GetWeaponLevel(const UWeaponDataBase* WeaponData) const
{
	if (!WeaponData)
	{
		return 0;
	}

	const FWeaponUpgradeState* State = WeaponStates.Find(WeaponData->WeaponID);
	return State ? State->Level : 0;
}

bool UUpgradeSubsystem::HasWeapon(const UWeaponDataBase* WeaponData) const
{
	if (!WeaponData)
	{
		return false;
	}
	return WeaponStates.Contains(WeaponData->WeaponID);
}

TArray<UWeaponDataBase*> UUpgradeSubsystem::GetOwnedWeapons() const
{
	TArray<UWeaponDataBase*> Result;
	for (const auto& Pair : WeaponStates)
	{
		if (Pair.Value.WeaponData)
		{
			Result.Add(Pair.Value.WeaponData);
		}
	}
	return Result;
}

TArray<UWeaponDataBase*> UUpgradeSubsystem::GetWeaponsUsingStat(EWeaponStat Stat) const
{
	TArray<UWeaponDataBase*> Result;
	for (const auto& Pair : WeaponStates)
	{
		ASurvivorWeapon* WeaponActor = Pair.Value.WeaponActor.Get();
		if (WeaponActor && WeaponActor->UsesStat(Stat))
		{
			Result.Add(Pair.Value.WeaponData);
		}
	}
	return Result;
}

FText UUpgradeSubsystem::GetWeaponCountForStat(EWeaponStat Stat) const
{
	int32 UsingCount = GetWeaponsUsingStat(Stat).Num();
	int32 TotalCount = GetCurrentWeaponCount();

	return FText::FromString(FString::Printf(TEXT("%d/%d"), UsingCount, TotalCount));
}

int32 UUpgradeSubsystem::GetTotalWeaponSlots() const
{
	// This matches MaxWeaponSlots in SurvivorCharacter
	return 4;
}

int32 UUpgradeSubsystem::GetCurrentWeaponCount() const
{
	return WeaponStates.Num();
}

void UUpgradeSubsystem::RegisterWeapon(UWeaponDataBase* WeaponData, ASurvivorWeapon* WeaponActor)
{
	if (!WeaponData)
	{
		return;
	}

	FWeaponUpgradeState& State = WeaponStates.FindOrAdd(WeaponData->WeaponID);
	State.WeaponData = WeaponData;
	State.WeaponActor = WeaponActor;
	State.Level = 1;

	UE_LOG(LogTemp, Log, TEXT("UpgradeSubsystem: Registered weapon '%s'"), *WeaponData->WeaponID.ToString());
}

ASurvivorWeapon* UUpgradeSubsystem::GetWeaponActor(const UWeaponDataBase* WeaponData) const
{
	if (!WeaponData)
	{
		return nullptr;
	}

	const FWeaponUpgradeState* State = WeaponStates.Find(WeaponData->WeaponID);
	return State ? State->WeaponActor.Get() : nullptr;
}

void UUpgradeSubsystem::TriggerUpgradeSelection()
{
	CurrentPlayerLevel++;

	TArray<UUpgradeDataAsset*> Choices = GetRandomUpgradeChoices(3);

	if (Choices.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("UpgradeSubsystem: Triggering upgrade selection with %d choices"), Choices.Num());
		OnShowUpgradeSelection.Broadcast(Choices);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UpgradeSubsystem: No upgrades available for selection!"));
	}
}
