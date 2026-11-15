// A rope as a chain of points (Verlet integration, distance constraints), for the tether and the harpoon line.
// Visual only: no rule reads it. Pure: no world, no components, so tests can drive it directly.

#pragma once

#include "CoreMinimal.h"

struct DEEP_API FDeepRopeSim
{
	/** Fixed step, so the rope behaves the same at any frame rate. */
	static constexpr float StepSeconds = 1.f / 60.f;
	/** At most this many steps per frame; a long hitch drops time instead of spiralling. */
	static constexpr int32 MaxStepsPerFrame = 4;

	TArray<FVector> Pos;
	TArray<FVector> Prev;
	/** Link i joins point i and point i + 1. A cut link is 0 and is never drawn or solved again until Init. */
	TArray<uint8> Linked;
	/** Rest length of every link (cm). */
	float RestCm = 100.f;
	float Accumulator = 0.f;

	bool IsValid() const { return Pos.Num() >= 2; }
	int32 NumLinks() const { return FMath::Max(0, Pos.Num() - 1); }
	bool IsLinked(int32 Link) const { return Linked.IsValidIndex(Link) && Linked[Link] != 0; }
	/** Number of links still joined. */
	int32 CountLinked() const;

	/** A straight line of Points points from A to B, all joined, at rest. */
	void Init(int32 Points, const FVector& A, const FVector& B);

	/**
	 * Advances by DeltaSeconds in fixed steps. A non-null pin holds the first or last point there. Each free point sinks
	 * at SinkCmS2 and loses speed at DragPerS (water). LengthCm is the whole rope's rest length.
	 */
	void Advance(float DeltaSeconds, const FVector* StartPin, const FVector* EndPin, float LengthCm, float SinkCmS2, float DragPerS, int32 Iterations);

	/** The link whose segment passes closest to P (INDEX_NONE if none is joined). */
	int32 NearestLink(const FVector& P) const;

	/**
	 * Parts the rope at Link. The two new ends recoil away from the cut along the line (WhipCmS at the cut, fading over
	 * WhipPoints points) with a sideways kick, so the loose length whips instead of stopping dead.
	 */
	void Cut(int32 Link, float WhipCmS, int32 WhipPoints);

	/** Length along the rope from the first point to point Index (cm), counting cut links too. */
	float LengthTo(int32 Index) const;
};
