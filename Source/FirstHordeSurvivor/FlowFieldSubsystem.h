#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FlowFieldSubsystem.generated.h"

class ACharacter;

/**
 * UFlowFieldSubsystem — BFS-flood-fill flow field for horde enemy navigation.
 *
 * Overlays a 2D grid on the playable floor and runs Dijkstra (8-directional, integer
 * cardinal/diagonal costs 10/14) from the player's cell outward at ~10Hz. Each open cell
 * stores a unit XY vector pointing toward the cheapest neighbor — enemies sample that as
 * their movement input.
 *
 * Cost is independent of enemy count (one rebuild covers every enemy on the map). Sized
 * automatically from UEnemySpawnSubsystem::GetFloorBounds(), with the obstacle pass run
 * once on init via capsule overlap against ECC_WorldStatic / ECC_WorldDynamic.
 *
 * Public API:
 *   SampleDirection(WorldLocation) — returns ZeroVector if off-grid / on a blocked cell /
 *   before init; callers are expected to fall back to direct steering in that case.
 */
UCLASS()
class FIRSTHORDESURVIVOR_API UFlowFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Sample the flow direction at WorldLocation. Returns ZeroVector when no useful
	 *  direction is available (off-grid, blocked cell, or pre-init). */
	FVector SampleDirection(const FVector& WorldLocation) const;

	/** Manually rerun the obstacle marking pass — call after spawning new walls / level geo
	 *  changes at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Flow Field")
	void RefreshBlockedCells();

	/** Force an immediate rebuild of the flow field (skips the cell-move dedupe). */
	UFUNCTION(BlueprintCallable, Category = "Flow Field")
	void ForceRebuild();

	// ---- Tuning ----

	/** World units per grid cell. Smaller = more accurate paths, more cells to compute. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Grid")
	float CellSize = 80.0f;

	/** Seconds between BFS rebuilds. 0.1 = 10Hz. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Update")
	float UpdateInterval = 0.1f;

	/** Half-extent of the active flow field region around the player (UU). Only this square
	 *  around the player is scanned and pathed; enemies outside fall back to direct steering.
	 *  Should be ≈ visible-area half-extent + ~30% margin. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Grid")
	float ActiveHalfExtent = 2500.0f;

	/** Distance the player must move from the current grid center before the grid recenters
	 *  + rescans. Larger = fewer rescans but the active region lags behind the player. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Grid")
	float RecenterDeadzone = 1000.0f;

	/** Safety cap on cells per axis. Active region / CellSize shouldn't normally hit this,
	 *  but it bounds memory if ActiveHalfExtent is set absurdly high. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Grid")
	int32 MaxCellsPerSide = 96;

	/** Wall-clock budget per scan tick (seconds). Init scan spreads across ticks; each
	 *  tick processes cells until this budget is spent, then yields. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Update")
	float ScanBudgetSeconds = 0.012f;

	/** Scan timer interval while obstacle scan is running. Faster = scan completes sooner
	 *  but more frequent hitches. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Update")
	float ScanTickInterval = 0.020f;

	/** Sphere radius used to probe each cell for walls. Sized roughly to enemy radius —
	 *  smaller = more accurate near walls, larger = more conservative (over-blocks). */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Obstacles")
	float ObstacleProbeRadius = 35.0f;

	/** Reserved — currently unused since the obstacle pass switched from a capsule probe
	 *  to a down-trace + sphere probe. Kept for backwards-compat. */
	UPROPERTY(EditDefaultsOnly, Category = "Flow Field|Obstacles")
	float ObstacleProbeHalfHeight = 88.0f;

	// Debug helpers
	int32 GetGridWidth() const { return GridWidth; }
	int32 GetGridHeight() const { return GridHeight; }
	float GetCellSize() const { return CellSize; }
	FVector GetGridOrigin() const { return GridOrigin; }
	float GetSampleZ() const { return SampleZ; }
	bool IsCellBlocked(int32 X, int32 Y) const;
	FVector2D GetFlowAtCell(int32 X, int32 Y) const;
	bool IsGridInitialized() const { return bGridInitialized; }
	FIntPoint WorldToCell(const FVector& WorldPos) const;
	FVector CellToWorld(int32 X, int32 Y) const;

private:
	void TryInitializeGrid();
	void StartObstacleScan();
	void ProcessScanChunk();
	void RebuildFlowField();
	void TickRebuild();
	void DrawDebug() const;

	// Returns true if a recenter was triggered (caller should skip BFS this tick — the new
	// scan needs to complete first).
	bool CheckRecenter();

	int32 CellIndex(int32 X, int32 Y) const { return Y * GridWidth + X; }
	bool IsInBounds(int32 X, int32 Y) const { return X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight; }
	FIntPoint NearestUnblockedCell(FIntPoint Start, int32 MaxRings = 6) const;

	bool bGridInitialized = false;       // grid dims set + obstacle scan complete
	bool bGridDimsReady = false;         // grid dims set but obstacle scan still pending / running
	bool bScanInProgress = false;        // obstacle scan currently chunking
	int32 ScanCursor = 0;                // next cell index to probe
	int32 ScanBlockedAccum = 0;          // running count during scan

	// Grid descriptor
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	float EffectiveCellSize = 80.0f;       // CellSize after auto-upscaling to fit MaxCellsPerSide
	FVector GridOrigin = FVector::ZeroVector;
	float SampleZ = 0.0f;
	// Center of the active region in world XY. Set when grid is built; player must move >
	// RecenterDeadzone away from this point to trigger a re-init + rescan.
	FVector2D GridCenter = FVector2D::ZeroVector;
	bool bGridCenterValid = false;

	// Per-cell data (row-major, length = GridWidth * GridHeight)
	TArray<FVector2D> FlowDirections;
	TBitArray<> BlockedCells;
	TArray<int32> CellCosts;

	// Cached refs
	TWeakObjectPtr<ACharacter> CachedPlayer;

	// Last cell rebuild was anchored to — skip rebuilds when player hasn't crossed a cell
	FIntPoint LastPlayerCell = FIntPoint(INT32_MIN, INT32_MIN);

	// Scratch buffers for layered (uniform-cost) BFS — kept as members to avoid
	// allocations every tick.
	TArray<int32> CurrentLayer;
	TArray<int32> NextLayer;

	// Diagnostic state for the chunked obstacle scan — accumulated across all chunks of a
	// single scan pass, emitted as a summary at scan completion. Reset at scan start.
	// Key is the actor's FName (stable across the run); value is # cells that actor blocked.
	TMap<FName, int32> ScanActorBlockCounts;
	// Names of actors we've already logged detail for during this scan — avoids spamming the
	// log when a single wall blocks dozens of cells.
	TSet<FName> ScanLoggedActorNames;

	FTimerHandle RebuildTimerHandle;
	FTimerHandle ScanTimerHandle;
};
