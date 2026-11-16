// The Captain on the radio (D-040): when a line may show and in what tone. Pure, tested without a world
// (Deep.Story.CaptainPacing). UDeepCaptain gathers the inputs from the local diver, the game state and the brain.
// Smug aboard, curt in the water, rattled when the serpent closes, then silent.

#pragma once

#include "CoreMinimal.h"

struct FDeepCaptainTuning;

/** Where the local diver is, for the Captain. */
enum class EDeepCaptainScene : uint8
{
	Title,
	Aboard,
	/** Outside the sub: tethered, cut or dead. */
	Water,
	/** Settlement or the review. */
	Settlement,
};

/** The Captain's mood follows the danger. Also the `tone` column of CaptainLines.csv (calm, tense, scared). */
enum class EDeepCaptainDanger : uint8
{
	Calm,
	Tense,
	Scared,
};

struct FDeepCaptainDangerInputs
{
	bool bInWater = false;
	/** The serpent stalks, hunts or bites this diver (on a client, where its state is unknown: it is present). */
	bool bSerpentClosing = false;
	/** Head to diver; negative when there is no serpent. */
	float SerpentDistanceM = -1.f;
	bool bBiteAlarm = false;
	bool bTethered = false;
	float PatienceFraction = 1.f;
	/** Cut: bailout air runs. */
	bool bBailout = false;
	/** The sub's hull, 0..1 (ADeepGameState::GetHullIntegrity01), read only while seated at the helm: in the cave shaft a
	 * battered hull is the danger. 1 = whole (or not at the helm). */
	float Hull01 = 1.f;
};

namespace DeepCaptainPacing
{
	/** Scared: the serpent closes within ScaredSerpentM, or a bite alarm runs. Tense: Patience under the fraction, or bailout air. Calm otherwise and out of the water. */
	DEEP_API EDeepCaptainDanger Danger(const FDeepCaptainDangerInputs& In, const FDeepCaptainTuning& Tuning);

	/** calm, tense or scared: the `tone` cell that matches. */
	DEEP_API FName ToneName(EDeepCaptainDanger Danger);

	/**
	 * Whether a line of this urgency may show. Title, aboard and settlement: always. In the water: urgency 0 never,
	 * urgency 1 only WaterNotableGapSeconds after the last shown line, 2 and 3 always; scared: only ScaredMinUrgency and up.
	 */
	DEEP_API bool Allows(EDeepCaptainScene Scene, EDeepCaptainDanger Danger, int32 Urgency, double SinceLastShownS, const FDeepCaptainTuning& Tuning);

	DEEP_API const TCHAR* SceneName(EDeepCaptainScene Scene);
}
