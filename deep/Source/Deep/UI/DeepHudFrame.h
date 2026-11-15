// One frame of what the HUD shows, as plain data. ADeepHUD gathers it from the run; SDeepHud paints it.
// Screen positions are viewport pixels; the widget converts them to its own (DPI-scaled) space.

#pragma once

#include "CoreMinimal.h"
#include "UI/DeepHudModel.h"

/** Tone of a prompt, cue or row: picks a colour from the style guide, never a colour of its own. */
enum class EDeepHudTone : uint8 { Normal, Primary, Warning, Danger, Dim };

struct FDeepHudPrompt
{
	FString Key;      // "E", empty for none
	FString Verb;     // caps label: "GRAB", "TAKE THE HELM"
	FString Detail;   // body text after the verb
	FString Detail2;  // a second body line, optional
	EDeepHudTone Tone = EDeepHudTone::Normal;
	/** Anchored to a world object; otherwise in the prompt slot under the reticle. */
	bool bAnchored = false;
	FVector2f ScreenPx = FVector2f::ZeroVector;
	/** 0..1 by distance for anchored tags. */
	float Strength = 1.f;
	/** Stable id so each prompt fades on its own. */
	FName Id;
};

struct FDeepHudRow
{
	FString Label;
	int32 Amount = 0;
	/** Shown as "-123 cr" when true. */
	bool bMinus = false;
	/** Shown as text instead of an amount when set. */
	FString Text;
	EDeepHudTone Tone = EDeepHudTone::Normal;
	bool bRuleAbove = false;
	bool bBig = false;
};

struct FDeepHudCue
{
	FString Label;
	EDeepHudTone Tone = EDeepHudTone::Primary;
};

/** The helm sonar (Sonar lane): one beam's return on a slice. RangeM < 0 = nothing; Strength 0..1 is the phosphor fade. */
struct FDeepHudSonarReturn
{
	float RangeM = -1.f;
	float Strength = 0.f;
};

/** A contact on the sonar. Kind: 0 salvage, 1 relic, 2 serpent. */
struct FDeepHudSonarBlip
{
	float BearingDeg = 0.f;
	float RangeM = 0.f;
	float DzM = 0.f;
	float Strength = 0.f;
	uint8 Kind = 0;
};

/** The sonar scope's picture: per-beam returns on both slices (beam i at 360 * i / N degrees, clockwise from the bow). */
struct FDeepHudSonar
{
	TArray<FDeepHudSonarReturn> Upper;
	TArray<FDeepHudSonarReturn> Lower;
	TArray<FDeepHudSonarBlip> Blips;
	float SweepDeg = 0.f;
	float RangeM = 60.f;
	float RingStepM = 20.f;
	/** Neighbouring returns closer in range than this join into one wall line. */
	float JoinM = 6.f;
	float LowerBelowM = 20.f;
	/** Straight down to the bottom, metres; negative = none in range. */
	float BedM = -1.f;
	/** From the hull's side to the nearest wall at the sub's depth; negative = none. */
	float ClearanceM = -1.f;
	float Proximity01 = 0.f;
	/** The hull's half size in metres: x along the bow, y across. */
	FVector2f HullHalfM = FVector2f::ZeroVector;
};

struct FDeepHudFrame
{
	EDeepHudMode Mode = EDeepHudMode::None;
	uint32 Panels = 0;
	FVector2f ViewportPx = FVector2f(1920.f, 1080.f);

	// The water.
	float DepthM = 0.f;
	FString Zone;
	float CargoKg = 0.f;
	int32 CargoValue = 0;
	bool bOverloaded = false;
	bool bTether = false;
	float TetherLeftM = 0.f;
	float TetherLengthM = 1.f;
	float TetherStrain = 0.f;
	bool bTaut = false;
	bool bHasLamp = false;
	bool bLampOn = false;
	FString LampKey;
	/** 0 ready, 1 flying, 2 latched, 3 spent. */
	uint8 Harpoon = 0;
	int32 HarpoonShots = 0;
	float Patience01 = 1.f;
	TArray<float> PatienceMarks;
	bool bPatienceActive = true;
	float RefillFraction = 0.f;
	float SinceRefill = 100.f;
	float Threat01 = 0.f;
	float BiteLeft = 0.f;
	float AirSeconds = 0.f;
	float AirMax = 1.f;
	float CutHold = 0.f;
	int32 Fee = 0;
	float SinceFee = 100.f;

	// Harpoon aim at the reticle. Kind: 0 none, 1 terrain, 2 sub, 3 serpent, 4 out of range.
	uint8 AimKind = 0;
	FString AimLabel;

	// Where home is.
	bool bHatchOnScreen = false;
	FVector2f HatchPx = FVector2f::ZeroVector;
	/** Off screen: a unit direction in screen space (x right, y down). */
	FVector2f HatchDir = FVector2f(0.f, -1.f);
	FString HatchLabel;

	TArray<FDeepHudPrompt> Prompts;
	FString Warning;
	EDeepHudTone WarningTone = EDeepHudTone::Warning;

	// The helm.
	float SubDepthM = 0.f;
	float LimitM = 0.f;
	float GaugeMaxM = 1.f;
	TArray<float> Stops;
	int32 ParkedStop = 0;
	bool bAtLimit = false;
	/** The sub's hull integrity 0..1 at the helm in the cave shaft (ADeepGameState::GetHullIntegrity01); negative = no bar. */
	float Hull01 = -1.f;
	EDeepHudTone HullTone = EDeepHudTone::Normal;
	FString HelmStatus;
	FString HelmReason;
	/** The seated readout: the levers the player cannot see from the helm camera (ADeepGameState). */
	int32 HelmNotch = 0;
	int32 HelmNotchMin = -2;
	int32 HelmNotchMax = 3;
	/** The ballast lever, -1 blown .. 0 neutral .. +1 flooded. */
	float HelmBallast = 0.f;
	float HeadingDeg = 0.f;
	/** The helm sonar, when EDeepHudPanel::Sonar is on. */
	FDeepHudSonar Sonar;

	int32 Credits = 0;
	FString CaptainLine;
	/** Title: the subtitle shows the Captain's last line here when he is quiet. */
	FString TitleLine;

	// Full-screen cards (dead, settlement, review, pause, settings, credits, quit). The title uses the menu fields too.
	FString CardTitle;
	FString CardLead;
	TArray<FDeepHudRow> Rows;
	FString CardBigLabel;
	FString CardBig;
	FString CardFooterKey;
	FString CardFooter;
	bool bFooterReady = true;
	TArray<FString> MenuItems;
	int32 MenuIndex = 0;
	/** Settings: a value shown at the right of each menu item (empty for none). */
	TArray<FString> MenuValues;
	/** Settings: a 0..1 bar beside the value, negative for none. */
	TArray<float> MenuBars;
	bool bMenuEnabled = true;
	FString MenuHint;

	/** Seconds the controls hint stays up in this mode. */
	float HintSeconds = 15.f;

	/** The controls hint for this mode, shown once per run. */
	TArray<FDeepHudPrompt> Hint;
	/** Hide the hint early (the first grab). */
	bool bHintDone = false;

	/** Feedback cues raised this frame by state changes (grab, winch, cut, harpoon, bank, buy, park). */
	TArray<FDeepHudCue> Cues;
};
