#include "EnemySpawnSubsystem.h"
#include "SurvivorEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "EngineUtils.h"

void UEnemySpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Auto-start spawning after short delay to ensure world is ready
	if (UWorld* World = GetWorld())
	{
		FTimerHandle DelayHandle;
		World->GetTimerManager().SetTimer(
			DelayHandle,
			this,
			&UEnemySpawnSubsystem::StartSpawning,
			0.5f,  // Half second delay
			false
		);
	}
}

void UEnemySpawnSubsystem::Deinitialize()
{
	StopSpawning();

	// Clear pools (actors will be cleaned up by world)
	EnemyPool.Empty();
	ActiveEnemies.Empty();

	Super::Deinitialize();
}

bool UEnemySpawnSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create for game worlds, not editor preview
	UWorld* World = Cast<UWorld>(Outer);
	if (World && World->IsGameWorld())
	{
		return true;
	}
	return false;
}

void UEnemySpawnSubsystem::Configure(TSubclassOf<ASurvivorEnemy> InEnemyClass, UDataTable* InEnemyDataTable, UDataTable* InSpawnConfigTable)
{
	EnemyClass = InEnemyClass;
	EnemyDataTable = InEnemyDataTable;
	SpawnConfigTable = InSpawnConfigTable;
	bIsConfigured = true;
}

void UEnemySpawnSubsystem::LoadDefaultAssets()
{
	// Try to load default assets by path if not already set
	if (!EnemyClass)
	{
		// Load B_BaseEnemy Blueprint class
		EnemyClass = LoadClass<ASurvivorEnemy>(nullptr, TEXT("/Game/Enemies/B_BaseEnemy.B_BaseEnemy_C"));
	}

	if (!EnemyDataTable)
	{
		// Load DT_Enemies DataTable
		EnemyDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Enemies/Data/DT_Enemies.DT_Enemies"));
	}

	if (!SpawnConfigTable)
	{
		// Load DT_EnemySpawns DataTable (optional)
		SpawnConfigTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Enemies/Data/DT_EnemySpawns.DT_EnemySpawns"));
	}
}

void UEnemySpawnSubsystem::StartSpawning()
{
	// Try to load defaults if not configured
	if (!bIsConfigured)
	{
		LoadDefaultAssets();
	}

	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemySpawnSubsystem: No EnemyClass set! Configure via Blueprint or create B_BaseEnemy at /Game/Enemies/"));
		return;
	}

	if (!EnemyDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemySpawnSubsystem: No EnemyDataTable set! Create DT_Enemies at /Game/Enemies/"));
		return;
	}

	// Cache floor bounds for spawn location clamping
	CacheFloorBounds();

	// Pre-warm the pool
	PreWarmPool(PreWarmCount);

	// Start spawn timer
	SpawnEnemy();
}

void UEnemySpawnSubsystem::StopSpawning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void UEnemySpawnSubsystem::PreWarmPool(int32 Count)
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		return;
	}

	for (int32 i = 0; i < Count; i++)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASurvivorEnemy* Enemy = World->SpawnActor<ASurvivorEnemy>(
			EnemyClass,
			FVector(0.0f, 0.0f, 100.0f),  // Spawn at valid Z (floor is at Z=0)
			FRotator::ZeroRotator,
			Params
		);

		if (Enemy)
		{
			Enemy->Deactivate();
			EnemyPool.Add(Enemy);
		}
	}
}

ASurvivorEnemy* UEnemySpawnSubsystem::GetEnemyFromPool()
{
	if (EnemyPool.Num() > 0)
	{
		ASurvivorEnemy* Enemy = EnemyPool.Pop();
		ActiveEnemies.Add(Enemy);
		return Enemy;
	}

	// Pool empty - spawn new enemy
	UWorld* World = GetWorld();
	if (World && EnemyClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASurvivorEnemy* Enemy = World->SpawnActor<ASurvivorEnemy>(
			EnemyClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params
		);

		if (Enemy)
		{
			Enemy->Deactivate();  // Start deactivated, will be reinitialized
			ActiveEnemies.Add(Enemy);
			return Enemy;
		}
	}

	return nullptr;
}

void UEnemySpawnSubsystem::ReturnEnemyToPool(ASurvivorEnemy* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	ActiveEnemies.Remove(Enemy);
	Enemy->Deactivate();
	EnemyPool.Add(Enemy);
}

void UEnemySpawnSubsystem::OnEnemyDeath(ASurvivorEnemy* Enemy)
{
	// Return to pool after short delay (allows death effects)
	ReturnEnemyToPool(Enemy);
}

void UEnemySpawnSubsystem::SpawnEnemy()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: No world!"));
		return;
	}

	// Check enemy cap
	if (ActiveEnemies.Num() < MaxEnemiesOnMap)
	{
		// Select enemy type first - skip spawn if no valid type
		FName EnemyType = SelectEnemyType();
		if (EnemyType.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnEnemy: No valid enemy type available"));
		}
		else
		{
			// Get enemy from pool
			ASurvivorEnemy* Enemy = GetEnemyFromPool();
			if (Enemy)
			{
				FVector Location = GetSpawnLocation();
				Enemy->Reinitialize(EnemyDataTable, EnemyType, Location);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("SpawnEnemy: Failed to get enemy from pool"));
			}
		}
	}

	// Schedule next spawn
	float SpawnRate = GetCurrentSpawnRate();
	float Interval = 60.0f / SpawnRate;  // Convert spawns/min to seconds

	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&UEnemySpawnSubsystem::SpawnEnemy,
		Interval,
		false  // Not looping - we recalculate each time
	);

	// Update debug display
	UpdateDebugHUD();
}

FVector UEnemySpawnSubsystem::GetSpawnLocation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FVector::ZeroVector;
	}

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player)
	{
		return FVector::ZeroVector;
	}

	FVector PlayerLoc = Player->GetActorLocation();

	// Random angle around player
	float Angle = FMath::RandRange(0.0f, 360.0f);
	float Distance = SpawnRadius + FMath::RandRange(0.0f, SpawnMargin);

	FVector Offset;
	Offset.X = FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance;
	Offset.Y = FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance;
	Offset.Z = 0.0f;

	FVector SpawnLoc = PlayerLoc + Offset;

	// Clamp to floor bounds if available
	return ClampToFloorBounds(SpawnLoc);
}

FName UEnemySpawnSubsystem::SelectEnemyType()
{
	// If no spawn config, return first row from enemy table
	if (!SpawnConfigTable)
	{
		if (EnemyDataTable)
		{
			TArray<FName> RowNames = EnemyDataTable->GetRowNames();
			if (RowNames.Num() > 0)
			{
				return RowNames[0];
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SelectEnemyType: Both SpawnConfigTable and EnemyDataTable are NULL!"));
		}
		return NAME_None;
	}

	UWorld* World = GetWorld();
	float ElapsedMinutes = World ? World->GetTimeSeconds() / 60.0f : 0.0f;

	// Gather available enemy types (unlocked based on time)
	TArray<FEnemySpawnEntry*> Available;
	float TotalWeight = 0.0f;

	TArray<FName> RowNames = SpawnConfigTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FEnemySpawnEntry* Entry = SpawnConfigTable->FindRow<FEnemySpawnEntry>(RowName, TEXT(""));
		if (Entry && ElapsedMinutes >= Entry->MinuteUnlock)
		{
			// Check if deprecated (0 means never expires)
			if (Entry->MinuteDeprecate > 0.0f && ElapsedMinutes >= Entry->MinuteDeprecate)
			{
				continue;  // Skip deprecated enemy types
			}
			Available.Add(Entry);
			TotalWeight += Entry->Weight;
		}
	}

	// Weighted random selection
	if (Available.Num() > 0 && TotalWeight > 0.0f)
	{
		float Roll = FMath::RandRange(0.0f, TotalWeight);
		float Cumulative = 0.0f;

		for (FEnemySpawnEntry* Entry : Available)
		{
			Cumulative += Entry->Weight;
			if (Roll <= Cumulative)
			{
				return Entry->EnemyRowName;
			}
		}

		// Fallback to last
		return Available.Last()->EnemyRowName;
	}

	return NAME_None;
}

float UEnemySpawnSubsystem::GetCurrentSpawnRate()
{
	UWorld* World = GetWorld();
	float ElapsedMinutes = World ? World->GetTimeSeconds() / 60.0f : 0.0f;

	// Base rate increases over time
	float BaseRate = BaseSpawnRate + (ElapsedMinutes * SpawnRateGrowth);

	// Responsive bonus - spawn faster when fewer enemies on map
	float TargetCount = GetCurrentTargetCount();
	int32 CurrentCount = ActiveEnemies.Num();
	float EnemyDeficit = (TargetCount > 0.0f) ? FMath::Max(0.0f, TargetCount - CurrentCount) / TargetCount : 0.0f;
	float ResponsiveBonus = MaxResponsiveBonus * EnemyDeficit * Responsiveness;

	// Apply cap
	return FMath::Min(BaseRate + ResponsiveBonus, MaxSpawnRate);
}

float UEnemySpawnSubsystem::GetCurrentTargetCount()
{
	UWorld* World = GetWorld();
	float ElapsedMinutes = World ? World->GetTimeSeconds() / 60.0f : 0.0f;

	// Target count increases over time, capped at MaxTargetCount
	float TargetCount = BaseTargetCount + (ElapsedMinutes * TargetCountGrowth);
	return FMath::Min(TargetCount, MaxTargetCount);
}

void UEnemySpawnSubsystem::CacheFloorBounds()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Build tag — change this string when you edit this function so you can verify in the
	// log that the latest compiled version is actually loaded (Live Coding silently fails
	// sometimes).
	UE_LOG(LogTemp, Log, TEXT("===== EnemySpawnSubsystem::CacheFloorBounds build=2026-05-17e ====="));

	// Match against name, label, OR class name — covers BSP brushes, static-mesh actors,
	// Blueprint instances. Case-insensitive.
	auto MatchesFloor = [](const FString& S) -> bool
	{
		return S.Contains(TEXT("LevelFloor"), ESearchCase::IgnoreCase);
	};

	// Always dump every brush we see — this is the user-confirmed actor type, and there's
	// usually <5 of them, so the log volume is fine.
	TArray<AActor*> AllBrushes;
	for (TActorIterator<ABrush> BrushIt(World); BrushIt; ++BrushIt)
	{
		if (ABrush* B = *BrushIt) { AllBrushes.Add(B); }
	}
	UE_LOG(LogTemp, Log, TEXT("EnemySpawnSubsystem: Brushes in level (%d):"), AllBrushes.Num());
	for (AActor* B : AllBrushes)
	{
		FVector O, E;
		B->GetActorBounds(false, O, E);
		UE_LOG(LogTemp, Log,
			TEXT("    name='%s' label='%s' class='%s' origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)"),
			*B->GetName(), *B->GetActorLabel(),
			B->GetClass() ? *B->GetClass()->GetName() : TEXT("?"),
			O.X, O.Y, O.Z, E.X, E.Y, E.Z);
	}

	// Two-pass: first try brushes (the confirmed floor type), then any other actor.
	auto TryMatch = [&](AActor* Actor) -> bool
	{
		if (!Actor) return false;
		const FString ActorName  = Actor->GetName();
		const FString ActorLabel = Actor->GetActorLabel();
		const FString ClassName  = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
		if (!MatchesFloor(ActorName) && !MatchesFloor(ActorLabel) && !MatchesFloor(ClassName))
		{
			return false;
		}

		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);
		FloorBounds = FBox(Origin - Extent, Origin + Extent);
		bHasFloorBounds = true;
		FloorActor = Actor;

		UE_LOG(LogTemp, Log,
			TEXT("EnemySpawnSubsystem: ✓ Matched floor actor '%s' (label='%s', class='%s'), bounds origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)"),
			*ActorName, *ActorLabel, *ClassName,
			Origin.X, Origin.Y, Origin.Z, Extent.X, Extent.Y, Extent.Z);
		return true;
	};

	for (AActor* B : AllBrushes)
	{
		if (TryMatch(B)) { return; }
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (TryMatch(*It)) { return; }
	}

	UE_LOG(LogTemp, Warning,
		TEXT("EnemySpawnSubsystem: ✗ No actor matching 'LevelFloor' (case-insensitive, name/label/class) found across %d brushes and all other actors."),
		AllBrushes.Num());
}

FVector UEnemySpawnSubsystem::ClampToFloorBounds(FVector Location)
{
	if (!bHasFloorBounds)
	{
		return Location;
	}

	// Apply safety margin
	FBox SafeBounds = FloorBounds.ExpandBy(-FloorBoundsMargin);

	// Clamp X and Y to safe bounds (keep original Z)
	Location.X = FMath::Clamp(Location.X, SafeBounds.Min.X, SafeBounds.Max.X);
	Location.Y = FMath::Clamp(Location.Y, SafeBounds.Min.Y, SafeBounds.Max.Y);

	return Location;
}

void UEnemySpawnSubsystem::UpdateDebugHUD()
{
	if (!bShowDebugHUD || !GEngine)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float ElapsedSeconds = World->GetTimeSeconds();
	int32 Minutes = FMath::FloorToInt(ElapsedSeconds / 60.0f);
	int32 Seconds = FMath::FloorToInt(FMath::Fmod(ElapsedSeconds, 60.0f));
	float ElapsedMinutes = ElapsedSeconds / 60.0f;

	// Calculate spawn rate components
	float TimeBasedRate = BaseSpawnRate + (ElapsedMinutes * SpawnRateGrowth);
	float TargetCount = GetCurrentTargetCount();
	int32 CurrentCount = ActiveEnemies.Num();
	float EnemyDeficit = (TargetCount > 0.0f) ? FMath::Max(0.0f, TargetCount - CurrentCount) / TargetCount : 0.0f;
	float ResponsiveBonus = MaxResponsiveBonus * EnemyDeficit * Responsiveness;
	float TotalRate = FMath::Min(TimeBasedRate + ResponsiveBonus, MaxSpawnRate);
	float SpawnsPerSecond = TotalRate / 60.0f;

	// Find latest unlocked and active enemy types
	FString UnlockedTypes = TEXT("");
	FString ActiveTypes = TEXT("");
	if (SpawnConfigTable)
	{
		TArray<FName> RowNames = SpawnConfigTable->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FEnemySpawnEntry* Entry = SpawnConfigTable->FindRow<FEnemySpawnEntry>(RowName, TEXT(""));
			if (Entry)
			{
				bool bUnlocked = ElapsedMinutes >= Entry->MinuteUnlock;
				bool bDeprecated = Entry->MinuteDeprecate > 0.0f && ElapsedMinutes >= Entry->MinuteDeprecate;

				if (bUnlocked && !bDeprecated)
				{
					if (ActiveTypes.Len() > 0) ActiveTypes += TEXT(", ");
					ActiveTypes += Entry->EnemyRowName.ToString();
				}
				else if (bUnlocked && bDeprecated)
				{
					// Show deprecated with strikethrough-ish
				}

				if (bUnlocked && UnlockedTypes.Len() == 0)
				{
					UnlockedTypes = Entry->EnemyRowName.ToString();
				}
				else if (bUnlocked)
				{
					UnlockedTypes = Entry->EnemyRowName.ToString();  // Keep updating to get latest
				}
			}
		}
	}
	else if (EnemyDataTable)
	{
		TArray<FName> RowNames = EnemyDataTable->GetRowNames();
		if (RowNames.Num() > 0)
		{
			ActiveTypes = RowNames[0].ToString();
		}
	}

	bool bAtCap = CurrentCount >= MaxEnemiesOnMap;

	// Build debug strings - use different keys for each line so they don't overwrite
	GEngine->AddOnScreenDebugMessage(100, 0.5f, FColor::Cyan,
		FString::Printf(TEXT("=== SPAWN SYSTEM DEBUG ===")));

	GEngine->AddOnScreenDebugMessage(101, 0.5f, FColor::White,
		FString::Printf(TEXT("Time: %02d:%02d (%.1f min)"), Minutes, Seconds, ElapsedMinutes));

	GEngine->AddOnScreenDebugMessage(102, 0.5f, bAtCap ? FColor::Red : FColor::Green,
		FString::Printf(TEXT("Enemies: %d / %d %s"), CurrentCount, MaxEnemiesOnMap, bAtCap ? TEXT("[AT CAP]") : TEXT("")));

	GEngine->AddOnScreenDebugMessage(103, 0.5f, FColor::White,
		FString::Printf(TEXT("Pool: %d available"), EnemyPool.Num()));

	GEngine->AddOnScreenDebugMessage(104, 0.5f, FColor::Yellow,
		FString::Printf(TEXT("Spawn Rate: %.1f/min (%.2f/sec)"), TotalRate, SpawnsPerSecond));

	GEngine->AddOnScreenDebugMessage(105, 0.5f, FColor::White,
		FString::Printf(TEXT("  Base+Time: %.1f/min"), TimeBasedRate));

	GEngine->AddOnScreenDebugMessage(106, 0.5f, ResponsiveBonus > 10.0f ? FColor::Orange : FColor::White,
		FString::Printf(TEXT("  Responsive: +%.1f/min (target: %.0f, deficit: %.0f%%)"), ResponsiveBonus, TargetCount, EnemyDeficit * 100.0f));

	GEngine->AddOnScreenDebugMessage(107, 0.5f, FColor::Cyan,
		FString::Printf(TEXT("Active Types: %s"), ActiveTypes.Len() > 0 ? *ActiveTypes : TEXT("(none)")));
}
