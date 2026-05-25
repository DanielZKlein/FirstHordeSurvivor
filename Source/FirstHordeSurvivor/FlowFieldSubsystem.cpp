#include "FlowFieldSubsystem.h"
#include "EnemySpawnSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// Tag on any actor (or component) that should NEVER be treated as a wall by the flow field
// obstacle scan. Add this tag in the editor (Actor → Tags, or Component → Tags) to the floor,
// decorative volumes, decals, etc. The flow field skips any overlap hit whose actor or
// component has this tag.
static const FName FlowFieldIgnoreTag(TEXT("FlowFieldIgnore"));

// Runtime kill-switch — `ff.enable 0` falls SampleDirection back to ZeroVector everywhere,
// which lets the SurvivorEnemy chase block revert to the legacy direct-steering path
// without recompiling. Useful for A/B comparing flow field vs no flow field in PIE.
static TAutoConsoleVariable<int32> CVarFlowFieldEnabled(
	TEXT("ff.enable"),
	1,
	TEXT("Enable flow field path sampling (0 = enemies steer directly at player)"),
	ECVF_Default);

// Debug cvar — toggles flow arrow + blocked cell visualization in PIE.
//   ff.debug 0 = off
//   ff.debug 1 = arrows
//   ff.debug 2 = arrows + blocked cells (red boxes)
static TAutoConsoleVariable<int32> CVarFlowFieldDebug(
	TEXT("ff.debug"),
	0,
	TEXT("Flow field debug visualization (0=off, 1=arrows, 2=arrows+blocked)"),
	ECVF_Cheat);

// How many cells to skip between arrows when drawing debug. Higher = sparser.
static TAutoConsoleVariable<int32> CVarFlowFieldDebugStride(
	TEXT("ff.debugstride"),
	2,
	TEXT("Stride for flow field debug arrows (draw every Nth cell)"),
	ECVF_Cheat);

// Console commands — let the user re-scan obstacles after spawning walls in-PIE, or force
// a rebuild when poking at the system manually.
static FAutoConsoleCommandWithWorld GFlowFieldRebuildCmd(
	TEXT("ff.rebuild"),
	TEXT("Force flow field rebuild now"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (UFlowFieldSubsystem* FF = World->GetSubsystem<UFlowFieldSubsystem>())
			{
				FF->ForceRebuild();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GFlowFieldRefreshCmd(
	TEXT("ff.refresh"),
	TEXT("Re-scan obstacles and rebuild the flow field"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (UFlowFieldSubsystem* FF = World->GetSubsystem<UFlowFieldSubsystem>())
			{
				FF->RefreshBlockedCells();
				FF->ForceRebuild();
			}
		}
	}));

bool UFlowFieldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create for live game worlds — skip editor preview, asset thumbnail worlds, etc.
	UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UFlowFieldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// We need the EnemySpawnSubsystem to come up first because we read its cached floor bounds.
	Collection.InitializeDependency(UEnemySpawnSubsystem::StaticClass());

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Console variables persist across PIE sessions. A stale "ff.debug 2" from an earlier
	// test would draw hundreds of debug shapes every tick the moment PIE comes up, which
	// looks identical to a "lockup" if the user forgot they enabled it. Force-clear on init.
	if (CVarFlowFieldDebug.GetValueOnGameThread() != 0)
	{
		const int32 PrevValue = CVarFlowFieldDebug.GetValueOnGameThread();
		CVarFlowFieldDebug.AsVariable()->Set(0, ECVF_SetByConsole);
		UE_LOG(LogTemp, Warning,
			TEXT("FlowField: cleared stale ff.debug=%d from previous PIE session. Re-enable manually if you want debug visualization."),
			PrevValue);
	}

	// Kick off the rebuild timer. The first tick will lazy-init the grid as soon as
	// EnemySpawnSubsystem has finished caching FloorBounds (it caches inside a delayed
	// StartSpawning callback ~0.5s after world begin).
	World->GetTimerManager().SetTimer(
		RebuildTimerHandle,
		this,
		&UFlowFieldSubsystem::TickRebuild,
		UpdateInterval,
		true,
		0.5f /* first-fire delay */);
}

void UFlowFieldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildTimerHandle);
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	FlowDirections.Empty();
	BlockedCells.Empty();
	CellCosts.Empty();
	CurrentLayer.Empty();
	NextLayer.Empty();
	bGridInitialized = false;
	bGridDimsReady = false;
	bScanInProgress = false;
	bGridCenterValid = false;

	Super::Deinitialize();
}

void UFlowFieldSubsystem::TickRebuild()
{
	// Master kill switch — `ff.enable 0` skips BFS, scan setup, debug draw entirely.
	// With this set, the flow field does ZERO work per frame and SampleDirection returns
	// ZeroVector immediately. Useful for A/B testing whether the flow field is the cause
	// of a perf issue.
	if (CVarFlowFieldEnabled.GetValueOnGameThread() == 0)
	{
		// Stop the scan timer if it was running — no point chunking work nobody needs.
		if (bScanInProgress)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(ScanTimerHandle);
			}
			bScanInProgress = false;
		}
		return;
	}

	// While the obstacle scan is chunking, skip BFS — flow field returns ZeroVector and
	// enemies fall back to direct steering. The scan has its own faster timer; this rebuild
	// timer just resumes BFS once the scan finishes.
	if (!bGridInitialized)
	{
		// Grid dims may not even be set up yet (first ~0.5s of play, waiting for player).
		// Try setup if we haven't started yet.
		if (!bGridDimsReady && !bScanInProgress)
		{
			TryInitializeGrid();
		}
		return;
	}

	// Active region follows the player — recenter + rescan when they've moved far enough
	// from the current grid center. CheckRecenter returns true if it kicked off a new scan,
	// in which case we skip BFS this tick (scan needs to complete first).
	if (CheckRecenter())
	{
		return;
	}

	RebuildFlowField();

	if (CVarFlowFieldDebug.GetValueOnGameThread() > 0)
	{
		DrawDebug();
	}
}

void UFlowFieldSubsystem::TryInitializeGrid()
{
	if (bGridInitialized || bScanInProgress)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// We need a player to anchor the active region. Without one, retry next tick.
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player)
	{
		return;
	}

	const FVector PL = Player->GetActorLocation();
	GridCenter = FVector2D(PL.X, PL.Y);
	bGridCenterValid = true;

	// Active region = square around the player. Optionally clamp to the level floor bounds
	// (if available) so we don't waste scan budget on the void outside the playable area.
	const float Half = FMath::Max(100.0f, ActiveHalfExtent);
	FBox Bounds(
		FVector(PL.X - Half, PL.Y - Half, 0.0f),
		FVector(PL.X + Half, PL.Y + Half, 0.0f));

	if (UEnemySpawnSubsystem* SpawnSub = World->GetSubsystem<UEnemySpawnSubsystem>())
	{
		if (SpawnSub->HasFloorBounds())
		{
			const FBox FB = SpawnSub->GetFloorBounds();
			Bounds.Min.X = FMath::Max(Bounds.Min.X, FB.Min.X);
			Bounds.Min.Y = FMath::Max(Bounds.Min.Y, FB.Min.Y);
			Bounds.Max.X = FMath::Min(Bounds.Max.X, FB.Max.X);
			Bounds.Max.Y = FMath::Min(Bounds.Max.Y, FB.Max.Y);
		}
	}

	const float Width  = Bounds.Max.X - Bounds.Min.X;
	const float Height = Bounds.Max.Y - Bounds.Min.Y;

	// Use the requested CellSize directly; auto-upscale only as a safety against absurdly
	// large active extents. With the default ActiveHalfExtent=2500 and CellSize=80 this
	// gives a ~63×63 grid (4000 cells) — much smaller than the previous 96×96 full-level grid,
	// AND with the originally-requested smaller cells for finer pathing.
	const float MinCellForWidth  = Width  / FMath::Max(1, MaxCellsPerSide);
	const float MinCellForHeight = Height / FMath::Max(1, MaxCellsPerSide);
	EffectiveCellSize = FMath::Max3(CellSize, MinCellForWidth, MinCellForHeight);

	GridWidth  = FMath::Max(1, FMath::CeilToInt(Width  / EffectiveCellSize));
	GridHeight = FMath::Max(1, FMath::CeilToInt(Height / EffectiveCellSize));
	GridOrigin = FVector(Bounds.Min.X, Bounds.Min.Y, 0.0f);

	// Probe Z plane — well above the floor surface but below typical wall tops. Derived from
	// player capsule feet position so it's correct regardless of floor brush Z thickness.
	const float WalkProbeOffset = ObstacleProbeRadius + 15.0f;
	float HalfHeight = 96.0f;
	if (UCapsuleComponent* Cap = Player->GetCapsuleComponent())
	{
		HalfHeight = Cap->GetScaledCapsuleHalfHeight();
	}
	SampleZ = (PL.Z - HalfHeight) + WalkProbeOffset;

	const int32 N = GridWidth * GridHeight;
	FlowDirections.SetNumZeroed(N);
	BlockedCells.Init(false, N);
	CellCosts.SetNum(N);
	CurrentLayer.Reserve(FMath::Max(1024, N / 4));
	NextLayer.Reserve(FMath::Max(1024, N / 4));

	bGridDimsReady = true;

	UE_LOG(LogTemp, Log,
		TEXT("FlowField: grid centered on player (%.0f,%.0f), dims %dx%d (%d cells), cell=%.0f (req %.0f), origin=(%.0f,%.0f), sampleZ=%.0f, activeHalfExtent=%.0f"),
		PL.X, PL.Y, GridWidth, GridHeight, N, EffectiveCellSize, CellSize, GridOrigin.X, GridOrigin.Y, SampleZ, Half);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("FlowField: scanning %dx%d (%d cells), SampleZ=%.0f..."),
				GridWidth, GridHeight, N, SampleZ));
	}

	StartObstacleScan();
}

bool UFlowFieldSubsystem::CheckRecenter()
{
	if (!bGridCenterValid)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (!CachedPlayer.IsValid())
	{
		CachedPlayer = UGameplayStatics::GetPlayerCharacter(World, 0);
		if (!CachedPlayer.IsValid())
		{
			return false;
		}
	}

	const FVector PL = CachedPlayer->GetActorLocation();
	const FVector2D PlayerXY(PL.X, PL.Y);
	const float Dist = FVector2D::Distance(PlayerXY, GridCenter);
	if (Dist <= RecenterDeadzone)
	{
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("FlowField: recentering grid — player moved %.0f UU from center (%.0f,%.0f → %.0f,%.0f), deadzone=%.0f"),
		Dist, GridCenter.X, GridCenter.Y, PlayerXY.X, PlayerXY.Y, RecenterDeadzone);

	// Cancel any in-progress scan; we're starting a new one from the new center.
	if (bScanInProgress)
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
		bScanInProgress = false;
	}
	bGridInitialized = false;
	bGridDimsReady = false;
	TryInitializeGrid();
	return true;
}

void UFlowFieldSubsystem::StartObstacleScan()
{
	UWorld* World = GetWorld();
	if (!World || !bGridDimsReady)
	{
		return;
	}

	// Reset state and zero out blocked cells before re-scanning.
	BlockedCells.Init(false, GridWidth * GridHeight);
	ScanCursor = 0;
	ScanBlockedAccum = 0;
	bScanInProgress = true;
	ScanActorBlockCounts.Reset();
	ScanLoggedActorNames.Reset();

	// Run scan on its own faster timer (ScanTickInterval) so it completes in reasonable
	// wall-clock time. Each tick is wall-clock budgeted via ScanBudgetSeconds, so the game
	// stays responsive while the scan progresses.
	World->GetTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&UFlowFieldSubsystem::ProcessScanChunk,
		FMath::Max(0.001f, ScanTickInterval),
		true);
}

void UFlowFieldSubsystem::ProcessScanChunk()
{
	UWorld* World = GetWorld();
	if (!World || !bScanInProgress)
	{
		return;
	}

	// Box probe at walking height — covers the full cell footprint with no inter-cell gaps.
	// Scale the XY half-extents to EffectiveCellSize so every point on the floor is covered
	// regardless of how much the grid was auto-upscaled from the requested CellSize.
	// Use FMath::Max so a tiny hand-set ObstacleProbeRadius can still widen the box on
	// small grids, but the box is never smaller than the cell demands.
	const float EffectiveRadius = FMath::Max(ObstacleProbeRadius, EffectiveCellSize * 0.55f);
	// Z half-extent: use SampleZ - 10.0f so the box bottom stays ~10 UU above the floor
	// (Z=0). With SampleZ≈57 this gives half-extent≈47, covering Z=10 to Z=104 — safely
	// above the floor brush (Z≈±5) while still catching wall geometry at knee height and up.
	// The old hard-coded 100 UU punched down to Z=-43, hitting the floor brush in every cell.
	const float WallProbeZHalfExtent = FMath::Max(10.0f, SampleZ - 10.0f);
	const FCollisionShape WallProbe = FCollisionShape::MakeBox(
		FVector(EffectiveCellSize * 0.5f, EffectiveCellSize * 0.5f, WallProbeZHalfExtent));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlowFieldObstacle), false);
	Params.bTraceComplex = false;

	// Explicitly ignore the floor actor — the LevelFloor brush sometimes has Z extent that
	// stretches well above Z=0 (depends on level setup), and its bounds may overlap the probe
	// height even with the SampleZ adjustment. Belt-and-suspenders.
	AActor* FloorActorPtr = nullptr;
	if (UEnemySpawnSubsystem* SpawnSub = World->GetSubsystem<UEnemySpawnSubsystem>())
	{
		FloorActorPtr = SpawnSub->GetFloorActor();
		if (FloorActorPtr)
		{
			Params.AddIgnoredActor(FloorActorPtr);
		}
	}

	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

	const int32 N = GridWidth * GridHeight;
	const double StartTime = FPlatformTime::Seconds();
	const double Budget = FMath::Max(0.001, (double)ScanBudgetSeconds);

	// Log the effective probe geometry once per scan chunk start (ScanCursor == 0 means
	// this is the very first chunk of a fresh scan). Also log floor actor info so we can
	// see whether the exclusion took effect.
	if (ScanCursor == 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("FlowField: effective probe radius = %.0f (raw ObstacleProbeRadius=%.0f, cellSize=%.0f); using box probe (%.0f x %.0f x %.0f), sampleZ=%.0f, probeZ range=[%.0f, %.0f]"),
			EffectiveRadius, ObstacleProbeRadius, EffectiveCellSize,
			EffectiveCellSize * 0.5f, EffectiveCellSize * 0.5f, WallProbeZHalfExtent, SampleZ,
			SampleZ - WallProbeZHalfExtent, SampleZ + WallProbeZHalfExtent);

		if (FloorActorPtr)
		{
			FVector FOrigin, FExtent;
			FloorActorPtr->GetActorBounds(false, FOrigin, FExtent);
			UE_LOG(LogTemp, Log,
				TEXT("FlowField: ignoring floor actor '%s' (class='%s'), bounds Z=[%.0f, %.0f]"),
				*FloorActorPtr->GetName(),
				FloorActorPtr->GetClass() ? *FloorActorPtr->GetClass()->GetName() : TEXT("?"),
				FOrigin.Z - FExtent.Z, FOrigin.Z + FExtent.Z);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FlowField: no floor actor available from EnemySpawnSubsystem — floor mesh may be marking cells as blocked!"));
		}
	}

	// Process cells in row-major order until we either finish or burn our wall-clock budget.
	// Time-based budgeting yields more consistent frame pacing than count-based. Check the
	// budget every cell — FPlatformTime::Seconds() is cheap (~50 ns) and a single overlap
	// query against complex geo can exceed the entire chunk budget, so we don't want to
	// process several before noticing.
	//
	// Use OverlapMulti so we can: (a) skip hits tagged FlowFieldIgnore on the actor or its
	// component, (b) log the actor/class for the first ~30 blocks in the scan so the user
	// can quickly verify what's actually marking cells. Cost is marginally higher than
	// OverlapAnyTest but the diagnostic value is high while debugging.
	int32 CellsThisChunk = 0;
	TArray<FOverlapResult> Overlaps;
	while (ScanCursor < N)
	{
		const int32 X = ScanCursor % GridWidth;
		const int32 Y = ScanCursor / GridWidth;
		FVector CellCenter = CellToWorld(X, Y);
		CellCenter.Z = SampleZ;

		Overlaps.Reset();
		const bool bAnyOverlap = World->OverlapMultiByObjectType(
			Overlaps, CellCenter, FQuat::Identity, ObjQuery, WallProbe, Params);

		if (bAnyOverlap)
		{
			// Walk hits and find the first one that isn't a pawn / isn't tagged
			// FlowFieldIgnore. If every hit is filtered out, the cell stays open.
			//   - Pawns (players, enemies) are explicitly excluded from pathfinding: enemy-
			//     vs-enemy separation is handled separately, and enemies' AttackOverlapComp
			//     uses ObjectType=WorldDynamic so it WOULD register here without this filter.
			//   - FlowFieldIgnore tag lets you mark any actor or component (floor, decals,
			//     trigger volumes, etc.) as "don't treat as a wall".
			AActor* BlockingActor = nullptr;
			UPrimitiveComponent* BlockingComp = nullptr;
			for (const FOverlapResult& O : Overlaps)
			{
				AActor* HitActor = O.GetActor();
				UPrimitiveComponent* HitComp = O.GetComponent();
				if (!HitActor)
				{
					continue;
				}
				if (HitActor->IsA<APawn>())
				{
					continue; // pawns never block the flow field
				}
				const bool bActorIgnored = HitActor->ActorHasTag(FlowFieldIgnoreTag);
				const bool bCompIgnored  = HitComp && HitComp->ComponentHasTag(FlowFieldIgnoreTag);
				if (bActorIgnored || bCompIgnored)
				{
					continue;
				}
				BlockingActor = HitActor;
				BlockingComp  = HitComp;
				break;
			}

			if (BlockingActor)
			{
				BlockedCells[ScanCursor] = true;
				ScanBlockedAccum++;

				// Track per-actor block counts. Emit a detailed log line the FIRST time each
				// actor blocks anything (cheap, bounded by # distinct blocking actors).
				const FName ActorKey = BlockingActor->GetFName();
				ScanActorBlockCounts.FindOrAdd(ActorKey)++;
				if (!ScanLoggedActorNames.Contains(ActorKey))
				{
					ScanLoggedActorNames.Add(ActorKey);
					FVector BoundsOrigin, BoundsExtent;
					BlockingActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
					UE_LOG(LogTemp, Log,
						TEXT("FlowField: NEW blocker — actor='%s' label='%s' class='%s' comp='%s' actorBoundsZ=[%.0f, %.0f] firstCell=(%d,%d) world=(%.0f,%.0f,%.0f)"),
						*BlockingActor->GetName(),
						*BlockingActor->GetActorLabel(),
						BlockingActor->GetClass() ? *BlockingActor->GetClass()->GetName() : TEXT("?"),
						BlockingComp ? *BlockingComp->GetName() : TEXT("?"),
						BoundsOrigin.Z - BoundsExtent.Z, BoundsOrigin.Z + BoundsExtent.Z,
						X, Y,
						CellCenter.X, CellCenter.Y, CellCenter.Z);
				}
			}
		}

		ScanCursor++;
		CellsThisChunk++;

		if ((FPlatformTime::Seconds() - StartTime) >= Budget)
		{
			break;
		}
	}

	// If a single chunk burned more than 2× the budget, log it so the user can tell that
	// per-cell overlap is unusually expensive (complex collision, lots of actors, etc.).
	const double Elapsed = FPlatformTime::Seconds() - StartTime;
	if (Elapsed > Budget * 2.0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FlowField: scan chunk overshot budget (%.1f ms for %d cells, budget %.1f ms). Per-cell cost ~%.0fµs."),
			Elapsed * 1000.0, CellsThisChunk, Budget * 1000.0,
			(CellsThisChunk > 0) ? (Elapsed * 1e6 / CellsThisChunk) : 0.0);
	}

	// Progress indicator. Key=999 so it overwrites itself rather than spamming the screen.
	if (GEngine)
	{
		const float Pct = (N > 0) ? (100.0f * ScanCursor / N) : 100.0f;
		GEngine->AddOnScreenDebugMessage(999, 0.25f, FColor::Yellow,
			FString::Printf(TEXT("FlowField scan: %.0f%% (%d/%d cells, %d blocked)"),
				Pct, ScanCursor, N, ScanBlockedAccum));
	}

	if (ScanCursor >= N)
	{
		// Scan finished — promote dims-ready → fully initialized and kill the scan timer.
		bScanInProgress = false;
		bGridInitialized = true;
		World->GetTimerManager().ClearTimer(ScanTimerHandle);

		// Force the next BFS to actually run (instead of being dedupe'd if the player
		// hasn't moved).
		LastPlayerCell = FIntPoint(INT32_MIN, INT32_MIN);

		const float BlockedPct = (N > 0) ? (100.0f * ScanBlockedAccum / N) : 0.f;
		UE_LOG(LogTemp, Log, TEXT("FlowField: scan done, %d / %d blocked (%.1f%%)"),
			ScanBlockedAccum, N, BlockedPct);

		// Per-actor block summary — sorted high-to-low. Lets the user verify every blocking
		// actor at a glance: expected walls (Cube*, Cylinder*, BspMesh*) should add up to
		// ~ScanBlockedAccum, and any unexpected name (sky sphere, decal, volume) jumps out.
		ScanActorBlockCounts.ValueSort([](int32 A, int32 B) { return A > B; });
		UE_LOG(LogTemp, Log, TEXT("FlowField: blocking-actor summary (%d distinct):"),
			ScanActorBlockCounts.Num());
		for (const TPair<FName, int32>& Pair : ScanActorBlockCounts)
		{
			UE_LOG(LogTemp, Log, TEXT("    %5d cells  %s"),
				Pair.Value, *Pair.Key.ToString());
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green,
				FString::Printf(TEXT("FlowField ready: %dx%d, %d blocked (%.0f%%)"),
					GridWidth, GridHeight, ScanBlockedAccum, BlockedPct));
		}
	}
}

void UFlowFieldSubsystem::RefreshBlockedCells()
{
	if (!bGridDimsReady)
	{
		// Grid not even set up yet — TickRebuild will lazy-init when ready.
		return;
	}
	// If a scan is mid-flight already, let it continue — restarting it would just throw away
	// work. After it completes the caller can invoke RefreshBlockedCells again.
	if (bScanInProgress)
	{
		return;
	}
	bGridInitialized = false; // SampleDirection returns ZeroVector while we rescan
	StartObstacleScan();
}

void UFlowFieldSubsystem::ForceRebuild()
{
	LastPlayerCell = FIntPoint(INT32_MIN, INT32_MIN);
	RebuildFlowField();
}

void UFlowFieldSubsystem::RebuildFlowField()
{
	if (!bGridInitialized)
	{
		TryInitializeGrid();
		if (!bGridInitialized)
			
		{
			return;
		}
	}

	// Refresh player ref if it expired (pooled actors / level transitions)
	if (!CachedPlayer.IsValid())
	{
		CachedPlayer = UGameplayStatics::GetPlayerCharacter(this, 0);
		if (!CachedPlayer.IsValid())
		{
			return;
		}
	}

	const FVector PlayerLoc = CachedPlayer->GetActorLocation();
	FIntPoint PlayerCell = WorldToCell(PlayerLoc);

	// Off-grid — happens if the player walks beyond the cached floor bounds. Skip; enemies
	// will fall back to direct steering until the player is back on-grid.
	if (!IsInBounds(PlayerCell.X, PlayerCell.Y))
	{
		return;
	}

	// If the player's home cell is blocked (somehow standing inside geometry), find the
	// nearest open neighbor. Otherwise BFS from a blocked cell yields a zero field.
	if (BlockedCells[CellIndex(PlayerCell.X, PlayerCell.Y)])
	{
		PlayerCell = NearestUnblockedCell(PlayerCell, /*MaxRings=*/8);
		if (!IsInBounds(PlayerCell.X, PlayerCell.Y))
		{
			return;
		}
	}

	// Bail if the player hasn't crossed a cell boundary since the last rebuild — the field
	// is still correct. Saves ~half of rebuilds at typical walking speed.
	if (PlayerCell == LastPlayerCell)
	{
		return;
	}
	LastPlayerCell = PlayerCell;

	TRACE_CPUPROFILER_EVENT_SCOPE(UFlowFieldSubsystem_BFS);

	const double BfsStart = FPlatformTime::Seconds();
	const int32 N = GridWidth * GridHeight;

	// Reset visited flags. We use CellCosts as a "visited" marker — INT32_MAX = unvisited.
	for (int32 i = 0; i < N; i++)
	{
		CellCosts[i] = INT32_MAX;
	}

	// Two-frontier BFS. Treats edges as uniform cost (8-connected, Chebyshev distance).
	// Visits each open cell exactly once, no heap. Was previously a min-heap Dijkstra with
	// 10/14 integer costs — that should also be fast, but the user was seeing multi-second
	// hitches on a 96×96 grid which only happens if something pathological is going on with
	// the heap path. Strip the heap entirely and we know the work is just N neighbor scans.
	//
	// Path quality cost: paths can prefer diagonals over cardinals where Euclidean would
	// prefer cardinals. For 200u cells in a horde survivor, the visual difference is small.
	CurrentLayer.Reset();
	NextLayer.Reset();

	const int32 StartIdx = CellIndex(PlayerCell.X, PlayerCell.Y);
	CellCosts[StartIdx] = 0;
	FlowDirections[StartIdx] = FVector2D::ZeroVector; // player's own cell has no flow
	CurrentLayer.Add(StartIdx);

	// 8-neighborhood offsets.
	static const int32 DX[8] = {  1, -1,  0,  0,   1,  1, -1, -1 };
	static const int32 DY[8] = {  0,  0,  1, -1,   1, -1,  1, -1 };

	int32 LayerIdx = 0;
	while (CurrentLayer.Num() > 0)
	{
		const int32 NextCost = LayerIdx + 1;
		for (int32 i = 0; i < CurrentLayer.Num(); i++)
		{
			const int32 Idx = CurrentLayer[i];
			const int32 CX = Idx % GridWidth;
			const int32 CY = Idx / GridWidth;

			for (int32 d = 0; d < 8; d++)
			{
				const int32 NX = CX + DX[d];
				const int32 NY = CY + DY[d];
				if (!IsInBounds(NX, NY))
				{
					continue;
				}
				const int32 NIdx = CellIndex(NX, NY);
				if (BlockedCells[NIdx])
				{
					continue;
				}
				if (CellCosts[NIdx] != INT32_MAX)
				{
					continue; // already visited (closer or equal cost)
				}

				// Diagonal corner-cut prevention: a diagonal move only counts if both
				// axis-aligned sides are open. Otherwise enemies clip through corners.
				if (d >= 4)
				{
					const int32 AX1 = CX + DX[d]; const int32 AY1 = CY;
					const int32 AX2 = CX;          const int32 AY2 = CY + DY[d];
					const bool bSide1Blocked = IsInBounds(AX1, AY1) && BlockedCells[CellIndex(AX1, AY1)];
					const bool bSide2Blocked = IsInBounds(AX2, AY2) && BlockedCells[CellIndex(AX2, AY2)];
					if (bSide1Blocked || bSide2Blocked)
					{
						continue;
					}
				}

				CellCosts[NIdx] = NextCost;
				FlowDirections[NIdx] = FVector2D(-DX[d], -DY[d]).GetSafeNormal();
				NextLayer.Add(NIdx);
			}
		}

		// Swap layers (cheap pointer-swap on TArray).
		Swap(CurrentLayer, NextLayer);
		NextLayer.Reset();
		LayerIdx++;
	}

	const double BfsElapsed = FPlatformTime::Seconds() - BfsStart;
	if (BfsElapsed > 0.020)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FlowField: BFS rebuild took %.1f ms (grid %dx%d = %d cells, layers=%d)"),
			BfsElapsed * 1000.0, GridWidth, GridHeight, N, LayerIdx);
	}
}

FVector UFlowFieldSubsystem::SampleDirection(const FVector& WorldLocation) const
{
	if (!bGridInitialized)
	{
		return FVector::ZeroVector;
	}

	if (CVarFlowFieldEnabled.GetValueOnGameThread() == 0)
	{
		return FVector::ZeroVector; // kill-switch — caller falls back to direct steering
	}

	// Bilinear sampling against the 4 cell centers surrounding WorldLocation. Treats the
	// cell-center grid as a continuous 2D vector field — much smoother than snapping to
	// the discrete 8-direction flow of the containing cell. Blocked cells contribute zero,
	// which naturally biases the blend away from walls near corners.
	const float RelX = (WorldLocation.X - GridOrigin.X) / EffectiveCellSize - 0.5f;
	const float RelY = (WorldLocation.Y - GridOrigin.Y) / EffectiveCellSize - 0.5f;
	const int32 X0 = FMath::FloorToInt(RelX);
	const int32 Y0 = FMath::FloorToInt(RelY);
	const int32 X1 = X0 + 1;
	const int32 Y1 = Y0 + 1;
	const float Fx = RelX - X0;
	const float Fy = RelY - Y0;

	auto SafeFlow = [this](int32 X, int32 Y) -> FVector2D
	{
		if (!IsInBounds(X, Y)) return FVector2D::ZeroVector;
		const int32 Idx = CellIndex(X, Y);
		if (BlockedCells[Idx]) return FVector2D::ZeroVector;
		return FlowDirections[Idx];
	};

	const FVector2D V00 = SafeFlow(X0, Y0);
	const FVector2D V10 = SafeFlow(X1, Y0);
	const FVector2D V01 = SafeFlow(X0, Y1);
	const FVector2D V11 = SafeFlow(X1, Y1);

	const FVector2D Top    = FMath::Lerp(V00, V10, Fx);
	const FVector2D Bottom = FMath::Lerp(V01, V11, Fx);
	const FVector2D Blended = FMath::Lerp(Top, Bottom, Fy);

	if (Blended.IsNearlyZero())
	{
		// Fall back to the home cell's discrete flow when the bilinear blend cancels out
		// (e.g. enemy sitting on a single non-zero cell with three blocked neighbors).
		const FIntPoint Home = WorldToCell(WorldLocation);
		if (IsInBounds(Home.X, Home.Y))
		{
			const int32 HomeIdx = CellIndex(Home.X, Home.Y);
			if (!BlockedCells[HomeIdx])
			{
				const FVector2D& Home2D = FlowDirections[HomeIdx];
				return FVector(Home2D.X, Home2D.Y, 0.0f);
			}
		}
		return FVector::ZeroVector;
	}

	const FVector2D Normalized = Blended.GetSafeNormal();
	return FVector(Normalized.X, Normalized.Y, 0.0f);
}

FIntPoint UFlowFieldSubsystem::WorldToCell(const FVector& WorldPos) const
{
	const float Rel_X = WorldPos.X - GridOrigin.X;
	const float Rel_Y = WorldPos.Y - GridOrigin.Y;
	return FIntPoint(
		FMath::FloorToInt(Rel_X / EffectiveCellSize),
		FMath::FloorToInt(Rel_Y / EffectiveCellSize));
}

FVector UFlowFieldSubsystem::CellToWorld(int32 X, int32 Y) const
{
	return FVector(
		GridOrigin.X + (X + 0.5f) * EffectiveCellSize,
		GridOrigin.Y + (Y + 0.5f) * EffectiveCellSize,
		GridOrigin.Z);
}

FIntPoint UFlowFieldSubsystem::NearestUnblockedCell(FIntPoint Start, int32 MaxRings) const
{
	if (IsInBounds(Start.X, Start.Y) && !BlockedCells[CellIndex(Start.X, Start.Y)])
	{
		return Start;
	}

	// Simple ring search. For each ring R, scan the square perimeter at distance R.
	for (int32 R = 1; R <= MaxRings; R++)
	{
		for (int32 dy = -R; dy <= R; dy++)
		{
			for (int32 dx = -R; dx <= R; dx++)
			{
				// Only scan the perimeter of the current ring.
				if (FMath::Abs(dx) != R && FMath::Abs(dy) != R)
				{
					continue;
				}
				const int32 NX = Start.X + dx;
				const int32 NY = Start.Y + dy;
				if (IsInBounds(NX, NY) && !BlockedCells[CellIndex(NX, NY)])
				{
					return FIntPoint(NX, NY);
				}
			}
		}
	}

	return FIntPoint(INT32_MIN, INT32_MIN); // signal "no open cell found"
}

bool UFlowFieldSubsystem::IsCellBlocked(int32 X, int32 Y) const
{
	if (!IsInBounds(X, Y)) return true;
	return BlockedCells[CellIndex(X, Y)];
}

FVector2D UFlowFieldSubsystem::GetFlowAtCell(int32 X, int32 Y) const
{
	if (!IsInBounds(X, Y)) return FVector2D::ZeroVector;
	return FlowDirections[CellIndex(X, Y)];
}

void UFlowFieldSubsystem::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World || !bGridInitialized)
	{
		return;
	}

	const int32 DebugLevel = CVarFlowFieldDebug.GetValueOnGameThread();
	const int32 Stride = FMath::Max(1, CVarFlowFieldDebugStride.GetValueOnGameThread());
	const float ArrowLen = EffectiveCellSize * 0.4f;
	// Expire just before the next redraw — keeps the screen from accumulating duplicate
	// shapes from frame N and frame N+1 simultaneously.
	const float LifeTime = UpdateInterval * 0.95f;

	// Limit drawing to a window of cells around the player. With a full 200x200 grid this
	// keeps ff.debug 2 from drawing 40K debug boxes per tick (which tanked the framerate).
	const int32 WindowCells = 25;
	FIntPoint Anchor(GridWidth / 2, GridHeight / 2);
	if (CachedPlayer.IsValid())
	{
		Anchor = WorldToCell(CachedPlayer->GetActorLocation());
	}
	const int32 X0 = FMath::Max(0, Anchor.X - WindowCells);
	const int32 X1 = FMath::Min(GridWidth,  Anchor.X + WindowCells);
	const int32 Y0 = FMath::Max(0, Anchor.Y - WindowCells);
	const int32 Y1 = FMath::Min(GridHeight, Anchor.Y + WindowCells);

	for (int32 Y = Y0; Y < Y1; Y += Stride)
	{
		for (int32 X = X0; X < X1; X += Stride)
		{
			const int32 Idx = CellIndex(X, Y);
			FVector Center = CellToWorld(X, Y);
			Center.Z = SampleZ + 5.0f;

			if (BlockedCells[Idx])
			{
				if (DebugLevel >= 2)
				{
					DrawDebugBox(World, Center, FVector(EffectiveCellSize * 0.45f, EffectiveCellSize * 0.45f, 5.0f),
						FQuat::Identity, FColor::Red, false, LifeTime, 0, 1.0f);
				}
				continue;
			}

			const FVector2D D2 = FlowDirections[Idx];
			if (D2.IsNearlyZero())
			{
				continue;
			}
			const FVector D(D2.X, D2.Y, 0.0f);
			DrawDebugDirectionalArrow(World, Center, Center + D * ArrowLen,
				15.0f, FColor::Cyan, false, LifeTime, 0, 1.5f);
		}
	}
}
