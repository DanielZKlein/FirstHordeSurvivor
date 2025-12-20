#include "XPGem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
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

	FlyAwayForce = 300.0f;
	MagnetAcceleration = 2000.0f;
	MaxSpeed = 1500.0f;
	CollectDistance = 50.0f;
    SpawnDuration = 0.5f;

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
    SetActorLocation(StartLocation);
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
    
    // Random fly away direction (up + random horizontal)
    FVector RandomDir = FMath::VRand();
    RandomDir.Z = FMath::Abs(RandomDir.Z) + 0.5f; // Bias upwards
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
}

void AXPGem::Deactivate()
{
    State = EXPGemState::Inactive;
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);
    TrailComp->Deactivate();
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
            // We need to cast to get the dynamic range, or use a default
            float PickupRange = 500.0f;
            if (ASurvivorCharacter* Player = Cast<ASurvivorCharacter>(TargetActor))
            {
                // We will add this property to SurvivorCharacter later
                 PickupRange = Player->PickupRange;
            }
            
            if (DistSq < PickupRange * PickupRange)
            {
                State = EXPGemState::Magnetizing;
                CurrentSpeed = 0.0f; // Reset speed for acceleration
            }
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
