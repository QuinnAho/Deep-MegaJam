// D1b: BT-01's own work lamps, as pure rules (World/DeepSubmarine traces the hull with the probes and mounts the lamps).
// A tower lamp on the tower's aft face near its top lights the after deck; two stern lamps on the after deck
// light the tower's back. Every lamp stands a little off the hull's real surface and aims at the
// hull. Units: cm in the visual hull's frame (+X the bow, +Y starboard, +Z up); the solid box is the hull's collision.
#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

namespace DeepHullLights
{
	/** A straight probe for a surface: from a point outside the hull toward it. */
	struct FProbe
	{
		FVector FromCm = FVector::ZeroVector;
		FVector ToCm = FVector::ZeroVector;
	};

	/** Probes straight down the centre line from over the box, Count of them spread stern to bow: the highest hit is the tower's top. */
	DEEP_API TArray<FProbe> TowerTopProbes(const FBox& Solid, int32 Count);

	/** The probe for the tower's aft face: forward along the centre line from astern of the box, TowerLampBelowTopCm under the top. */
	DEEP_API FProbe TowerBackProbe(const FBox& Solid, const FVector& TowerTopCm, const FDeepHullLightTuning& T);

	/** The probe for the after deck the tower lamp lights: straight down at TowerLampAimAlongFrac of the length from the stern. */
	DEEP_API FProbe DeckProbe(const FBox& Solid, const FDeepHullLightTuning& T);

	/** The probe for one stern lamp (Side -1 port, +1 starboard): straight down onto the after deck at the along and side shares. */
	DEEP_API FProbe SternProbe(const FBox& Solid, int32 Side, const FDeepHullLightTuning& T);

	/** A lamp on the hull: MountOutCm out from the surface along its normal, aimed at AimCm (the light's +X). */
	DEEP_API FTransform Mount(const FVector& SurfaceCm, const FVector& NormalCm, const FVector& AimCm, const FDeepHullLightTuning& T);

	/** True when Cm lies inside the spot light's cone (half angle OuterConeDeg) and within RangeCm of it. */
	DEEP_API bool InCone(const FTransform& Lamp, const FVector& Cm, float OuterConeDeg, float RangeCm);
}
