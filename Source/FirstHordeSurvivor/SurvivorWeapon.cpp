#include "SurvivorWeapon.h"
#include "WeaponData.h"
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
    if (WeaponData && WeaponData->RPM > 0.0f)
    {
        float Delay = 60.0f / WeaponData->RPM;
        GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &ASurvivorWeapon::Fire, Delay, true);
    }
}

void ASurvivorWeapon::StopShooting()
{
    GetWorldTimerManager().ClearTimer(TimerHandle_Attack);
}

void ASurvivorWeapon::Fire()
{
    if (!WeaponData || !WeaponData->ProjectileClass)
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
        
        // Apply Cone Precision
        // Precision is in degrees.
        float HalfCone = WeaponData->Precision * 0.5f;
        
        // Random point in cone
        // Simple way: Add random rotation to the direction
        FRotator Rot = Direction.Rotation();
        Rot.Pitch += FMath::RandRange(-HalfCone, HalfCone);
        Rot.Yaw += FMath::RandRange(-HalfCone, HalfCone);
        
        FVector FinalDir = Rot.Vector();
        FinalDir.Z = 0.0f;
        FinalDir.Normalize();
        Rot = FinalDir.Rotation();

        // Spawn Projectile
        FTransform SpawnTM(Rot, SpawnLocation);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Instigator = Cast<APawn>(GetOwner());

        ASurvivorProjectile* Proj = GetWorld()->SpawnActor<ASurvivorProjectile>(WeaponData->ProjectileClass, SpawnTM, SpawnParams);
        if (Proj)
        {
            Proj->Initialize(WeaponData->ProjectileSpeed, WeaponData->Damage, WeaponData->MaxRange);
        }

        // Play Sound
        if (WeaponData->ShootSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, WeaponData->ShootSound, SpawnLocation);
        }

        // Play VFX
        if (WeaponData->ShootVFX)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponData->ShootVFX, SpawnLocation);
        }
    }
}

AActor* ASurvivorWeapon::FindBestTarget()
{
    if (!WeaponData) return nullptr;

    TArray<AActor*> Enemies;
    UGameplayStatics::GetAllActorsOfClass(this, ASurvivorEnemy::StaticClass(), Enemies);

    if (Enemies.Num() == 0)
    {
        //UE_LOG(LogTemp, Warning, TEXT("ASurvivorWeapon::FindBestTarget - No enemies found in world"));
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

    for (AActor* Enemy : Enemies)
    {
        float DistSq = FVector::DistSquared(MyLoc, Enemy->GetActorLocation());
        float Dist = FMath::Sqrt(DistSq);

        // Max Range Check
        if (Dist > WeaponData->MaxRange)
        {
            continue;
        }

        // Calculate Score
        float RangeScore = Dist * WeaponData->RangeWeight;
        float InFrontScore = 0.0f;

        // InFront Score
        if (bIsMoving)
        {
            FVector DirToEnemy = (Enemy->GetActorLocation() - MyLoc).GetSafeNormal();
            float Dot = FVector::DotProduct(MoveDir, DirToEnemy);
            InFrontScore = Dot * WeaponData->InFrontWeight;
        }
        
        float TotalScore = RangeScore + InFrontScore;

        // Debug Logging
        FString DebugMsg = FString::Printf(TEXT("Enemy: %s | Dist: %.0f (Score: %.0f) | InFront: %.2f (Score: %.0f) | Total: %.0f"), 
            *Enemy->GetName(), Dist, RangeScore, (bIsMoving ? InFrontScore/WeaponData->InFrontWeight : 0.0f), InFrontScore, TotalScore);
        
        UE_LOG(LogTemp, Log, TEXT("%s"), *DebugMsg);

        // Visual Debug
        FColor DebugColor = FColor::Red;
        if (TotalScore > BestScore)
        {
            DebugColor = FColor::Green;
        }
        
        // DrawDebugLine(GetWorld(), MyLoc, Enemy->GetActorLocation(), DebugColor, false, 1.0f, 0, 2.0f);
        // DrawDebugString(GetWorld(), Enemy->GetActorLocation() + FVector(0,0,50), DebugMsg, nullptr, DebugColor, 2.0f);

        if (TotalScore > BestScore)
        {
            BestScore = TotalScore;
            BestTarget = Enemy;
        }
    }

    if (!BestTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("ASurvivorWeapon::FindBestTarget - %d enemies found but none in range (MaxRange: %.0f)"), Enemies.Num(), WeaponData->MaxRange);
    }

    return BestTarget;
}
