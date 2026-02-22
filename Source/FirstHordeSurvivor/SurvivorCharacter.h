#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AttributeComponent.h"
#include "InputActionValue.h"
#include "SurvivorCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UAudioComponent;
class UCurveFloat;
class UWeaponDataBase;
class ASurvivorWeapon;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAttributeComponent* AttributeComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class UStaticMeshComponent* PlayerVisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* RollingAudioComp;

	// Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	// Audio Settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UCurveFloat* SpeedToVolumeCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FName SpeedParameterName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* WallBounceSound;

    // Weapons
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TSubclassOf<class ASurvivorWeapon> WeaponClass; // Or Data? 
    // Plan said: UPROPERTY(EditDefaultsOnly) UWeaponData* StartingWeapon
    // But we need the Weapon Actor Class to spawn too. 
    // Let's assume we spawn a generic ASurvivorWeapon and give it the Data.
    
    UPROPERTY(EditAnywhere, Category = "Combat")
    TObjectPtr<UWeaponDataBase> StartingWeaponData;

	// ===== Multi-Weapon Support =====

	// Maximum number of weapons the player can have
	static constexpr int32 MaxWeaponSlots = 4;

	// All weapons currently owned by the player
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<ASurvivorWeapon*> OwnedWeapons;

	// Add a new weapon to the player (returns nullptr if at max capacity)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	ASurvivorWeapon* AddWeapon(UWeaponDataBase* WeaponData);

	// Remove a weapon from the player
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RemoveWeapon(ASurvivorWeapon* Weapon);

	// Check if player can add another weapon
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAddWeapon() const { return OwnedWeapons.Num() < MaxWeaponSlots; }

	// Get current weapon count
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetWeaponSlotCount() const { return OwnedWeapons.Num(); }

	// Get max weapon slots
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetMaxWeaponSlots() const { return MaxWeaponSlots; }

	// Helper to apply attributes to CharacterMovement
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyMovementAttributes();

	// You might want to call this when attributes change (e.g. via a delegate in the future)
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void OnAttributeChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange);

    // Debug
    UFUNCTION(Exec)
    void DebugKillNearby();

    // XP System
    UFUNCTION(BlueprintCallable, Category = "XP")
    void AddXP(int32 Amount);

    UFUNCTION(BlueprintPure, Category = "XP")
    int32 GetXPForLevel(int32 Level) const;

    UFUNCTION(BlueprintPure, Category = "XP")
    int32 GetXPForCurrentLevel() const;

    UFUNCTION(BlueprintPure, Category = "XP")
    float GetLevelProgress() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
    float PickupRange = 500.0f;

    // XP Curve Parameters - tune these to adjust leveling speed
    // Formula: XP = Base * Level^Exponent + Linear * Level
    UPROPERTY(EditDefaultsOnly, Category = "XP|Curve")
    float XPCurveBase = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "XP|Curve")
    float XPCurveExponent = 1.7f;

    UPROPERTY(EditDefaultsOnly, Category = "XP|Curve")
    float XPCurveLinear = 10.0f;

protected:
	void Move(const FInputActionValue& Value);

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnHealthChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange);

	// Event for Blueprint to update UI
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnHealthUpdated();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnXPAdded(int32 NewXP, int32 Level, float LevelProgress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnLevelUp(int32 NewLevel);

protected:
	// Legacy support for UI
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Legacy")
	float CurHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Legacy")
	float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XP")
    int32 CurrentXP = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XP")
    int32 CurrentLevel = 1;

private:
	FVector LastFrameVelocity;
};
