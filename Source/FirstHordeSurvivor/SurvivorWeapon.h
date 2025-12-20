#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivorWeapon.generated.h"

class UWeaponData;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	ASurvivorWeapon();

protected:
	virtual void BeginPlay() override;

public:	
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UWeaponData* WeaponData;

    void StartShooting();
    void StopShooting();

protected:
    FTimerHandle TimerHandle_Attack;

    void Fire();
    AActor* FindBestTarget();
};
