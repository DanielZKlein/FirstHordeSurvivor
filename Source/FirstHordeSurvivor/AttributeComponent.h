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

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes")
    float InvulnerabilityDuration = 0.5f;

    bool bIsInvulnerable;
    FTimerHandle TimerHandle_Invulnerability;

    void EndInvulnerability();
};
