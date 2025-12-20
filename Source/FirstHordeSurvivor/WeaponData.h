#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

class ASurvivorProjectile;
class UNiagaraSystem;
class USoundBase;

/**
 * DataAsset to configure weapon properties
 */
UCLASS()
class FIRSTHORDESURVIVOR_API UWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    TSubclassOf<ASurvivorProjectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    float RPM = 60.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    float MaxRange = 1000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    float Damage = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    float ProjectileSpeed = 1000.0f;

    // Cone angle in degrees for random spread
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    float Precision = 5.0f;

    // Targeting Weights
    
    // Weight for distance (Higher = prefer closer/farther depending on sign?)
    // Let's standardize: Higher Score = Better Target.
    // If we want closest, we want Small Distance to be High Score.
    // Score = (Distance * RangeWeight) + ...
    // If RangeWeight is negative, larger distance = lower score. So Negative = Prefer Close.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
    float RangeWeight = -1.0f;

    // Weight for being in front of player velocity (Dot Product -1 to 1)
    // Positive = Prefer In Front.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
    float InFrontWeight = 1000.0f;

    // Visuals
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    USoundBase* ShootSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem* ShootVFX;
};
