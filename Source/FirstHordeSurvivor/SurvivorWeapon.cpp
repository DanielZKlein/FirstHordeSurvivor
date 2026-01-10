#include "SurvivorWeapon.h"
#include "WeaponDataBase.h"
#include "ProjectileWeaponData.h"
#include "SurvivorProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorEnemy.h"
#include "NiagaraFunctionLibrary.h"
#include "DrawDebugHelpers.h"

ASurvivorWeapon::ASurvivorWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASurvivorWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponData)
	{
		StartShooting();
	}
}

void ASurvivorWeapon::StartShooting()
{
	float RPM = GetEffectiveRPM();
	if (RPM > 0.0f)
	{
		float Delay = 60.0f / RPM;
		GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &ASurvivorWeapon::Fire, Delay, true);
	}
}

void ASurvivorWeapon::StopShooting()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Attack);
}

float ASurvivorWeapon::GetStat(EWeaponStat Stat) const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	// Get base value from weapon data
	float BaseValue = WeaponData->GetStatValue(Stat);

	// Apply runtime modifiers if any
	if (const FGameplayAttribute* Modifier = StatModifiers.Find(Stat))
	{
		// Modifier is applied on top: (BaseValue + Additive) * Multiplicative
		return (BaseValue + Modifier->Additive) * Modifier->Multiplicative;
	}

	return BaseValue;
}

void ASurvivorWeapon::ApplyStatUpgrade(EWeaponStat Stat, float Additive, float Multiplicative)
{
	if (!StatModifiers.Contains(Stat))
	{
		// Initialize modifier if not present
		StatModifiers.Add(Stat, FGameplayAttribute(0.0f));
		StatModifiers[Stat].Multiplicative = 1.0f;
	}

	// Stack the modifiers
	StatModifiers[Stat].Additive += Additive;
	StatModifiers[Stat].Multiplicative *= Multiplicative;

	// If attack speed changed, update the fire timer
	if (Stat == EWeaponStat::AttackSpeed)
	{
		StopShooting();
		StartShooting();
	}
}

bool ASurvivorWeapon::UsesStat(EWeaponStat Stat) const
{
	if (!WeaponData)
	{
		return false;
	}
	return WeaponData->GetApplicableStats().Contains(Stat);
}

TArray<EWeaponStat> ASurvivorWeapon::GetApplicableStats() const
{
	if (WeaponData)
	{
		return WeaponData->GetApplicableStats();
	}
	return TArray<EWeaponStat>();
}

UProjectileWeaponData* ASurvivorWeapon::GetProjectileData() const
{
	return Cast<UProjectileWeaponData>(WeaponData);
}

float ASurvivorWeapon::GetEffectiveRPM() const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	float BaseRPM = WeaponData->GetBaseRPM();
	float AttackSpeedMod = GetStat(EWeaponStat::AttackSpeed);
	return BaseRPM * AttackSpeedMod;
}

void ASurvivorWeapon::Fire()
{
	UProjectileWeaponData* ProjData = GetProjectileData();
	if (!ProjData || !ProjData->ProjectileClass)
	{
		return;
	}

	AActor* Target = FindBestTarget();
	if (Target)
	{
		// Use owner's location as the firing position
		FVector SpawnLocation = GetOwner() ? GetOwner()->GetActorLocation() : GetActorLocation();

		// Calculate Direction
		FVector Direction = (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();

		// Get projectile count (for multi-shot)
		int32 ProjectileCount = FMath::Max(1, FMath::RoundToInt(GetStat(EWeaponStat::ProjectileCount)));

		for (int32 i = 0; i < ProjectileCount; ++i)
		{
			// Apply Cone Precision
			float HalfCone = ProjData->Precision * 0.5f;

			// For multi-shot, spread projectiles evenly within cone
			FRotator Rot = Direction.Rotation();
			if (ProjectileCount > 1)
			{
				// Spread evenly across the cone
				float Spread = (i - (ProjectileCount - 1) * 0.5f) * (ProjData->Precision / (ProjectileCount - 1));
				Rot.Yaw += Spread;
			}
			else
			{
				// Single projectile: random within cone
				Rot.Pitch += FMath::RandRange(-HalfCone, HalfCone);
				Rot.Yaw += FMath::RandRange(-HalfCone, HalfCone);
			}

			FVector FinalDir = Rot.Vector();
			FinalDir.Z = 0.0f;
			FinalDir.Normalize();
			Rot = FinalDir.Rotation();

			// Spawn Projectile
			FTransform SpawnTM(Rot, SpawnLocation);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Instigator = Cast<APawn>(GetOwner());

			ASurvivorProjectile* Proj = GetWorld()->SpawnActor<ASurvivorProjectile>(ProjData->ProjectileClass, SpawnTM, SpawnParams);
			if (Proj)
			{
				// Pass stats to projectile using the modifier-applied values
				Proj->Initialize(
					GetStat(EWeaponStat::ProjectileSpeed),
					GetStat(EWeaponStat::Damage),
					GetStat(EWeaponStat::Range),
					FMath::RoundToInt(GetStat(EWeaponStat::Penetration)),
					GetStat(EWeaponStat::Area),
					GetStat(EWeaponStat::Knockback),
					ProjData->ExplosionSound,
					ProjData->ExplosionVFX
				);
			}
		}

		// Play Sound
		if (WeaponData->AttackSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, WeaponData->AttackSound, SpawnLocation);
		}

		// Play VFX
		if (WeaponData->AttackVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->AttackVFX, SpawnLocation);
		}
	}
}

AActor* ASurvivorWeapon::FindBestTarget()
{
	UProjectileWeaponData* ProjData = GetProjectileData();
	if (!ProjData)
	{
		return nullptr;
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ASurvivorEnemy::StaticClass(), Enemies);

	if (Enemies.Num() == 0)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	// Use owner's location (the player) rather than weapon's own location
	FVector MyLoc = GetOwner() ? GetOwner()->GetActorLocation() : GetActorLocation();
	FVector MyVelocity = FVector::ZeroVector;

	// Get Owner Velocity for "InFront" calculation
	if (AActor* OwnerActor = GetOwner())
	{
		MyVelocity = OwnerActor->GetVelocity();
	}

	// Normalize velocity for Dot Product
	FVector MoveDir = MyVelocity.GetSafeNormal();
	bool bIsMoving = !MyVelocity.IsNearlyZero();

	float MaxRange = GetStat(EWeaponStat::Range);

	for (AActor* Enemy : Enemies)
	{
		float DistSq = FVector::DistSquared(MyLoc, Enemy->GetActorLocation());
		float Dist = FMath::Sqrt(DistSq);

		// Max Range Check
		if (Dist > MaxRange)
		{
			continue;
		}

		// Calculate Score
		float RangeScore = Dist * ProjData->RangeWeight;
		float InFrontScore = 0.0f;

		// InFront Score
		if (bIsMoving)
		{
			FVector DirToEnemy = (Enemy->GetActorLocation() - MyLoc).GetSafeNormal();
			float Dot = FVector::DotProduct(MoveDir, DirToEnemy);
			InFrontScore = Dot * ProjData->InFrontWeight;
		}

		float TotalScore = RangeScore + InFrontScore;

		if (TotalScore > BestScore)
		{
			BestScore = TotalScore;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}
