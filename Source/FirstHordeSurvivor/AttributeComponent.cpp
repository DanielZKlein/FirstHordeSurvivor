#include "AttributeComponent.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default values
	MaxHealth = FGameplayAttribute(100.f);
	HealthRegen = FGameplayAttribute(1.f);
	MaxSpeed = FGameplayAttribute(600.f);
	MaxAcceleration = FGameplayAttribute(2000.f);
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize Health
	CurrentHealth = MaxHealth.GetCurrentValue();

	StartRegen();
}

void UAttributeComponent::StartRegen()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_Regen, this, &UAttributeComponent::ApplyRegen, 1.0f, true);
	}
}

void UAttributeComponent::StopRegen()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Regen);
	}
}

void UAttributeComponent::ApplyRegen()
{
	float RegenAmount = HealthRegen.GetCurrentValue();
	if (RegenAmount != 0.f)
	{
		ApplyHealthChange(RegenAmount);
	}
}

bool UAttributeComponent::ApplyHealthChange(float Delta)
{
    // I-Frame Check for Damage (player only)
    if (bUseInvulnerability && Delta < 0.0f && bIsInvulnerable)
    {
        return false;
    }

	float OldHealth = CurrentHealth;
	float MaxHP = MaxHealth.GetCurrentValue();

	CurrentHealth = FMath::Clamp(CurrentHealth + Delta, 0.0f, MaxHP);

	float ActualDelta = CurrentHealth - OldHealth;

	if (ActualDelta != 0.0f)
	{
        // Apply I-Frames if we took damage (player only)
        if (bUseInvulnerability && ActualDelta < 0.0f && InvulnerabilityDuration > 0.0f)
        {
            bIsInvulnerable = true;
            if (GetWorld())
            {
                GetWorld()->GetTimerManager().SetTimer(TimerHandle_Invulnerability, this, &UAttributeComponent::EndInvulnerability, InvulnerabilityDuration, false);
            }
        }

		// Broadcast change. Using 'true' for bIsResultOfEditorChange is a bit hacky, 
		// maybe we should update the delegate signature later, but for now it works as a signal.
		// Actually, let's just say 'false' as it's game logic.
		OnHealthChanged.Broadcast(this, false); 

        if (CurrentHealth <= 0.0f)
        {
            OnDeath.Broadcast(this, false);
        }

		return true;
	}

	return false;
}

void UAttributeComponent::EndInvulnerability()
{
    bIsInvulnerable = false;
}

float UAttributeComponent::GetCurrentHealth() const
{
	return CurrentHealth;
}

float UAttributeComponent::GetAttributeValue(const FGameplayAttribute& Attribute) const
{
	return Attribute.GetCurrentValue();
}

void UAttributeComponent::SetBaseValue(FGameplayAttribute& Attribute, float NewBaseValue)
{
	Attribute.BaseValue = NewBaseValue;
	OnAttributeChanged.Broadcast(this, false);
}

void UAttributeComponent::ApplyAdditive(FGameplayAttribute& Attribute, float Bonus)
{
	Attribute.Additive += Bonus;
	OnAttributeChanged.Broadcast(this, false);
}

void UAttributeComponent::ApplyMultiplicative(FGameplayAttribute& Attribute, float Multiplier)
{
	Attribute.Multiplicative += Multiplier;
	OnAttributeChanged.Broadcast(this, false);
}
