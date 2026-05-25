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
#include "FlowFieldSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Engine/OverlapResult.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

// Diagnostic cvar — when > 0, log per-enemy chase/separation forces to spot when separation
// is fighting pathfinding. Filter: only logs enemies moving slower than this value (UU/s),
// rate-limited to once per ~60 ticks per enemy. So `enemy.movedebug 100` shows the stuck ones.
static TAutoConsoleVariable<int32> CVarEnemyMoveDebug(
	TEXT("enemy.movedebug"),
	0,
	TEXT("Log enemies moving slower than N UU/s. 0 = off, positive value = velocity threshold. Once-per-sec per enemy."),
	ECVF_Cheat);

// Clumping diagnostics. Every tick, each enemy contributes its closest-neighbor distance to
// a shared sample bucket; once per second, one enemy emits a summary line (min / median /
// bottom-25% average — the "most clumped" metric the user asked for).
//   enemy.clumpdebug 1 = summary line only
//   enemy.clumpdebug 2 = summary + per-enemy lines (rate-limited)
static TAutoConsoleVariable<int32> CVarEnemyClumpDebug(
	TEXT("enemy.clumpdebug"),
	0,
	TEXT("Log clumping stats. 1 = sec-summary only, 2 = + per-enemy lines."),
	ECVF_Cheat);

// Visualize the fast-pushes-slow shove. When > 0, every push fires a short-lived debug
// arrow above the pushed enemy in the chase direction with length proportional to shove
// magnitude, plus a rate-limited log line in the format the user requested.
static TAutoConsoleVariable<int32> CVarEnemyPushDebug(
	TEXT("enemy.pushdebug"),
	0,
	TEXT("Visualize fast-pushes-slow events. 1 = arrows + rate-limited log."),
	ECVF_Cheat);

// File-static accumulator for the clumping summary. One sample per enemy per tick when
// clumpdebug is on; flushed to log once per second by whichever enemy first crosses the
// gate. Reset at flush time. Also tracks the min/max MaxWalkSpeed seen — if min == max,
// every enemy in the field has the same speed and "fast pushes slow" cannot fire.
namespace EnemyClumpStats
{
	static TArray<float> Samples;
	static double LastSummaryTime = 0.0;
	static float MinMaxSpeedSeen = MAX_FLT;
	static float MaxMaxSpeedSeen = 0.0f;
}

// Enemy separation tuning. Single radial push-away-from-neighbors, projected so it never
// opposes the chase direction — replaces the old L/R perpendicular push + crowd-push pair.
namespace SeparationSettings
{
	// "Personal bubble" radius for the symmetric repulsive separation force. Sample radius
	// = scaled capsule radius * this multiplier. Generous — covers neighbors well outside
	// physical contact range so enemies start spreading out before they collide.
	constexpr float SeparationRadiusMultiplier = 20.5f;

	// Magnitude of the separation AddInputVector contribution. Kept well below chase (1.0)
	// so chase always dominates direction. Separation is a sideways nudge, not a force that
	// can override the path. Old value 1.1 was stronger than chase and caused stalling.
	constexpr float SeparationInputRatio = 0.5f;

	// Contact-push radius for the asymmetric fast-pushes-slow nudge. Much tighter than the
	// separation radius because this represents *physical contact* — a fast enemy should
	// only drag a slow one forward when they're actually overlapping, not from anywhere
	// inside the personal bubble.
	constexpr float ContactPushRadiusMultiplier = 2.5f;
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

// Orbital approach tuning
namespace OrbitSettings
{
	// Distance at which enemies stop pushing toward the player.
	// Should be slightly less than AttackOverlapComp radius (150) so they're in attack range.
	constexpr float OrbitRadius = 160.0f;
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
	// Enemies physically block each other so a fast enemy presses into a slow one and
	// sustains the contact window the fast-pushes-slow shove relies on. Separation force
	// + the contact shove prevent the deadlocks the old ECR_Ignore setup was avoiding.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// Cache constructor-time capsule defaults so InitializeFromData scales from the
	// original base each time (prevents compounding on pooled re-init).
	DefaultCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

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

		// Scale collision capsule to match visual size.
		// Uses the cached constructor-time defaults as the base so repeated
		// Reinitialize calls don't compound the scaling each time.
		GetCapsuleComponent()->SetCapsuleSize(
			DefaultCapsuleRadius * EnemyData->MeshScale,
			DefaultCapsuleHalfHeight * EnemyData->MeshScale
		);

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

	// Decay any "shoved forward by a faster neighbor" boost back toward base speed.
	// The fast-pushes-slow path writes our MaxWalkSpeed up to the pusher's speed each
	// frame they're in contact (see shove block below); this fades it back when contact
	// is lost. Boost lives in MaxWalkSpeed rather than AddActorWorldOffset because the
	// world-offset sweep was being blocked by the column of slow enemies in front of
	// the pushee — boosting MaxWalkSpeed lets CMC drive movement through normal channels,
	// which propagates blocking correctly and cascades through the column (a boosted
	// slow enemy becomes "fast" relative to the slow enemy in front of it and boosts
	// that one too via the same code path).
	if (AttributeComp)
	{
		if (UCharacterMovementComponent* MoveCompForDecay = GetCharacterMovement())
		{
			const float BaseSpeed = AttributeComp->MaxSpeed.GetCurrentValue();
			if (MoveCompForDecay->MaxWalkSpeed > BaseSpeed)
			{
				constexpr float ShoveBoostDecay = 1000.f; // UU/s²
				MoveCompForDecay->MaxWalkSpeed = FMath::Max(BaseSpeed,
					MoveCompForDecay->MaxWalkSpeed - ShoveBoostDecay * DeltaTime);
			}
		}
	}

	// Stun after dealing damage — skip movement input while active. Knockback above is
	// intentionally NOT suppressed so the player can still knock a stunned enemy around.
	const bool bMovementAllowed = (DamageStunTimer <= 0.0f);
	if (!bMovementAllowed)
	{
		DamageStunTimer = FMath::Max(0.0f, DamageStunTimer - DeltaTime);
	}

	// === 1. Chase direction (flow field, with direct-steering fallback) ===
	// SampleDirection returns ZeroVector when the enemy is off the active flow field grid
	// or before the subsystem has finished its first scan; in those cases we fall back to a
	// straight-line ToPlayer vector. Computed before separation so separation can project
	// against it (sideways nudges only — never backward against the path).
	FVector ChaseDir = FVector::ZeroVector;
	bool bHasFlowField = false;
	float DistToPlayer = MAX_FLT;
	if (TargetPlayer)
	{
		FVector ToPlayer = TargetPlayer->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.0f;
		DistToPlayer = ToPlayer.Size2D();
		if (DistToPlayer > KINDA_SMALL_NUMBER)
		{
			ChaseDir = ToPlayer / DistToPlayer;
		}
		if (UFlowFieldSubsystem* FlowField = GetWorld()->GetSubsystem<UFlowFieldSubsystem>())
		{
			const FVector FlowDir = FlowField->SampleDirection(GetActorLocation());
			if (!FlowDir.IsNearlyZero())
			{
				ChaseDir = FlowDir;
				bHasFlowField = true;
			}
		}
	}

	// === 1b. Out-of-flow-field optimization ===
	// Enemies outside the flow field's active region are far from the player (the field
	// recenters on the player) and effectively off-screen. Skip collision so we don't pay
	// enemy-vs-enemy/wall sweeps for them, and give them a flat "catch-up" speed boost so
	// they don't lag indefinitely behind. The boost is re-applied each frame to override the
	// per-tick decay above. When they re-enter the field's region, collision turns back on
	// and the boost decays naturally back to base.
	constexpr float OutOfFieldSpeedMultiplier = 1.4f;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		const bool bShouldDisableCollision = !bHasFlowField;
		const bool bIsCollisionDisabled = (Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		if (bShouldDisableCollision != bIsCollisionDisabled)
		{
			Capsule->SetCollisionEnabled(bShouldDisableCollision
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryAndPhysics);
		}
	}
	if (!bHasFlowField && AttributeComp)
	{
		if (UCharacterMovementComponent* CatchUpMove = GetCharacterMovement())
		{
			const float CatchUpTarget = AttributeComp->MaxSpeed.GetCurrentValue() * OutOfFieldSpeedMultiplier;
			if (CatchUpMove->MaxWalkSpeed < CatchUpTarget)
			{
				CatchUpMove->MaxWalkSpeed = CatchUpTarget;
			}
		}
	}

	// === 2. Separation (single radial push, projected so it never opposes chase) ===
	// Replaces the old L/R-classify + crowd-push pair. Both old systems used straight-line
	// ToPlayer as their reference — which failed near walls when the flow path bent. This
	// version projects out any backward-along-chase component, so the only effect of
	// separation is sideways relative to the actual path. Faster enemies still penetrate
	// crowds further because they have higher MaxWalkSpeed, not because of an explicit push.
	FVector SeparationDir = FVector::ZeroVector;
	int32 NeighborCount = 0;
	float ClosestNeighborDist = MAX_FLT;
	float MyCapsuleRadius = 0.f; // captured for the clump summary line below
	if (bMovementAllowed)
	{
		// OverlapMultiByObjectType (not -ByChannel) so the query ignores the capsule's
		// ECR_Ignore response to ECC_Pawn — channel queries respect the response matrix on
		// both sides, object-type queries do not.
		TArray<FOverlapResult> Overlaps;
		const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		MyCapsuleRadius = CapsuleRadius;
		const float SepRadius = CapsuleRadius * SeparationSettings::SeparationRadiusMultiplier;
		const float ContactRadius = CapsuleRadius * SeparationSettings::ContactPushRadiusMultiplier;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(SepRadius);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		if (GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
			ObjectQueryParams, Sphere, QueryParams))
		{
			FVector Accum = FVector::ZeroVector;
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
				// Weight strongest at distance 0, zero at SepRadius. Adds the unit away-vector
				// scaled by closeness — closer neighbors dominate the sum.
				const float Weight = 1.0f - FMath::Clamp(Dist / SepRadius, 0.0f, 1.0f);
				Accum += (Away / Dist) * Weight;
				NeighborCount++;
				if (Dist < ClosestNeighborDist)
				{
					ClosestNeighborDist = Dist;
				}

				// === Fast pushes slow ===
				// Capsules ignore each other (ECR_Ignore vs ECC_Pawn), so faster enemies can't
				// physically displace slower ones. Without intervention they just wriggle past.
				// Fix: when a slower neighbor is in our forward cone AND within physical
				// contact range, directly translate them along our chase direction at half
				// our speed differential. AddActorWorldOffset with sweep=true respects static
				// geometry, so the shove can't punch them through walls.
				//
				// ContactWeight (tight) is distinct from the personal-bubble Weight used by
				// separation above — contact pushing should only fire when truly overlapping.
				if (!ChaseDir.IsNearlyZero() && DeltaTime > 0.0f && Dist < ContactRadius)
				{
					const FVector ToOther = -Away / Dist; // unit me → other
					if (FVector::DotProduct(ToOther, ChaseDir) > 0.5f) // ~60° forward cone
					{
						const float MyMaxSpeed    = GetCharacterMovement()->MaxWalkSpeed;
						const float OtherMaxSpeed = OtherEnemy->GetCharacterMovement()->MaxWalkSpeed;
						if (OtherMaxSpeed < MyMaxSpeed - KINDA_SMALL_NUMBER)
						{
							// Boost the slower neighbor's MaxWalkSpeed up to ours so their CMC drives them
							// forward at our pace via normal movement integration. We were using
							// AddActorWorldOffset(bSweep=true) before, but the sweep got blocked by the
							// column of slow enemies in front of the pushee → zero displacement. CMC-driven
							// movement, by contrast, propagates the blocking cascade correctly: a boosted
							// slow enemy becomes "fast" relative to the slow one in front and re-boosts
							// it via this same path. Decay back to base happens at the top of Tick.
							UCharacterMovementComponent* OtherMove = OtherEnemy->GetCharacterMovement();
							if (OtherMove && OtherMove->MaxWalkSpeed < MyMaxSpeed)
							{
								OtherMove->MaxWalkSpeed = MyMaxSpeed;
							}

							// Lateral nudge so the slow enemy gradually slides off our path instead of
							// getting taken for a ride directly in front of us. Decide which side based on
							// where the slow enemy already sits relative to our forward axis — projecting
							// ToOther onto ChaseDir's right-perpendicular gives a signed lateral bias.
							// If they're nearly head-on (|bias| below threshold), break the tie with a
							// hash of the pair's UniqueIDs so the chosen side stays stable (no flicker).
							// Input is added via AddInputVector so CMC sums it with the slow enemy's own
							// chase input — net effect is a diagonal walk that drifts them off-axis.
							if (OtherMove)
							{
								const FVector RightDir = FVector::CrossProduct(FVector::UpVector, ChaseDir).GetSafeNormal();
								const float LateralBias = FVector::DotProduct(ToOther, RightDir);
								float SideSign;
								constexpr float HeadOnTolerance = 0.05f; // ~3° off-axis
								if (FMath::Abs(LateralBias) > HeadOnTolerance)
								{
									SideSign = FMath::Sign(LateralBias);
								}
								else
								{
									const uint32 HashKey = GetUniqueID() ^ OtherEnemy->GetUniqueID();
									SideSign = (HashKey & 1u) ? 1.f : -1.f;
								}
								constexpr float LateralInputRatio = 0.3f; // < chase (1.0) so forward still dominates
								OtherMove->AddInputVector(RightDir * (SideSign * LateralInputRatio));
							}

							const float ShoveSpeed = MyMaxSpeed - OtherMaxSpeed; // UU/sec, for arrow/log only

							// Push viz + log. Arrow every frame (short lifetime so it fades);
							// log line rate-limited per pushee to ~once/sec so dense fights
							// don't spam.
							const int32 PushDebug = CVarEnemyPushDebug.GetValueOnGameThread();
							if (PushDebug > 0)
							{
								const FVector ArrowBase = OtherEnemy->GetActorLocation()
									+ FVector(0.f, 0.f, 90.f);
								// Length scaled to magnitude so big shoves draw long arrows.
								// 0.4f keeps a 200 UU/s shove ≈ 80 UU long — visible but not silly.
								const FVector ArrowTip = ArrowBase + ChaseDir * (ShoveSpeed * 0.4f);
								DrawDebugDirectionalArrow(GetWorld(), ArrowBase, ArrowTip,
									20.f, FColor::Yellow, false, 0.12f, 0, 3.f);

								const uint32 Stagger = (GFrameCounter + OtherEnemy->GetUniqueID()) % 60u;
								if (Stagger == 0u)
								{
									const float PusheeSpeed = OtherEnemy->GetVelocity().Size2D();
									UE_LOG(LogTemp, Verbose,
										TEXT("EnemyPush: '%s' pushed by %.0f UU/s (pre-push speed=%.0f UU/s, my MaxWalk=%.0f, their MaxWalk=%.0f, dist=%.0f)"),
										*OtherEnemy->GetName(), ShoveSpeed, PusheeSpeed,
										MyMaxSpeed, OtherMaxSpeed, Dist);
								}
							}
						}
					}
				}
			}

			if (!Accum.IsNearlyZero(0.01f))
			{
				SeparationDir = Accum.GetSafeNormal();
			}
			else if (NeighborCount > 0)
			{
				// Cancellation fallback: when we're surrounded symmetrically the away-vectors
				// sum to ≈0 and the normalize returns ZeroVector — net separation push: zero.
				// This is exactly the case where we MOST need separation (deep clump). Pick a
				// stable per-enemy escape angle from the actor's unique ID so each surrounded
				// enemy commits to a different direction; otherwise neighboring enemies would
				// all pick the same arbitrary direction and re-collide. The old L/R-count
				// system provided this asymmetric tie-breaker implicitly.
				const float Angle = static_cast<float>(GetUniqueID() % 360) * (PI / 180.0f);
				SeparationDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			}

			if (!SeparationDir.IsNearlyZero())
			{
				// Project out any component opposing the chase direction. This is the key
				// fix vs the old design: with a perpendicular reference of ToPlayer, when
				// flow bends around a wall, separation could push enemies into the wall or
				// backward along the path. Projecting against ChaseDir means separation can
				// only push sideways or forward — never against the flow.
				if (!ChaseDir.IsNearlyZero())
				{
					const float Backward = FVector::DotProduct(SeparationDir, ChaseDir);
					if (Backward < 0.0f)
					{
						SeparationDir = (SeparationDir - Backward * ChaseDir).GetSafeNormal();
					}
				}
			}
		}
	}

	// === 2b. Clumping diagnostics (opt-in via `enemy.clumpdebug 1` + `log LogTemp Verbose`) ===
	// Each enemy contributes its closest-neighbor distance to a rolling sample bucket; once
	// per second, whichever enemy first crosses the gate emits a summary line. The bottom-25%
	// average tells us how close the most-clumped enemies are to their nearest neighbor — the
	// objective health metric for the separation/push systems.
	const int32 ClumpDebug = CVarEnemyClumpDebug.GetValueOnGameThread();
	if (ClumpDebug > 0)
	{
		// Always track speed range so the summary can show whether enemies are uniform-speed
		// (which makes fast-pushes-slow impossible by construction).
		const float MyMaxWalkSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f;
		EnemyClumpStats::MinMaxSpeedSeen = FMath::Min(EnemyClumpStats::MinMaxSpeedSeen, MyMaxWalkSpeed);
		EnemyClumpStats::MaxMaxSpeedSeen = FMath::Max(EnemyClumpStats::MaxMaxSpeedSeen, MyMaxWalkSpeed);

		if (ClosestNeighborDist < MAX_FLT)
		{
			EnemyClumpStats::Samples.Add(ClosestNeighborDist);
		}

		const double Now = FPlatformTime::Seconds();
		if (Now - EnemyClumpStats::LastSummaryTime > 1.0)
		{
			EnemyClumpStats::LastSummaryTime = Now;
			TArray<float>& S = EnemyClumpStats::Samples;
			if (S.Num() > 0)
			{
				S.Sort();
				const int32 N = S.Num();
				const float MinDist = S[0];
				const float MedianDist = S[N / 2];
				const int32 Bottom25 = FMath::Max(1, N / 4);
				float Bottom25Sum = 0.f;
				for (int32 i = 0; i < Bottom25; ++i)
				{
					Bottom25Sum += S[i];
				}
				const float Bottom25Avg = Bottom25Sum / Bottom25;
				const float TouchingDist = MyCapsuleRadius * 2.f;
				UE_LOG(LogTemp, Verbose,
					TEXT("EnemyClumpSummary: samples=%d  min=%.0f  median=%.0f  bottom25%%-avg=%.0f UU  (capsule-touching ~%.0f)  speedRange=[%.0f..%.0f]"),
					N, MinDist, MedianDist, Bottom25Avg, TouchingDist,
					EnemyClumpStats::MinMaxSpeedSeen, EnemyClumpStats::MaxMaxSpeedSeen);
				S.Reset();
				EnemyClumpStats::MinMaxSpeedSeen = MAX_FLT;
				EnemyClumpStats::MaxMaxSpeedSeen = 0.0f;
			}
		}
	}

	// === 3. Apply forces — both via CharacterMovement so walls are respected ===
	if (bMovementAllowed && TargetPlayer)
	{
		UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		if (MoveComp)
		{
			// Chase only outside orbit radius — inside, the enemy is in attack range and
			// further input would shove them through the player.
			if (DistToPlayer > OrbitSettings::OrbitRadius && !ChaseDir.IsNearlyZero())
			{
				MoveComp->AddInputVector(ChaseDir);
			}
			// Separation applies at all distances so enemies don't overlap even when piled
			// up inside the orbit ring.
			if (!SeparationDir.IsNearlyZero())
			{
				MoveComp->AddInputVector(SeparationDir * SeparationSettings::SeparationInputRatio);
			}
		}

		// Face the player (visual only, doesn't affect movement).
		FVector ToPlayer = TargetPlayer->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.0f;
		const float DistFlat = ToPlayer.Size2D();
		if (DistFlat > KINDA_SMALL_NUMBER)
		{
			SetActorRotation((ToPlayer / DistFlat).Rotation());
		}
	}

	// === 4. Diagnostic logging — opt-in via `enemy.movedebug N` (N = velocity threshold) ===
	// Filters to slow-moving enemies (the stuck ones) and rate-limits to ~once per sec per
	// enemy. Lets us see when separation strength rivals chase, or when an enemy has a
	// non-zero flow but isn't moving.
	const int32 MoveDebugThreshold = CVarEnemyMoveDebug.GetValueOnGameThread();
	if (MoveDebugThreshold > 0)
	{
		const float SpeedSq = GetVelocity().SizeSquared2D();
		const float ThreshSq = (float)MoveDebugThreshold * (float)MoveDebugThreshold;
		if (SpeedSq < ThreshSq)
		{
			// Stagger per-enemy by hashing the unique ID into the frame counter, so we don't
			// burst-log every enemy on the same frame.
			const uint32 Stagger = (GFrameCounter + GetUniqueID()) % 60u;
			if (Stagger == 0u)
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("EnemyMove: '%s' vel=%.0f dist=%.0f flow=%s chaseDir=(%.2f,%.2f) sepDir=(%.2f,%.2f) sepMag=%.2f neighbors=%d stun=%.2f"),
					*GetName(), FMath::Sqrt(SpeedSq), DistToPlayer,
					bHasFlowField ? TEXT("YES") : TEXT("no"),
					ChaseDir.X, ChaseDir.Y,
					SeparationDir.X, SeparationDir.Y,
					SeparationSettings::SeparationInputRatio,
					NeighborCount, DamageStunTimer);
			}
		}
	}

	// === 5. Hit flash decay (unchanged) ===
	if (HitFlashIntensity > 0.0f)
	{
		if (HitFlashHoldTimer > 0.0f)
		{
			HitFlashHoldTimer -= DeltaTime;
		}
		else
		{
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
			// Self-stun for 0.5s after dealing damage — gives the player a brief breathing
			// window before this enemy can re-engage / re-attack.
			DamageStunTimer = 0.5f;
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

	// Reset damage stun
	DamageStunTimer = 0.0f;
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

	// Re-apply ECR_Block for ECC_Pawn after pool reactivation to match constructor.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

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

	// Reset damage stun
	DamageStunTimer = 0.0f;
}
