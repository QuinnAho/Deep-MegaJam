// The submarine's outside dress: a believable cheap work submersible built from engine basic shapes round the room's
// colliding box. Visual only: no part has collision, every part stays tight to the box (the nose and window visor sit
// over the bow, which already carries the nose), and nothing sits in the hatch column but the thin tether bridle.
// Reference (dev log, 9:20 AM Nov 15): an open tube frame round a pressure hull, buoyancy foam high, heavy things low
// (ballast tanks, battery pods), ducted thrusters at the stern, skids, racked gas cylinders, lamp housings.

#pragma once

#include "CoreMinimal.h"

enum class EDeepHullPaint : uint8
{
	Hull,   // the hull's own worn grey-green
	Frame,  // dark steel tube, heavily worn at the edges
	Foam,   // faded dirty ochre buoyancy blocks
	Tank,   // lighter patched grey: tanks, pods, gas cylinders
	Dark,   // near-black: duct mouths, lens bezels
	Rim,    // the hatch's worn ochre
	Patch,  // doubler plates: hull paint, rustier
	Count
};

struct FDeepHullPart
{
	FName Name;
	const TCHAR* Mesh = nullptr;
	FVector Centre = FVector::ZeroVector;  // sub-local cm
	FVector Size = FVector(100.0);          // cm in the shape's own frame (cylinders: diameter, diameter, length)
	FRotator Rotation = FRotator::ZeroRotator;
	EDeepHullPaint Paint = EDeepHullPaint::Hull;
	bool bShadow = true;
	/** Allowed inside the hatch column (the bridle the tether hangs from). */
	bool bInHatchColumn = false;
};

namespace DeepHull
{
	/**
	 * Every dress part for a hull whose colliding box has these outer half extents (cm) and whose hatch opening has this
	 * half size (cm). Anchor Z is the tether's anchor below the sub's centre (cm, negative). The count and the names do
	 * not depend on the sizes, so a constructor can create the components and LayOut can place them.
	 */
	DEEP_API TArray<FDeepHullPart> Parts(const FVector& OuterCm, double HatchHalfCm, double AnchorZCm);

	/** The part's box in sub-local cm (the shape's 100 cm box scaled and rotated). */
	DEEP_API FBox PartBox(const FDeepHullPart& Part);
}
