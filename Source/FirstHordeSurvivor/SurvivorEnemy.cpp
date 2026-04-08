#include "SurvivorEnemy.h"
#include "SurvivorCharacter.h"
#include "EnemyData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "XPGemSubsystem.h"
#include "EnemySpawnSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Engine/OverlapResult.h"

// Enemy separation tuning constants
namespace SeparationSettings
{
	// Radius within which we push away from other enemies
	constexpr float SeparationRadius = 150.0f;

	// Max speed added by separation force (units/sec) — gentle, just prevents full overlap
	constexpr float MaxSeparationSpeed = 400.0f;
}

// Knockback tuning constants
namespace KnockbackSettings
{
	// How fast knockback decays (units/sec lost per second)
	constexpr float FrictionDeceleration = 1000.0f;

	// Minimum velocity before knockback stops entirely
	constexpr float MinVelocityThreshold = 50.0f;

	// How much momentum the pusher retains after hitting another enemy (0.5 = 50%)
	constexpr float PusherMomentumRetention = 0.9f;

	// How much of the pusher's momentum transfers to the pushed enemy
	constexpr float MomentumTransferRatio = 0.95f;

	// Radius to check for enemy collisions during knockback
	constexpr float CollisionCheckRadius = 80.0f;

	// === Initial knockback scaling by enemy HP ===

	// HP at or below this gets full knockback (100%)
	constexpr float LightEnemyHP = 20.0f;

	// HP at or above this gets minimum knockback
	constexpr float HeavyEnemyHP = 500.0f;

	// Minimum knockback multiplier for heavy enemies (10% = 0.1)
	constexpr float MinKnockbackMultiplier = 0.1f;
}

// Crowd push tuning — enemies behind shove enemies in front toward the player
namespace CrowdPushSettings
{
	// Extra speed (units/sec) a fully-adjacent trailing enemy adds to the front enemy
	constexpr float MaxPushSpeed = 350.0f;

	// How fast push velocity decays when no longer being pushed (units/sec lost per sec)
	constexpr float PushDecay = 600.0f;
}

// Orbital approach tuning
namespace OrbitSettings
{
	// Distance at which enemies stop pushing toward the player.
	// Should be slightly less than AttackOverlapComp radius (150) so they're in attack range.
	constexpr float OrbitRadius = 120.0f;
}

ASurvivorEnemy::ASurvivorEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	AttributeComp = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComp"));
	AttributeComp->bUseInvulnerability = false;  // Enemies don't get i-frames

	EnemyMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMeshComp"));
	EnemyMeshComp->SetupAttachment(GetCapsuleComponent());
	// The mesh is visual-only. Disable all collision so it never blocks Pawns
	// (the capsule owns all physics interaction; AttackOverlapComp owns hit detection).
	EnemyMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
	// Let enemies pass through each other so they can't deadlock/clump.
	// Separation is handled by a lightweight per-tick push force instead.
	// ECR_Ignore (vs ECR_Overlap) tells CharacterMovementComponent's internal sweep tests
	// to completely skip other Pawns, eliminating steering/push interference.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// We want the overlap event to trigger attacks
	AttackOverlapComp->OnComponentBeginOverlap.AddDynamic(this, &ASurvivorEnemy::OnOverlapBegin);
	AttackOverlapComp->OnComponentEndOverlap.AddDynamic(this, &ASurvivorEnemy::OnOverlapEnd);

	// Default Movement settings
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 1000.0f, 0.0f); // Fast rotation to face RVO velocity
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;  // Critical: allow movement without AI controller

	// Avoidance
	GetCharacterMovement()->bUseRVOAvoidance = false;
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

	// Validate EnemyData (skip warning if row name is empty - likely being pooled/pre-warmed)
	if (!EnemyData && !EnemyRowName.IsNone())
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

	// Process knockback momentum and collisions
	ProcessKnockback(DeltaTime);

	// Apply crowd push velocity (from trailing enemies shoving us toward the player)
	if (!CrowdPushVelocity.IsNearlyZero(1.0f))
	{
		SetActorLocation(GetActorLocation() + CrowdPushVelocity * DeltaTime);
		float PushSpeed = CrowdPushVelocity.Size();
		float NewPushSpeed = FMath::Max(0.f, PushSpeed - CrowdPushSettings::PushDecay * DeltaTime);
		CrowdPushVelocity = NewPushSpeed > 1.f ? CrowdPushVelocity.GetSafeNormal() * NewPushSpeed : FVector::ZeroVector;
	}

	// --- Separation force ---
	// Find nearby enemies and add a gentle push away from each one.
	// Uses OverlapMultiByObjectType (Pawn object type) instead of OverlapMultiByChannel so
	// that the query is unaffected by the capsule's ECR_Ignore response to ECC_Pawn.
	// Channel queries respect the response matrix on both sides; object-type queries do not.
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(SeparationSettings::SeparationRadius);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		if (GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectQueryParams, Sphere, QueryParams))
		{
			FVector SeparationInput = FVector::ZeroVector;

			// Pre-compute my distance to the player (used for crowd push eligibility)
			float MyDistToPlayer = TargetPlayer
				? (GetActorLocation() - TargetPlayer->GetActorLocation()).Size2D()
				: MAX_FLT;

			for (const FOverlapResult& Hit : Overlaps)
			{
				ASurvivorEnemy* OtherEnemy = Cast<ASurvivorEnemy>(Hit.GetActor());
				if (!OtherEnemy)
				{
					continue;
				}

				FVector Away = GetActorLocation() - OtherEnemy->GetActorLocation();
				Away.Z = 0.0f;
				float Dist = Away.Size2D();

				if (Dist < KINDA_SMALL_NUMBER)
				{
					// Perfectly overlapping — push in a stable arbitrary direction
					Away = FVector(1.0f, 0.0f, 0.0f);
					Dist = 1.0f;
				}

				// Force scales linearly: strongest at dist=0, zero at SeparationRadius
				float Strength = 1.0f - FMath::Clamp(Dist / SeparationSettings::SeparationRadius, 0.0f, 1.0f);

				// Crowd push only when we're still approaching (outside orbit radius).
				// Once at the player, everyone spreads via normal separation instead of
				// shoving front-row enemies through the player into a blob.
				bool bOtherIsInFront = TargetPlayer &&
					MyDistToPlayer > OrbitSettings::OrbitRadius &&
					(OtherEnemy->GetActorLocation() - TargetPlayer->GetActorLocation()).Size2D() < MyDistToPlayer;

				if (bOtherIsInFront)
				{
					// Push them toward the player — trailing enemies shove the front row forward
					FVector ToPlayerFromOther = TargetPlayer->GetActorLocation() - OtherEnemy->GetActorLocation();
					ToPlayerFromOther.Z = 0.0f;
					FVector PushDir = ToPlayerFromOther.GetSafeNormal();
					OtherEnemy->AddCrowdPush(PushDir * Strength * CrowdPushSettings::MaxPushSpeed);
				}
				else
				{
					// Normal separation: push ourselves away from lateral/trailing enemies,
					// or from any neighbor when we're already at attack range
					SeparationInput += (Away / Dist) * Strength;
				}
			}

			if (!SeparationInput.IsNearlyZero())
			{
				// Normalise direction, scale to a fixed max speed, then feed as input
				// (AddInputVector is consumed by CharacterMovement and blended with walk speed)
				SeparationInput = SeparationInput.GetSafeNormal()
					* (SeparationSettings::MaxSeparationSpeed / GetCharacterMovement()->MaxWalkSpeed);
				GetCharacterMovement()->AddInputVector(SeparationInput);
			}
		}
	}

	// Simple Chase Logic
	// We don't use AIController for thousands of units usually, but for v0.1 simple AddMovementInput is fine
	// or SimpleMoveToActor if we had a controller.
	// Let's use direct vector movement for "dumb" chasing which is cheap and effective for hordes.

	if (TargetPlayer)
	{
		FVector ToPlayer = TargetPlayer->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.0f;
		float DistToPlayer = ToPlayer.Size2D();

		UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		if (MoveComp)
		{
			if (DistToPlayer > OrbitSettings::OrbitRadius)
			{
				// Outside orbit radius: chase toward player normally
				FVector Direction = ToPlayer / DistToPlayer;
				MoveComp->AddInputVector(Direction);
			}
			// Inside orbit radius: don't push further in.
			// The separation force handles lateral positioning naturally.
		}

		// Always face the player regardless of movement
		if (DistToPlayer > KINDA_SMALL_NUMBER)
		{
			FVector FaceDir = ToPlayer / DistToPlayer;
			SetActorRotation(FaceDir.Rotation());
		}
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
		bIsOverlappingPlayer = true;
		StartAttackTimer();
	}
}

void ASurvivorEnemy::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == TargetPlayer)
	{
		bIsOverlappingPlayer = false;
		StopAttackTimer();
	}
}

void ASurvivorEnemy::StartAttackTimer()
{
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
	if (TargetPlayer && AttributeComp && EnemyData)
	{
		// Apply Damage
		// We need to access the Player's AttributeComponent
		UAttributeComponent* PlayerAttributes = TargetPlayer->AttributeComp;
		if (PlayerAttributes)
		{
			PlayerAttributes->ApplyArmoredDamage(EnemyData->BaseDamage, this);
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

void ASurvivorEnemy::ApplyKnockback(FVector Impulse)
{
	// Add to existing knockback velocity (allows stacking)
	KnockbackVelocity += Impulse;

	// Clear hit tracking for new knockback chain
	KnockbackHitEnemies.Empty();
}

float ASurvivorEnemy::GetKnockbackMass() const
{
	// Use max HP as mass proxy
	if (AttributeComp)
	{
		return FMath::Max(1.0f, AttributeComp->MaxHealth.GetCurrentValue());
	}
	return 100.0f; // Default mass
}

float ASurvivorEnemy::GetKnockbackResistance() const
{
	float MaxHP = GetKnockbackMass();

	// Below light threshold: full knockback
	if (MaxHP <= KnockbackSettings::LightEnemyHP)
	{
		return 1.0f;
	}

	// Above heavy threshold: minimum knockback
	if (MaxHP >= KnockbackSettings::HeavyEnemyHP)
	{
		return KnockbackSettings::MinKnockbackMultiplier;
	}

	// Linear interpolation between light and heavy
	float Alpha = (MaxHP - KnockbackSettings::LightEnemyHP) / (KnockbackSettings::HeavyEnemyHP - KnockbackSettings::LightEnemyHP);
	return FMath::Lerp(1.0f, KnockbackSettings::MinKnockbackMultiplier, Alpha);
}

void ASurvivorEnemy::ProcessKnockback(float DeltaTime)
{
	// Skip if not being knocked back
	if (KnockbackVelocity.IsNearlyZero(KnockbackSettings::MinVelocityThreshold))
	{
		KnockbackVelocity = FVector::ZeroVector;
		KnockbackHitEnemies.Empty();
		return;
	}

	// Move by knockback velocity
	FVector Delta = KnockbackVelocity * DeltaTime;
	FVector NewLocation = GetActorLocation() + Delta;
	SetActorLocation(NewLocation);

	// Check for collisions with other enemies
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		GetActorLocation(),
		KnockbackSettings::CollisionCheckRadius,
		ObjectTypes,
		ASurvivorEnemy::StaticClass(),
		IgnoreActors,
		OverlappingActors
	);

	// Transfer momentum to hit enemies
	for (AActor* Actor : OverlappingActors)
	{
		ASurvivorEnemy* OtherEnemy = Cast<ASurvivorEnemy>(Actor);
		if (!OtherEnemy || KnockbackHitEnemies.Contains(OtherEnemy))
		{
			continue;
		}

		// Mark as hit to prevent double-transfer
		KnockbackHitEnemies.Add(OtherEnemy);

		// Calculate momentum transfer based on mass
		float MyMass = GetKnockbackMass();
		float OtherMass = OtherEnemy->GetKnockbackMass();
		float MassRatio = MyMass / (MyMass + OtherMass);

		// Direction from me to them
		FVector PushDir = (OtherEnemy->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		PushDir.Z = 0.0f;
		if (PushDir.IsNearlyZero())
		{
			PushDir = KnockbackVelocity.GetSafeNormal();
		}
		PushDir.Normalize();

		// Transfer momentum: heavier enemies get pushed less
		float TransferSpeed = KnockbackVelocity.Size() * KnockbackSettings::MomentumTransferRatio * MassRatio;
		FVector TransferVelocity = PushDir * TransferSpeed;

		// Apply to the other enemy (this can chain!)
		OtherEnemy->ApplyKnockback(TransferVelocity);

		// Reduce our own velocity
		KnockbackVelocity *= KnockbackSettings::PusherMomentumRetention;
	}

	// Apply friction deceleration
	float Speed = KnockbackVelocity.Size();
	float NewSpeed = FMath::Max(0.0f, Speed - KnockbackSettings::FrictionDeceleration * DeltaTime);

	if (NewSpeed < KnockbackSettings::MinVelocityThreshold)
	{
		KnockbackVelocity = FVector::ZeroVector;
		KnockbackHitEnemies.Empty();
	}
	else
	{
		KnockbackVelocity = KnockbackVelocity.GetSafeNormal() * NewSpeed;
	}
}

void ASurvivorEnemy::AddCrowdPush(FVector Push)
{
	CrowdPushVelocity = (CrowdPushVelocity + Push).GetClampedToMaxSize(CrowdPushSettings::MaxPushSpeed);
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

	// Reset knockback
	KnockbackVelocity = FVector::ZeroVector;
	KnockbackHitEnemies.Empty();

	// Reset crowd push
	CrowdPushVelocity = FVector::ZeroVector;
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

	// CRITICAL: Re-apply ECR_Ignore for ECC_Pawn after pool reactivation.
	// SetActorEnableCollision(true) above may reset the collision profile to "Pawn" defaults
	// which has ECR_Block for ECC_Pawn, undoing the constructor's ECR_Ignore setting.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Re-apply no-collision on the mesh. SetActorEnableCollision(true) restores saved
	// component profiles, which would bring back the mesh's default blocking profile.
	EnemyMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Re-enable movement - force full reset of movement component state
	// Pre-warmed enemies spawned at Z=-10000 have corrupted floor detection
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->SetComponentTickEnabled(true);
	MoveComp->StopMovementImmediately();
	MoveComp->SetMovementMode(MOVE_None);  // Clear current mode
	MoveComp->SetMovementMode(MOVE_Falling);  // Force falling to reset floor state
	MoveComp->SetMovementMode(MOVE_Walking);  // Now set walking - will trigger floor detection
	MoveComp->Velocity = FVector::ZeroVector;

	// Force floor detection at new location
	FFindFloorResult FloorResult;
	MoveComp->FindFloor(GetActorLocation(), FloorResult, false);
	if (FloorResult.bWalkableFloor)
	{
		MoveComp->SetBase(FloorResult.HitResult.GetComponent(), FloorResult.HitResult.BoneName);
	}

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

	// Reset knockback
	KnockbackVelocity = FVector::ZeroVector;
	KnockbackHitEnemies.Empty();

	// Reset crowd push
	CrowdPushVelocity = FVector::ZeroVector;
}
