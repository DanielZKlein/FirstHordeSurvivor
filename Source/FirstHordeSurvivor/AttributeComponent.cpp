#include "AttributeComponent.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default values
	MaxHealth = FGameplayAttribute(100.f);
	HealthRegen = FGameplayAttribute(0.f);
	MaxSpeed = FGameplayAttribute(600.f);
	MaxAcceleration = FGameplayAttribute(2000.f);

	// New stats
	MovementControl = FGameplayAttribute(1.0f);  // 1.0 = normal responsiveness
	Armor = FGameplayAttribute(0.0f);            // No flat damage reduction by default
	Impact = FGameplayAttribute(0.0f);           // No impact bonuses by default
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

float UAttributeComponent::GetThornsDamage(float CurrentSpeed) const
{
	// Thorns damage scales with:
	// - Impact stat (primary scaling)
	// - MaxHealth (tanky characters deal more thorns)
	// - Armor (armored characters deal more thorns)
	// - Current speed (faster = more damage)

	float ImpactValue = Impact.GetCurrentValue();
	if (ImpactValue <= 0.0f)
	{
		return 0.0f;
	}

	float MaxHP = MaxHealth.GetCurrentValue();
	float ArmorValue = Armor.GetCurrentValue();

	// Base thorns = Impact * (1 + HP/100 + Armor/10)
	// Speed multiplier adds up to 50% bonus at max speed
	float SpeedMultiplier = FMath::Max(600.0f, MaxSpeed.GetCurrentValue());
	float SpeedBonus = 1.0f + 0.5f * FMath::Clamp(CurrentSpeed / SpeedMultiplier, 0.0f, 1.0f);

	float BaseThorns = ImpactValue * (1.0f + MaxHP / 100.0f + ArmorValue / 10.0f);

	return BaseThorns * SpeedBonus;
}

float UAttributeComponent::GetContactKnockback(float CurrentSpeed) const
{
	// Knockback scales with:
	// - Impact stat (primary scaling)
	// - Current speed (faster = more knockback)

	float ImpactValue = Impact.GetCurrentValue();
	if (ImpactValue <= 0.0f)
	{
		return 0.0f;
	}

	// Base knockback force, scales linearly with speed
	float SpeedMultiplier = FMath::Max(600.0f, MaxSpeed.GetCurrentValue());
	float SpeedFactor = FMath::Clamp(CurrentSpeed / SpeedMultiplier, 0.0f, 1.5f);

	return ImpactValue * 100.0f * (1.0f + SpeedFactor);
}

float UAttributeComponent::GetDamageResistPercent() const
{
	// Damage resist from Impact
	// Each point of Impact gives 1% damage resist
	// Not capped - design can decide the maximum Impact achievable

	float ImpactValue = Impact.GetCurrentValue();
	return ImpactValue * 0.01f;  // 1% per point
}

float UAttributeComponent::ApplyArmoredDamage(float IncomingDamage)
{
	if (IncomingDamage <= 0.0f)
	{
		return 0.0f;
	}

	// Apply flat armor reduction
	float ArmorValue = Armor.GetCurrentValue();
	float ReducedDamage = FMath::Max(0.0f, IncomingDamage - ArmorValue);

	// Apply Impact percentage damage resist
	float DamageResist = GetDamageResistPercent();
	float FinalDamage = ReducedDamage * (1.0f - FMath::Clamp(DamageResist, 0.0f, 0.9f));  // Cap at 90% resist

	// Minimum 1 damage if any damage was incoming (prevent complete immunity)
	if (IncomingDamage > 0.0f && FinalDamage < 1.0f)
	{
		FinalDamage = 1.0f;
	}

	// Apply the damage
	ApplyHealthChange(-FinalDamage);

	return FinalDamage;
}
