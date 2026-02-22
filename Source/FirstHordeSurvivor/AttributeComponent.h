#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

// Delegate for when any attribute changes. 
// We keep it simple: "Something changed, please refresh."
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttributeChanged, UAttributeComponent*, Component, bool, bIsResultOfEditorChange);

USTRUCT(BlueprintType)
struct FGameplayAttribute
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float BaseValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float Multiplicative;

	FGameplayAttribute()
		: BaseValue(0.f)
		, Additive(0.f)
		, Multiplicative(1.f)
	{}

	FGameplayAttribute(float InBaseValue)
		: BaseValue(InBaseValue)
		, Additive(0.f)
		, Multiplicative(1.f)
	{}

	float GetCurrentValue() const
	{
		return (BaseValue + Additive) * Multiplicative;
	}
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FIRSTHORDESURVIVOR_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAttributeComponent();

protected:
	virtual void BeginPlay() override;

public:	
	// Attributes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute HealthRegen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute MaxAcceleration;

	// Movement control - affects deceleration, direction change responsiveness, air control
	// Base value 1.0 = normal, higher = more responsive, lower = more sluggish
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute MovementControl;

	// Armor - flat damage reduction (take X less damage from each attack)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute Armor;

	// Impact - compound stat affecting: thorns damage, contact knockback, % damage resist
	// Scales with MaxHealth, Armor, and current speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FGameplayAttribute Impact;

	// ===== Impact Calculation Helpers =====

	// Calculate thorns damage dealt to enemies on contact
	// Scales with Impact, MaxHealth, Armor, and current speed
	UFUNCTION(BlueprintCallable, Category = "Attributes|Impact")
	float GetThornsDamage(float CurrentSpeed) const;

	// Calculate knockback force applied to enemies on contact
	// Scales with Impact and current speed
	UFUNCTION(BlueprintCallable, Category = "Attributes|Impact")
	float GetContactKnockback(float CurrentSpeed) const;

	// Calculate percentage damage resistance from Impact
	// Returns value between 0.0 and some maximum (not capped at 1.0 to allow design flexibility)
	UFUNCTION(BlueprintCallable, Category = "Attributes|Impact")
	float GetDamageResistPercent() const;

	// Apply damage with Armor reduction
	// Returns the actual damage dealt after armor
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float ApplyArmoredDamage(float IncomingDamage);

	// Delegate fired when any attribute is modified via the setter functions
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnAttributeChanged OnAttributeChanged;

	// Helper to apply changes (optional, but good for encapsulation if we expand later)
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetAttributeValue(const FGameplayAttribute& Attribute) const;

	// Setters that trigger the OnAttributeChanged delegate
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetBaseValue(UPARAM(ref) FGameplayAttribute& Attribute, float NewBaseValue);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyAdditive(UPARAM(ref) FGameplayAttribute& Attribute, float Bonus);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyMultiplicative(UPARAM(ref) FGameplayAttribute& Attribute, float Multiplier);

protected:
	FTimerHandle TimerHandle_Regen;

	void ApplyRegen();

public:
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void StartRegen();

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void StopRegen();

	// Health State
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	float CurrentHealth;

public:
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnAttributeChanged OnHealthChanged; // Reusing the same delegate signature for simplicity, or we can make a new one

    // Delegate fired when health reaches 0
    UPROPERTY(BlueprintAssignable, Category = "Attributes")
    FOnAttributeChanged OnDeath;

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	bool ApplyHealthChange(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetCurrentHealth() const;

	// Invulnerability (set to false for enemies - they shouldn't have i-frames)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Attributes")
	bool bUseInvulnerability = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Attributes", meta = (EditCondition = "bUseInvulnerability"))
	float InvulnerabilityDuration = 0.5f;

protected:
    bool bIsInvulnerable = false;
    FTimerHandle TimerHandle_Invulnerability;

    void EndInvulnerability();
};
