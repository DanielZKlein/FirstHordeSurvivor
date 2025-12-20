#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XPGem.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EXPGemState : uint8
{
	Inactive,
	Spawning,
	Idle,
	Fleeing,      // Brief dramatic push away from player when entering pickup range
	Magnetizing,
	Collected
};

USTRUCT(BlueprintType)
struct FXPGemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;

	// Multiplier for emissive brightness (higher = brighter glow)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EmissiveStrength = 20.0f;

	// Point light intensity (how bright the light is)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LightIntensity = 1000.0f;

	// Point light radius (how far the light reaches)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LightRadius = 200.0f;

    // Optional particle system for high value gems
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class UNiagaraSystem* TrailEffect;
};

UCLASS()
class FIRSTHORDESURVIVOR_API AXPGem : public AActor
{
	GENERATED_BODY()
	
public:	
	AXPGem();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	void Initialize(int32 InValue, const FVector& StartLocation);
    void SetVisuals(const FXPGemData& VisualData);

    // Called when returned to pool
    void Deactivate();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* TrailComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPointLightComponent* LightComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 XPValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EXPGemState State;

    // Movement params
    FVector Velocity;
    float CurrentSpeed;
    
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float FlyAwayForce;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float MagnetAcceleration;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float MaxSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float CollectDistance;

    // Time to wait before magnetizing (after spawn)
    float SpawnTimer;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float SpawnDuration;

    // Flee away from player before magnetizing
    float FleeTimer;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float FleeDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float FleeForce;

    // Reference to player
    UPROPERTY()
    AActor* TargetActor;

    // Helper to find player
    void FindTarget();
};
