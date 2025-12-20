#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivorProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class FIRSTHORDESURVIVOR_API ASurvivorProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	ASurvivorProjectile();

protected:
	virtual void BeginPlay() override;

public:	
    // Initialize with specific data
    void Initialize(float Speed, float DamageAmount, float Range);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* SphereComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProjectileMovementComponent* MovementComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* TrailComp;

    // Properties
    float Damage;
    float MaxRange;
    FVector StartLocation;

    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    USoundBase* HitSound;

    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    UNiagaraSystem* HitVFX;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    
public:
    virtual void Tick(float DeltaTime) override;
};
