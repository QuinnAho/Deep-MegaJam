// Deep.World.Caves, M2b: pure rules for the dive in the cave string (no world, no actors). Salvage piles round a socket
// (D-067), the relic's stop and the stops the HUD may name (D-068), and the serpent's room in a cavern: a voxel grid of
// the rock, flooded from the cavern's Serpent socket, that maps any point to the nearest one whose ball of ClearCm is
// clear of rock. World/DeepCaveString builds the rock flags from the chunks; Monster/ keeps the serpent's path in it.
// Tests: Private/Tests/DeepCaveDiveTests.cpp (Deep.Sim.CaveDive.*).

#pragma once

#include "CoreMinimal.h"

/** A cavern's free space for the serpent, axis-aligned in its chunk's entry frame (cm). */
struct DEEP_API FDeepCaveRoom
{
	/** The min corner of cell (0, 0, 0), in the chunk's frame (cm). */
	FVector OriginCm = FVector::ZeroVector;
	float CellCm = 50.f;
	FIntVector Dims = FIntVector::ZeroValue;
	/** The clearance the free cells keep (cm): every point of a free cell has a ball this big clear of rock. */
	float ClearCm = 0.f;
	/** Per cell: reached from the seed without crossing rock. */
	TArray<uint8> Reached;
	/** Per cell: every point in it is ClearCm clear of rock. */
	TArray<uint8> Free;
	/** Per cell: centre to the nearest solid cell's centre (or the grid's outside), cm. */
	TArray<float> DistCm;
	/** Per cell: the nearest free cell (itself when free), INDEX_NONE when there is none. */
	TArray<int32> Nearest;
	int32 FreeCount = 0;
	/** How many separate pools of free cells there were before all but the largest were dropped. */
	int32 PoolCount = 0;

	bool IsValid() const { return Dims.X > 0 && Dims.Y > 0 && Dims.Z > 0 && Free.Num() == Dims.X * Dims.Y * Dims.Z; }
	int32 Num() const { return Dims.X * Dims.Y * Dims.Z; }
	int32 Index(const FIntVector& Cell) const { return Cell.X + Dims.X * (Cell.Y + Dims.Y * Cell.Z); }
	FIntVector CellOf(int32 Index) const { return FIntVector(Index % Dims.X, (Index / Dims.X) % Dims.Y, Index / (Dims.X * Dims.Y)); }
	/** The cell holding a point, clamped into the grid. */
	FIntVector CellAt(const FVector& LocalCm) const;
	FVector CentreCm(const FIntVector& Cell) const { return OriginCm + (FVector(Cell) + FVector(0.5)) * CellCm; }
	FBox CellBox(const FIntVector& Cell) const { return FBox(OriginCm + FVector(Cell) * CellCm, OriginCm + FVector(Cell + FIntVector(1)) * CellCm); }
};

/** A route through a room's free cells: per cell, the face steps to Target through free cells; -1 where there is no way. */
struct DEEP_API FDeepCaveRoute
{
	int32 Target = INDEX_NONE;
	TArray<int32> Steps;
};

namespace DeepCaveDiveRules
{
	/**
	 * Where a pile's piece Index of Count may lie, relative to its socket (cm, the chunk's frame), best first: Spread out
	 * at Index's share of the circle (from YawDeg), then nearer or farther by Step and turned up to just under half a share
	 * either way, never past Reach. A pile of one starts at the socket.
	 */
	DEEP_API TArray<FVector2D> PileSpotsCm(int32 Index, int32 Count, float SpreadCm, float StepCm, float ReachCm, float YawDeg);

	/** The stop the relic waits at (D-068): the cave string's last berth (CaveBerths stops) in the caves, else the shaft's. */
	DEEP_API int32 RelicStop(bool bCaves, int32 ShaftRelicStop, int32 CaveBerths);

	/** How many stops the HUD may name (D-068): the cave string's berths in the caves (the next is the next berth), else all the shaft's. */
	DEEP_API int32 ShownStops(bool bCaves, int32 ShaftStops, int32 CaveBerths);

	/**
	 * Builds the room from rock flags (per cell, Room's Dims and OriginCm and CellCm set): the cells reached from Seed
	 * (the nearest open cell to it) without crossing rock are water; every other cell, and the grid's outside, is solid.
	 * A cell is free when its centre stands ClearCm plus a cell's diagonal off every solid cell's centre: then each point
	 * in it has a ClearCm ball clear of the rock (rock in a solid cell lies within half a diagonal of its centre). Only
	 * the largest pool of free cells joined by faces stays free: the serpent never jumps between pools, nor sits in a pocket.
	 */
	DEEP_API void BuildRoom(FDeepCaveRoom& Room, const TArray<uint8>& Rock, const FIntVector& Seed, float ClearCm);

	/** A point kept in the room (chunk frame, cm): unchanged in a free cell, else the nearest point of the nearest free cell. */
	DEEP_API FVector KeepInRoom(const FDeepCaveRoom& Room, const FVector& LocalCm);

	/** The share of the circle round Centre (radius, horizontal, chunk frame) that lies in free cells, sampled at Samples points. */
	DEEP_API float FreeShareOfCircle(const FDeepCaveRoom& Room, const FVector& CentreCm, float RadiusCm, int32 Samples);

	/**
	 * The lurk circle fitted to the room: the largest radius from MinCm up to MaxCm (in StepCm) whose circle round Centre
	 * is at least MinShare free; if none is, the radius with the most free share (the smaller on a tie).
	 */
	DEEP_API float FitLurkRadiusCm(const FDeepCaveRoom& Room, const FVector& CentreCm, float MinCm, float MaxCm, float StepCm, float MinShare, int32 Samples);

	/**
	 * The lurk circle near a socket: over the free cells' points within ReachCm of it (every StrideCm each way), the centre
	 * whose widest circle (MinCm up to MaxCm, in StepCm) is at least MinShare free; the widest wins, the nearer the
	 * socket on a tie. True if one is; else false, with the socket and FitLurkRadiusCm round it.
	 */
	DEEP_API bool FitLurkCircle(const FDeepCaveRoom& Room, const FVector& SocketCm, float ReachCm, float StrideCm, float MinCm, float MaxCm, float StepCm,
		float MinShare, int32 Samples, FVector& OutCentreCm, float& OutRadiusCm);

	/**
	 * Where on a line (Anchor to DiverEnd, chunk frame) the serpent can bite: from FromDiverCm up the line from the diver
	 * toward the anchor, in StepCm, the first point whose kept point (KeepInRoom) is within ToleranceCm of it; the point
	 * with the smallest gap if none is.
	 */
	/** Every point of the segment (chunk frame, cm) lies in a free cell (sampled every half cell). */
	DEEP_API bool LineFree(const FDeepCaveRoom& Room, const FVector& FromCm, const FVector& ToCm);

	/** Fills Route with the face steps to the cell TargetIndex through free cells (a breadth-first flood). */
	DEEP_API void BuildRoute(const FDeepCaveRoom& Room, int32 TargetIndex, FDeepCaveRoute& Route);

	/**
	 * Where to steer from From toward To (chunk frame, cm) in the room: To when the straight way between their kept
	 * points is free; else down the route to To's nearest free cell (Route rebuilt when its target differs), the
	 * farthest cell within LookCm of steps that the head sees in a free straight line. To when there is no route.
	 */
	DEEP_API FVector RouteAim(const FDeepCaveRoom& Room, FDeepCaveRoute& Route, const FVector& FromCm, const FVector& ToCm, float LookCm);

	DEEP_API FVector ReachableOnLine(const FDeepCaveRoom& Room, const FVector& AnchorCm, const FVector& DiverEndCm, float FromDiverCm, float ToleranceCm, float StepCm);
}
