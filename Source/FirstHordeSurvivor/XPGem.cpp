#include "XPGem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "XPGemSubsystem.h"
#include "SurvivorCharacter.h"

AXPGem::AXPGem()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
	MeshComp->SetCollisionProfileName(TEXT("NoCollision")); // We handle pickup via distance check in Tick or Player

	TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailComp"));
	TrailComp->SetupAttachment(RootComponent);
	TrailComp->SetAutoActivate(false);

	LightComp = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComp"));
	LightComp->SetupAttachment(RootComponent);
	LightComp->SetRelativeLocation(FVector::ZeroVector); // Center of mesh
	LightComp->SetCastShadows(false); // No shadows - light shines through the gem
	LightComp->SetIntensity(1000.0f);
	LightComp->SetAttenuationRadius(200.0f);

	FlyAwayForce = 800.0f;
	MagnetAcceleration = 2000.0f;
	MaxSpeed = 3000.0f;
	CollectDistance = 50.0f;
	SpawnDuration = 0.8f;
	FleeDuration = 0.35f;
	FleeForce = 1200.0f;

	State = EXPGemState::Inactive;
}

void AXPGem::BeginPlay()
{
	Super::BeginPlay();
	FindTarget();
}

void AXPGem::FindTarget()
{
	TargetActor = UGameplayStatics::GetPlayerCharacter(this, 0);
}

void AXPGem::Initialize(int32 InValue, const FVector& StartLocation)
{
	XPValue = InValue;

	// Lower spawn position closer to ground (enemy location is at capsule center)
	FVector AdjustedLocation = StartLocation;
	AdjustedLocation.Z -= 100.0f;
	SetActorLocation(AdjustedLocation);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);

	// Random fly away direction (biased upward for a satisfying pop)
	FVector RandomDir = FMath::VRand();
	RandomDir.Z = FMath::Abs(RandomDir.Z) + 1.0f; // Strong upward bias
	Velocity = RandomDir.GetSafeNormal() * FlyAwayForce;

	State = EXPGemState::Spawning;
	SpawnTimer = 0.0f;
	CurrentSpeed = FlyAwayForce;

	if (!TargetActor)
	{
		FindTarget();
	}
}

void AXPGem::SetVisuals(const FXPGemData& VisualData)
{
	if (VisualData.Mesh)
	{
		MeshComp->SetStaticMesh(VisualData.Mesh);
	}

	if (VisualData.Material)
	{
		// Create a dynamic material instance for full runtime control
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(VisualData.Material, this);
		if (DynMaterial)
		{
			// Set parameters on the dynamic instance
			DynMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(VisualData.Color));
			DynMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), VisualData.EmissiveStrength);

			int32 NumMaterials = MeshComp->GetNumMaterials();
			for (int32 i = 0; i < NumMaterials; i++)
			{
				MeshComp->SetMaterial(i, DynMaterial);
			}
		}
	}

	SetActorScale3D(FVector(VisualData.Scale));

	if (VisualData.TrailEffect)
	{
		TrailComp->SetAsset(VisualData.TrailEffect);
		TrailComp->Activate();
	}
	else
	{
		TrailComp->Deactivate();
	}

	// Configure point light
	LightComp->SetLightColor(VisualData.Color);
	LightComp->SetIntensity(VisualData.LightIntensity);
	LightComp->SetAttenuationRadius(VisualData.LightRadius);
	LightComp->SetVisibility(true);
}

void AXPGem::Deactivate()
{
	State = EXPGemState::Inactive;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	TrailComp->Deactivate();
	LightComp->SetVisibility(false);
}

void AXPGem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == EXPGemState::Inactive || State == EXPGemState::Collected)
	{
		return;
	}

	if (State == EXPGemState::Spawning)
	{
		// Apply drag/gravity-ish to slow down the fly away
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, 5.0f);
		AddActorWorldOffset(Velocity * DeltaTime);

		SpawnTimer += DeltaTime;
		if (SpawnTimer >= SpawnDuration)
		{
			State = EXPGemState::Idle;
		}
	}
	else if (State == EXPGemState::Idle)
	{
		if (TargetActor)
		{
			float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());

			// Check pickup range from player
			float PickupRange = 500.0f;
			if (ASurvivorCharacter* Player = Cast<ASurvivorCharacter>(TargetActor))
			{
				PickupRange = Player->PickupRange;
			}

			if (DistSq < PickupRange * PickupRange)
			{
				// Start fleeing AWAY from player first
				State = EXPGemState::Fleeing;
				FleeTimer = 0.0f;

				// Calculate direction away from player (with upward bias for drama)
				FVector FleeDir = (GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();
				FleeDir.Z = FMath::Abs(FleeDir.Z) + 0.5f; // Pop up while fleeing
				Velocity = FleeDir.GetSafeNormal() * FleeForce;
			}
		}
	}
	else if (State == EXPGemState::Fleeing)
	{
		// Dramatically move away from player before reversing
		FleeTimer += DeltaTime;

		// Apply drag to slow down the flee
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, 4.0f);
		AddActorWorldOffset(Velocity * DeltaTime);

		if (FleeTimer >= FleeDuration)
		{
			// Now magnetize toward player
			State = EXPGemState::Magnetizing;
			CurrentSpeed = 0.0f;
		}
	}

	if (State == EXPGemState::Magnetizing)
	{
		if (TargetActor)
		{
			FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			CurrentSpeed += MagnetAcceleration * DeltaTime;
			CurrentSpeed = FMath::Min(CurrentSpeed, MaxSpeed);

			Velocity = Direction * CurrentSpeed;
			AddActorWorldOffset(Velocity * DeltaTime);

			float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
			if (DistSq < CollectDistance * CollectDistance)
			{
				// Collect
				State = EXPGemState::Collected;

				// Give XP
				ASurvivorCharacter* Player = Cast<ASurvivorCharacter>(TargetActor);
				if (Player)
				{
					Player->AddXP(XPValue);
				}

				// Return to pool
				if (UWorld* World = GetWorld())
				{
					if (UXPGemSubsystem* Subsystem = World->GetSubsystem<UXPGemSubsystem>())
					{
						Subsystem->ReturnGem(this);
					}
				}
			}
		}
	}
}
