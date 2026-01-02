#include "SurvivorEnemy.h"
#include "SurvivorCharacter.h"
#include "EnemyData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Components/StaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "XPGemSubsystem.h"

ASurvivorEnemy::ASurvivorEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	AttributeComp = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComp"));

	EnemyMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMeshComp"));
	EnemyMeshComp->SetupAttachment(GetCapsuleComponent());

	AttackOverlapComp = CreateDefaultSubobject<USphereComponent>(TEXT("AttackOverlapComp"));
	AttackOverlapComp->SetupAttachment(RootComponent);
	AttackOverlapComp->SetSphereRadius(150.0f); // Larger than capsule to ensure overlap before block
	AttackOverlapComp->SetCollisionProfileName("OverlapAllDynamic"); // Overlap everything
	AttackOverlapComp->SetGenerateOverlapEvents(true);

	// Configure collisions
	GetCapsuleComponent()->SetCollisionProfileName("Pawn");
	
	// We want the overlap event to trigger attacks
	AttackOverlapComp->OnComponentBeginOverlap.AddDynamic(this, &ASurvivorEnemy::OnOverlapBegin);
	AttackOverlapComp->OnComponentEndOverlap.AddDynamic(this, &ASurvivorEnemy::OnOverlapEnd);

	// Default Movement settings
	GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 1000.0f, 0.0f); // Fast rotation to face RVO velocity
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
    
    // Avoidance
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetCharacterMovement()->AvoidanceWeight = 0.5f;
}

void ASurvivorEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Find Player (Simple version for now, assume single player)
	TargetPlayer = Cast<ASurvivorCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

	// Validate EnemyData
	if (!EnemyData)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyData is NOT assigned in %s! Damage and Stats will not work."), *GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("ERROR: EnemyData is Missing on %s!"), *GetName()));
		}
	}

	// Initialize Stats
	InitializeFromData();

    // Bind Death
    if (AttributeComp)
    {
        AttributeComp->OnDeath.AddDynamic(this, &ASurvivorEnemy::OnDeath);
    }
}

void ASurvivorEnemy::InitializeFromData()
{
	if (EnemyData)
	{
		// Load Mesh (Synchronous load for simplicity in this phase, async is better for production)
		if (!EnemyData->EnemyMesh.IsNull())
		{
			EnemyMeshComp->SetStaticMesh(EnemyData->EnemyMesh.LoadSynchronous());
		}
		if (!EnemyData->EnemyMaterial.IsNull())
		{
			EnemyMeshComp->SetMaterial(0, EnemyData->EnemyMaterial.LoadSynchronous());
		}

		// Apply mesh scale
		EnemyMeshComp->SetWorldScale3D(FVector(EnemyData->MeshScale));

		// Apply Stats
		AttributeComp->MaxHealth.BaseValue = EnemyData->BaseHealth;
		AttributeComp->MaxSpeed.BaseValue = EnemyData->MoveSpeed;
		
		// Apply Speed to Movement Component
		GetCharacterMovement()->MaxWalkSpeed = AttributeComp->MaxSpeed.GetCurrentValue();
		
		// Initialize Health
		AttributeComp->ApplyHealthChange(0); // Just to ensure clamping/init if needed, though BeginPlay of Comp handles it
	}
}

void ASurvivorEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Simple Chase Logic
	// We don't use AIController for thousands of units usually, but for v0.1 simple AddMovementInput is fine
	// or SimpleMoveToActor if we had a controller.
	// Let's use direct vector movement for "dumb" chasing which is cheap and effective for hordes.
	
	if (TargetPlayer)
	{
		FVector Direction = (TargetPlayer->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		
		// Flatten Z
		Direction.Z = 0.f;

		AddMovementInput(Direction);
		
		// Optional: Force rotation if AddMovementInput doesn't handle it fast enough
		SetActorRotation(Direction.Rotation());
	}
}

void ASurvivorEnemy::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == TargetPlayer)
	{
		UE_LOG(LogTemp, Log, TEXT("OVERLAP BEGIN: %s overlapped with Player!"), *GetName());
		bIsOverlappingPlayer = true;
		StartAttackTimer();
	}
}

void ASurvivorEnemy::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == TargetPlayer)
	{
		UE_LOG(LogTemp, Log, TEXT("OVERLAP END: %s stopped overlapping Player."), *GetName());
		bIsOverlappingPlayer = false;
		StopAttackTimer();
	}
}

void ASurvivorEnemy::StartAttackTimer()
{
	UE_LOG(LogTemp, Log, TEXT("STARTING ATTACK TIMER on %s"), *GetName());
	// Attack immediately then loop
	AttackPlayer();
	GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &ASurvivorEnemy::AttackPlayer, 1.0f, true);
}

void ASurvivorEnemy::StopAttackTimer()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Attack);
}

void ASurvivorEnemy::AttackPlayer()
{
	UE_LOG(LogTemp, Log, TEXT("ATTEMPTING ATTACK by %s"), *GetName());
	
	if (TargetPlayer && AttributeComp && EnemyData)
	{
		// Apply Damage
		// We need to access the Player's AttributeComponent
		UAttributeComponent* PlayerAttributes = TargetPlayer->AttributeComp;
		if (PlayerAttributes)
		{
			bool bSuccess = PlayerAttributes->ApplyHealthChange(-EnemyData->BaseDamage);
			UE_LOG(LogTemp, Warning, TEXT("Attack Applied! Damage: %f | New Health: %f | Success: %d"), 
				EnemyData->BaseDamage, PlayerAttributes->GetCurrentHealth(), bSuccess);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Attack Failed: Player has no AttributeComponent!"));
		}
	}
	else
	{
		// Debug failure
		if (!EnemyData) UE_LOG(LogTemp, Error, TEXT("Attack failed: EnemyData is NULL"));
		if (!TargetPlayer) UE_LOG(LogTemp, Error, TEXT("Attack failed: TargetPlayer is NULL"));
		if (!AttributeComp) UE_LOG(LogTemp, Error, TEXT("Attack failed: My AttributeComp is NULL"));
	}
}

void ASurvivorEnemy::OnDeath(UAttributeComponent* Component, bool bIsResultOfEditorChange)
{
    // Stop attacking
    StopAttackTimer();
    
    // Disable collision and movement
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    
    // Spawn Gems
    if (EnemyData)
    {
        if (UWorld* World = GetWorld())
        {
            if (UXPGemSubsystem* GemSubsystem = World->GetSubsystem<UXPGemSubsystem>())
            {
                int32 XPToDrop = FMath::RandRange(EnemyData->MinXP, EnemyData->MaxXP);
                
                // Greedy algorithm to find fewest gems
                // Gem Tiers: 100, 50, 20, 5, 1
                TArray<int32> GemTiers = {100, 50, 20, 5, 1};
                
                for (int32 TierValue : GemTiers)
                {
                    while (XPToDrop >= TierValue)
                    {
                        // Spawn this gem
                        FVector SpawnLoc = GetActorLocation() + FMath::VRand() * 50.0f;
                        SpawnLoc.Z = GetActorLocation().Z; // Keep at same height roughly
                        
                        GemSubsystem->SpawnGem(SpawnLoc, TierValue);
                        
                        XPToDrop -= TierValue;
                    }
                }
            }
        }
    }

    // Destroy enemy (or play death anim then destroy)
    SetLifeSpan(0.1f);
}
