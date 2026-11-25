// D1c: the pilot's lights and the specks they light, as pure rules (World/DeepSubmarine aims and dims the floods and the
// fill; World/DeepEnvironment shows the marine snow). Units: cm, m where named, degrees; Z is up.
#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

namespace DeepHelmLight
{
	/**
	 * A lamp's share of full for the nearest rock: 1 at FullM or farther; nearer, (NearestM / FullM) ^ Power, never under
	 * MinShare. Power 2 keeps the light on that rock constant (inverse square); higher dims a small room's walls more.
	 * MaxShare over 1 (D1c, the throws while sinking) also lifts a lamp for rock farther than FullM, the same way, up to
	 * MaxShare: the far floor of a sink reads as if FullM off.
	 */
	DEEP_API float NearShare(float NearestM, float FullM, float Power, float MinShare, float MaxShare = 1.f);

	/** The hull's heave share from its vertical speed (m/s, + down, as ADeepGameState::HelmHeaveMps): -1 sinking at FullMps, +1 rising. */
	DEEP_API float HeaveFromSpeed(float HeaveMps, float FullMps);

	/** A lamp's pitch for a heave share: LevelDeg level, easing to SinkDeg sinking (-1) or RiseDeg rising (+1). */
	DEEP_API float HeavePitchDeg(float LevelDeg, float Heave, float SinkDeg, float RiseDeg);

	/** One light, as the specks see it: where it is, its axis, the cosine of its cone's half angle (-1: all round), its reach. */
	struct FSnowLight
	{
		FVector AtCm = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;
		float CosOuter = -1.f;
		float ReachCm = 0.f;
	};

	/** How far a light lights a speck (cm): where Intensity over the distance squared (m) falls to LitMin, never past its attenuation. */
	DEEP_API float SnowReachCm(float Intensity, float AttenuationCm, float LitMin);

	/** True when any light's cone and reach hold the point. */
	DEEP_API bool SnowLit(const FVector& Cm, TConstArrayView<FSnowLight> Lights);

	/** A speck's streak (cm) at the hull's real speed: the cockpit's shutter (HelmStreakSeconds); none in the chase view unless bHelmStreakInChase. */
	DEEP_API float SnowStreakCm(float SpeedMps, bool bCockpit, const FDeepLookTuning& Look);

	/** A speck's size (cm) seen from the helm: its own size times HelmSnowSizeScale, never under HelmSnowMinAngle at its distance. */
	DEEP_API float SnowSizeCm(float OwnSizeCm, float DistanceCm, const FDeepLookTuning& Look);
}
