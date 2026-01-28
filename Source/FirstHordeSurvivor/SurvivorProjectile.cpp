#include "SurvivorProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AttributeComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SurvivorEnemy.h"

ASurvivorProjectile::ASurvivorProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
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

	// Initialize defaults
	RemainingPierces = 0;
	AoERadius = 0.0f;
	Knockback = 0.0f;

	SphereComp->OnComponentBeginOverlap.AddDynamic(this, &ASurvivorProjectile::OnOverlapBegin);
}

void ASurvivorProjectile::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
}

void ASurvivorProjectile::Initialize(
	float Speed,
	float DamageAmount,
	float Range,
	int32 PierceCount,
	float ExplosionRadius,
	float KnockbackForce,
	USoundBase* InExplosionSound,
	UNiagaraSystem* InExplosionVFX)
{
	MovementComp->InitialSpeed = Speed;
	MovementComp->MaxSpeed = Speed;
	MovementComp->Velocity = GetActorForwardVector() * Speed;

	Damage = DamageAmount;
	MaxRange = Range;
	RemainingPierces = PierceCount;
	AoERadius = ExplosionRadius;
	Knockback = KnockbackForce;
	ExplosionSound = InExplosionSound;
	ExplosionVFX = InExplosionVFX;
}

void ASurvivorProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Range Check
	if (FVector::DistSquared(GetActorLocation(), StartLocation) > MaxRange * MaxRange)
	{
		// If this is an explosive projectile, explode at end of range
		if (AoERadius > 0.0f)
		{
			Explode();
		}
		Destroy();
	}
}

void ASurvivorProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetInstigator())
	{
		return;
	}

	// Skip already-hit enemies (for piercing projectiles)
	if (HitEnemies.Contains(OtherActor))
	{
		return;
	}

	// Check if it's a damageable target
	UAttributeComponent* AttrComp = Cast<UAttributeComponent>(OtherActor->GetComponentByClass(UAttributeComponent::StaticClass()));

	if (AttrComp)
	{
		// Track that we directly hit this enemy (prevents re-hitting same enemy)
		HitEnemies.Add(OtherActor);

		// Apply damage and knockback to the directly-hit enemy
		DamageTarget(OtherActor);

		// Effects based on whether this is an explosive or single-target projectile
		if (AoERadius > 0.0f)
		{
			// Explosive projectile - damage nearby enemies (they can be hit by future explosions)
			Explode(OtherActor);
		}
		else
		{
			// Single-target projectile - play hit effects
			if (HitSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
			}
			if (HitVFX)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitVFX, GetActorLocation());
			}
		}

		// Pierce logic applies to both AoE and non-AoE projectiles
		if (RemainingPierces > 0)
		{
			RemainingPierces--;
			// Continue flying
		}
		else
		{
			// No more pierces, destroy
			Destroy();
		}
	}
	else if (OtherActor->ActorHasTag("WorldStatic"))
	{
		// Hit a wall - explode if applicable
		if (AoERadius > 0.0f)
		{
			Explode();
		}
		Destroy();
	}
}

void ASurvivorProjectile::DamageTarget(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	UAttributeComponent* AttrComp = Cast<UAttributeComponent>(Target->GetComponentByClass(UAttributeComponent::StaticClass()));
	if (AttrComp)
	{
		AttrComp->ApplyHealthChange(-Damage);
	}

	// Apply knockback
	ApplyKnockback(Target);
}

void ASurvivorProjectile::Explode(AActor* DirectHitActor)
{
	// Play explosion effects
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	}
	if (ExplosionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionVFX, GetActorLocation());
	}

	// Find all actors in explosion radius
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		GetActorLocation(),
		AoERadius,
		ObjectTypes,
		nullptr, // Actor class filter
		TArray<AActor*>(), // Ignore actors
		OverlappingActors
	);

	// Damage all enemies in radius EXCEPT:
	// - The instigator (player)
	// - The directly-hit enemy who triggered this explosion (already damaged by DamageTarget)
	// Previously-hit enemies CAN be damaged by this explosion, allowing for stacking AoE damage
	for (AActor* Actor : OverlappingActors)
	{
		if (Actor && Actor != GetInstigator() && Actor != DirectHitActor)
		{
			UAttributeComponent* AttrComp = Cast<UAttributeComponent>(Actor->GetComponentByClass(UAttributeComponent::StaticClass()));
			if (AttrComp)
			{
				DamageTarget(Actor);
			}
		}
	}
}

void ASurvivorProjectile::ApplyKnockback(AActor* Target)
{
	if (Knockback <= 0.0f || !Target)
	{
		return;
	}

	// Get direction from projectile to target
	FVector KnockbackDir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	KnockbackDir.Z = 0.0f; // Keep horizontal
	KnockbackDir.Normalize();

	// Use enemy's knockback system for momentum transfer
	if (ASurvivorEnemy* Enemy = Cast<ASurvivorEnemy>(Target))
	{
		// Scale knockback by enemy's HP-based resistance (lighter = more knockback)
		float ScaledKnockback = Knockback * Enemy->GetKnockbackResistance();
		Enemy->ApplyKnockback(KnockbackDir * ScaledKnockback);
	}
	else if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		// Fallback for non-enemy characters
		Character->LaunchCharacter(KnockbackDir * Knockback, true, false);
	}
}
