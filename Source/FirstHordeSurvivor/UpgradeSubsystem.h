#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UpgradeTypes.h"
#include "UpgradeSubsystem.generated.h"

class UUpgradeDataAsset;
class UDataTable;
class UWeaponDataBase;
class ASurvivorWeapon;
class ASurvivorCharacter;

// Delegate for when upgrade selection UI should be shown
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowUpgradeSelection, const TArray<UUpgradeDataAsset*>&, AvailableUpgrades);

// Delegate for when an upgrade is applied
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradeApplied, UUpgradeDataAsset*, AppliedUpgrade);

/**
 * Tracks the state of a weapon the player owns.
 */
USTRUCT(BlueprintType)
struct FWeaponUpgradeState
{
	GENERATED_BODY()

public:
	// The weapon data this state tracks
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UWeaponDataBase> WeaponData;

	// Current weapon level (increments with each upgrade applied to this weapon)
	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;

	// Reference to the spawned weapon actor
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<ASurvivorWeapon> WeaponActor;

	FWeaponUpgradeState()
		: Level(1)
	{}
};

/**
 * World subsystem managing the upgrade selection and application system.
 * Handles upgrade pool, selection rules, application, and state tracking.
 */
UCLASS()
class FIRSTHORDESURVIVOR_API UUpgradeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ===== Registration =====

	// Register the upgrade DataTable (called by GameMode)
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void RegisterUpgradeTable(UDataTable* InTable);

	// Register the player character (needed for weapon tracking and stat application)
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void RegisterPlayer(ASurvivorCharacter* InPlayer);

	// ===== Upgrade Selection =====

	// Get N random weighted upgrades from available pool
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	TArray<UUpgradeDataAsset*> GetRandomUpgradeChoices(int32 Count = 3);

	// Check if a specific upgrade is available to be offered
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	bool IsUpgradeAvailable(const UUpgradeDataAsset* Upgrade) const;

	// ===== Upgrade Application =====

	// Apply the selected upgrade (called when player makes a selection)
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void ApplyUpgrade(UUpgradeDataAsset* Upgrade);

	// ===== State Queries =====

	// Get how many times an upgrade has been taken
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetUpgradeStacks(FName UpgradeID) const;

	// Check if player owns a specific upgrade (at least 1 stack)
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	bool HasUpgrade(FName UpgradeID) const;

	// Get weapon level for a specific weapon
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetWeaponLevel(const UWeaponDataBase* WeaponData) const;

	// Check if player owns a specific weapon
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	bool HasWeapon(const UWeaponDataBase* WeaponData) const;

	// Get all owned weapons
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	TArray<UWeaponDataBase*> GetOwnedWeapons() const;

	// Get current player level
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetCurrentPlayerLevel() const { return CurrentPlayerLevel; }

	// ===== Weapon Applicability Queries (for UI) =====

	// Get all owned weapons that use a specific stat
	UFUNCTION(BlueprintCallable, Category = "Upgrades|UI")
	TArray<UWeaponDataBase*> GetWeaponsUsingStat(EWeaponStat Stat) const;

	// Get count of weapons using a stat (e.g., "2/4")
	UFUNCTION(BlueprintCallable, Category = "Upgrades|UI")
	FText GetWeaponCountForStat(EWeaponStat Stat) const;

	// Get total weapon slots
	UFUNCTION(BlueprintPure, Category = "Upgrades|UI")
	int32 GetTotalWeaponSlots() const;

	// Get current weapon count
	UFUNCTION(BlueprintPure, Category = "Upgrades|UI")
	int32 GetCurrentWeaponCount() const;

	// ===== Weapon Management =====

	// Called when a new weapon is granted to the player
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void RegisterWeapon(UWeaponDataBase* WeaponData, ASurvivorWeapon* WeaponActor);

	// Get the weapon actor for a specific weapon data
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	ASurvivorWeapon* GetWeaponActor(const UWeaponDataBase* WeaponData) const;

	// ===== Level-Up Trigger =====

	// Called when player levels up - triggers upgrade selection
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	void TriggerUpgradeSelection();

	// ===== Delegates =====

	// Fired when upgrade selection UI should be shown
	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnShowUpgradeSelection OnShowUpgradeSelection;

	// Fired when an upgrade is successfully applied
	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeApplied OnUpgradeApplied;

protected:
	// The registered upgrade table
	UPROPERTY()
	TObjectPtr<UDataTable> UpgradeTable;

	// Cached list of all registered upgrades
	UPROPERTY()
	TArray<UUpgradeDataAsset*> AllUpgrades;

	// Track how many times each upgrade has been taken (by UpgradeID)
	UPROPERTY()
	TMap<FName, int32> OwnedUpgradeStacks;

	// Track weapon states (owned weapons and their levels)
	UPROPERTY()
	TMap<FName, FWeaponUpgradeState> WeaponStates;  // Keyed by WeaponID

	// Reference to player character
	UPROPERTY()
	TWeakObjectPtr<ASurvivorCharacter> PlayerCharacter;

	// Current player level (for MinLevel checks)
	int32 CurrentPlayerLevel = 1;

	// ===== Internal Helpers =====

	void CacheUpgradesFromTable();

	// Availability checks
	bool MeetsPrerequisites(const UUpgradeDataAsset* Upgrade) const;
	bool MeetsLevelRequirement(const UUpgradeDataAsset* Upgrade) const;
	bool MeetsWeaponRequirement(const UUpgradeDataAsset* Upgrade) const;
	bool IsNotMaxedOut(const UUpgradeDataAsset* Upgrade) const;
	bool HasWeaponSlotAvailable() const;

	// Get effective weight for selection
	int32 GetEffectiveWeight(const UUpgradeDataAsset* Upgrade) const;

	// Apply different effect types
	void ApplyPlayerStatEffect(const struct FUpgradeEffect& Effect);
	void ApplyWeaponStatEffect(const struct FUpgradeEffect& Effect, UWeaponDataBase* TargetWeapon);
	void ApplyNewWeaponUpgrade(UWeaponDataBase* WeaponToGrant);

	// Increment weapon level when upgrade targets it
	void IncrementWeaponLevel(UWeaponDataBase* WeaponData);
};
