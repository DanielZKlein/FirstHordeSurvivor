#include "SurvivorProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AttributeComponent.h"
#include "NiagaraFunctionLibrary.h"

ASurvivorProjectile::ASurvivorProjectile()
{
 	PrimaryActorTick.bCanEverTick = true;

    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->SetCollisionProfileName("Projectile"); // Custom profile or OverlapAllDynamic?
    // Let's use OverlapAllDynamic but Block WorldStatic? 
    // Actually, usually projectiles overlap enemies.
    SphereComp->SetCollisionProfileName("OverlapAllDynamic");
    SphereComp->SetGenerateOverlapEvents(true);
    RootComponent = SphereComp;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
    MovementComp->ProjectileGravityScale = 0.0f;
    MovementComp->bRotationFollowsVelocity = true;
    MovementComp->bInitialVelocityInLocalSpace = true;

    TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailComp"));
    TrailComp->SetupAttachment(RootComponent);

    // Bind Hit (Actually Overlap since we set OverlapAllDynamic)
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &ASurvivorProjectile::OnOverlapBegin);
}

void ASurvivorProjectile::BeginPlay()
{
	Super::BeginPlay();
    StartLocation = GetActorLocation();
}

void ASurvivorProjectile::Initialize(float Speed, float DamageAmount, float Range)
{
    MovementComp->InitialSpeed = Speed;
    MovementComp->MaxSpeed = Speed;
    MovementComp->Velocity = GetActorForwardVector() * Speed; // Explicitly set velocity if needed
    
    Damage = DamageAmount;
    MaxRange = Range;
}

void ASurvivorProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Range Check
    if (FVector::DistSquared(GetActorLocation(), StartLocation) > MaxRange * MaxRange)
    {
        Destroy();
    }
}

void ASurvivorProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != GetInstigator())
    {
        // Check if it's an enemy (has AttributeComponent)
        // We could interface check, or just look for the component
        UAttributeComponent* AttrComp = Cast<UAttributeComponent>(OtherActor->GetComponentByClass(UAttributeComponent::StaticClass()));
        
        if (AttrComp)
        {
            // Apply Damage
            if (AttrComp->ApplyHealthChange(-Damage))
            {
                // Play Hit Sound
                if (HitSound)
                {
                    UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
                }

                // Play Hit VFX
                if (HitVFX)
                {
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitVFX, GetActorLocation());
                }

                // Destroy Projectile
                Destroy();
            }
        }
        else if (OtherActor->ActorHasTag("WorldStatic")) // Optional: Destroy on walls?
        {
             Destroy();
        }
    }
}
