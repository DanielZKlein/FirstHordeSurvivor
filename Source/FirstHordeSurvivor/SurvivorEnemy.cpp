#include "SurvivorEnemy.h"
#include "SurvivorCharacter.h"
#include "EnemyData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "XPGemSubsystem.h"
#include "EnemySpawnSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"

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

	// Health bar widget
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComp"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(100.0f, 10.0f));

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

	// Look up enemy data from DataTable
	if (EnemyDataTable && !EnemyRowName.IsNone())
	{
		EnemyData = EnemyDataTable->FindRow<FEnemyTableRow>(EnemyRowName, TEXT("EnemyLookup"));
	}

	// Validate EnemyData
	if (!EnemyData)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyData row '%s' not found in %s! Damage and Stats will not work."), *EnemyRowName.ToString(), *GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("ERROR: EnemyData row '%s' missing on %s!"), *EnemyRowName.ToString(), *GetName()));
		}
	}

	// Initialize Stats
	InitializeFromData();

    // Bind Death and Health Changed
    if (AttributeComp)
    {
        AttributeComp->OnDeath.AddDynamic(this, &ASurvivorEnemy::OnDeath);
        AttributeComp->OnHealthChanged.AddDynamic(this, &ASurvivorEnemy::OnHealthChanged);

        // Enemies don't regenerate health
        AttributeComp->StopRegen();

        // Initialize last known health for hit flash detection
        LastKnownHealth = AttributeComp->GetCurrentHealth();
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
			UMaterialInterface* BaseMaterial = EnemyData->EnemyMaterial.LoadSynchronous();
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), EnemyData->EnemyColor);
			DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), EnemyData->EmissiveStrength);
			DynamicMaterial->SetScalarParameterValue(TEXT("HitFlashIntensity"), 0.0f);
			EnemyMeshComp->SetMaterial(0, DynamicMaterial);
		}

		// Apply mesh scale
		EnemyMeshComp->SetWorldScale3D(FVector(EnemyData->MeshScale));

		// Render to custom depth for post-process outline
		EnemyMeshComp->SetRenderCustomDepth(true);

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

	// Decay hit flash (with hold period)
	if (HitFlashIntensity > 0.0f)
	{
		if (HitFlashHoldTimer > 0.0f)
		{
			// Still holding at full intensity
			HitFlashHoldTimer -= DeltaTime;
		}
		else
		{
			// Decay after hold period
			HitFlashIntensity = FMath::Max(0.0f, HitFlashIntensity - DeltaTime * HitFlashDecayRate);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetScalarParameterValue(TEXT("HitFlashIntensity"), HitFlashIntensity);
			}
		}
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

    // Return to pool via subsystem
    if (UWorld* World = GetWorld())
    {
        if (UEnemySpawnSubsystem* SpawnSubsystem = World->GetSubsystem<UEnemySpawnSubsystem>())
        {
            SpawnSubsystem->OnEnemyDeath(this);
            return;
        }
    }

    // Fallback: Destroy if no subsystem (e.g., manually placed enemies)
    SetLifeSpan(0.1f);
}

void ASurvivorEnemy::OnHealthChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange)
{
	float CurrentHealth = AttributeComp->GetCurrentHealth();

	// Only trigger hit flash if health DECREASED (took damage)
	if (CurrentHealth < LastKnownHealth)
	{
		HitFlashIntensity = 1.0f;
		HitFlashHoldTimer = HitFlashHoldDuration;
		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(TEXT("HitFlashIntensity"), HitFlashIntensity);
		}
	}

	// Update last known health
	LastKnownHealth = CurrentHealth;

	// Update health bar
	if (HealthBarComp && AttributeComp)
	{
		UUserWidget* Widget = HealthBarComp->GetWidget();
		if (Widget)
		{
			UProgressBar* ProgressBar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("HealthBar")));
			if (ProgressBar)
			{
				float HealthPercent = AttributeComp->GetCurrentHealth() / AttributeComp->MaxHealth.GetCurrentValue();
				ProgressBar->SetPercent(HealthPercent);
			}
		}
	}
}

void ASurvivorEnemy::Deactivate()
{
	// Hide and disable
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	// Stop movement
	GetCharacterMovement()->StopMovementImmediately();

	// Stop attack timer
	StopAttackTimer();

	// Hide health bar
	if (HealthBarComp)
	{
		HealthBarComp->SetVisibility(false);
	}

	// Reset state
	bIsOverlappingPlayer = false;
	TargetPlayer = nullptr;
}

void ASurvivorEnemy::Reinitialize(UDataTable* DataTable, FName RowName, FVector Location)
{
	// Update data references
	EnemyDataTable = DataTable;
	EnemyRowName = RowName;

	// Look up new row data
	if (EnemyDataTable && !EnemyRowName.IsNone())
	{
		EnemyData = EnemyDataTable->FindRow<FEnemyTableRow>(EnemyRowName, TEXT("EnemyReinit"));
	}

	// Re-enable actor
	SetActorLocation(Location);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);

	// Re-enable collision on capsule
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Re-enable movement
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// Apply visuals and stats from data
	InitializeFromData();

	// Reset health to full
	if (AttributeComp && EnemyData)
	{
		AttributeComp->MaxHealth.BaseValue = EnemyData->BaseHealth;
		// Heal to full by applying max health as change
		float MaxHP = AttributeComp->MaxHealth.GetCurrentValue();
		AttributeComp->ApplyHealthChange(MaxHP);
		LastKnownHealth = AttributeComp->GetCurrentHealth();
	}

	// Reset hit flash
	HitFlashIntensity = 0.0f;
	HitFlashHoldTimer = 0.0f;
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("HitFlashIntensity"), 0.0f);
	}

	// Show health bar
	if (HealthBarComp)
	{
		HealthBarComp->SetVisibility(true);
	}

	// Find player target
	TargetPlayer = Cast<ASurvivorCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}
