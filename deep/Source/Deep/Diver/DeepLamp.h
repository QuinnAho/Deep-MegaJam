// The diver's lamp up close: what is in its cone, and how far to dim so nothing there burns out.
// Inverse-square light at a locked exposure: a facing surface at 1 m takes 49x the light it takes at 7 m, so the lamp
// dims to keep the brightest lit point in its cone at about its NearFullM brightness (UDeepRules::LampSampleBrightness).
// It samples the whole cone, not the axis: world collision by rays across the cone, the serpent's drawn body (it has
// no collision by design: ADeepSerpent::GetDrawnSpheres), and the dressing's pack rock (no collision either: the
// instances' boxes). Local look only; the actor applies the result (ADeepDiver::Tick, the lamp block).

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class ADeepDiver;
class USpotLightComponent;
struct FDeepLampTuning;

namespace DeepLamp
{
	/** `Deep.Lamp.Near`: 2 = the whole cone (default), 1 = the old single ray on the axis with its old 0.06 floor (before/after frames only), 0 = no dimming. */
	int32 NearMode();

	/** The intensity scale the lamp wants now for what is in its cone (1 = full). Holder = the diver (ignored by the rays). */
	float WantNearScale(const AActor* Holder, const USpotLightComponent* Lamp, const FDeepLampTuning& Tuning);

	/**
	 * The relic's warm glow while the diver carries it (the relic actor is destroyed on the grab, with its lights): a
	 * point light at the harness, made on first need and kept on the diver (component "RelicGlow"). Every machine.
	 */
	void UpdateCarriedGlow(ADeepDiver& Diver);

	/** Distance in cm along Dir from From to the sea bed's surface (its height field, not its collision), or -1 within MaxCm. */
	float RayToBedCm(UWorld* World, const FVector& From, const FVector& Dir, float MaxCm);
}
