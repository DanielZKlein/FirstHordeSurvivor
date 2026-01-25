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
	GetWorldTimerManager().ClearTimer(TimerHandle_Burst);
	RemainingBurstProjectiles = 0;
}

float ASurvivorWeapon::GetStat(EWeaponStat Stat) const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	// Get base value from weapon data
	float BaseValue = WeaponData->GetBaseStatValue(Stat);

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
	if (!Target)
	{
		return;
	}

	// Cache spawn info
	BurstSpawnLocation = GetOwner() ? GetOwner()->GetActorLocation() : GetActorLocation();
	FVector Direction = (Target->GetActorLocation() - BurstSpawnLocation).GetSafeNormal();

	// Apply Precision to base direction (random deviation for the whole fan)
	float HalfPrecision = ProjData->Precision * 0.5f;
	FRotator BaseRot = Direction.Rotation();
	BaseRot.Pitch += FMath::RandRange(-HalfPrecision, HalfPrecision);
	BaseRot.Yaw += FMath::RandRange(-HalfPrecision, HalfPrecision);
	BurstBaseDirection = BaseRot.Vector();
	BurstBaseDirection.Z = 0.0f;
	BurstBaseDirection.Normalize();

	// Get projectile count
	TotalBurstProjectiles = FMath::Max(1, FMath::RoundToInt(GetStat(EWeaponStat::ProjectileCount)));

	if (TotalBurstProjectiles == 1 || ProjData->MultiShotMode == EMultiShotMode::Volley)
	{
		// Volley mode: Fire all projectiles at once
		for (int32 i = 0; i < TotalBurstProjectiles; ++i)
		{
			FireSingleProjectile(i, TotalBurstProjectiles);
		}

		// Play attack sound/VFX once for the volley
		if (WeaponData->AttackSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, WeaponData->AttackSound, BurstSpawnLocation);
		}
		if (WeaponData->AttackVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->AttackVFX, BurstSpawnLocation);
		}
	}
	else
	{
		// Barrage mode: Fire projectiles sequentially
		// Stop the repeating timer - we'll restart it after burst completes
		StopShooting();
		GetWorldTimerManager().ClearTimer(TimerHandle_Burst);

		// Fire first projectile
		RemainingBurstProjectiles = TotalBurstProjectiles;
		FireSingleProjectile(0, TotalBurstProjectiles);
		RemainingBurstProjectiles--;

		// Play sound/VFX for first shot
		if (WeaponData->AttackSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, WeaponData->AttackSound, BurstSpawnLocation);
		}
		if (WeaponData->AttackVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->AttackVFX, BurstSpawnLocation);
		}

		// Schedule next projectile in burst
		if (RemainingBurstProjectiles > 0)
		{
			float BurstDelay = 60.0f / ProjData->BarrageRPM;
			GetWorldTimerManager().SetTimer(TimerHandle_Burst, this, &ASurvivorWeapon::ContinueBurst, BurstDelay, false);
		}
		else
		{
			ScheduleNextAttack();
		}
	}
}

void ASurvivorWeapon::FireSingleProjectile(int32 ProjectileIndex, int32 TotalProjectiles)
{
	UProjectileWeaponData* ProjData = GetProjectileData();
	if (!ProjData || !ProjData->ProjectileClass)
	{
		return;
	}

	// Calculate direction for this projectile in the spread pattern
	FRotator Rot = BurstBaseDirection.Rotation();

	if (TotalProjectiles > 1)
	{
		// Spread evenly across SpreadAngle
		float HalfSpread = ProjData->SpreadAngle * 0.5f;
		float SpreadOffset = -HalfSpread + (ProjData->SpreadAngle * ProjectileIndex / (TotalProjectiles - 1));
		Rot.Yaw += SpreadOffset;
	}

	FVector FinalDir = Rot.Vector();
	FinalDir.Z = 0.0f;
	FinalDir.Normalize();
	Rot = FinalDir.Rotation();

	// Spawn Projectile
	FTransform SpawnTM(Rot, BurstSpawnLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	ASurvivorProjectile* Proj = GetWorld()->SpawnActor<ASurvivorProjectile>(ProjData->ProjectileClass, SpawnTM, SpawnParams);
	if (Proj)
	{
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

void ASurvivorWeapon::ContinueBurst()
{
	UProjectileWeaponData* ProjData = GetProjectileData();
	if (!ProjData)
	{
		ScheduleNextAttack();
		return;
	}

	// Fire next projectile in burst
	int32 CurrentIndex = TotalBurstProjectiles - RemainingBurstProjectiles;
	FireSingleProjectile(CurrentIndex, TotalBurstProjectiles);
	RemainingBurstProjectiles--;

	// Play sound/VFX for each shot in barrage
	if (WeaponData->AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->AttackSound, BurstSpawnLocation);
	}
	if (WeaponData->AttackVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->AttackVFX, BurstSpawnLocation);
	}

	// More projectiles to fire?
	if (RemainingBurstProjectiles > 0)
	{
		float BurstDelay = 60.0f / ProjData->BarrageRPM;
		GetWorldTimerManager().SetTimer(TimerHandle_Burst, this, &ASurvivorWeapon::ContinueBurst, BurstDelay, false);
	}
	else
	{
		// Burst complete, start cooldown
		ScheduleNextAttack();
	}
}

void ASurvivorWeapon::ScheduleNextAttack()
{
	float RPM = GetEffectiveRPM();
	if (RPM > 0.0f)
	{
		float Delay = 60.0f / RPM;
		GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &ASurvivorWeapon::Fire, Delay, false);
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
