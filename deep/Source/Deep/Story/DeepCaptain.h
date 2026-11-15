// The Captain's subtitles. Listens to the event bus, picks a CaptainLines.csv variant, fills tokens and holds one
// line at a time for the HUD. A more urgent event replaces the line on screen; a less urgent one waits in a single
// pending slot and shows next if it is still fresh. Presentation only: it changes no game state.
// With Deep.Captain.Radio 1 (D-040) he is a voice on the sub's radio: DeepCaptainPacing holds back lines in the
// water and picks the tone (calm, tense, scared) from the danger; each shown line keys the radio in
// UDeepAudioSubsystem (squelch, the voice file `<VoiceRoot>/<id>` if there is one, ducking) and a voice file holds
// the subtitle up for its length.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Story/DeepCaptainLines.h"
#include "Story/DeepCaptainPacing.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepCaptain.generated.h"

struct FDeepEventPayload;

UCLASS()
class DEEP_API UDeepCaptain : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepCaptain* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 0 chatter, 1 notable, 2 important, 3 life or death. */
	static int32 Urgency(FName Event);

	/** Handles an event at an explicit time (tests). The bus calls this with the world's real time. */
	void HandleEventAt(FName Event, const FDeepEventPayload& Payload, double Now);

	/** The same with the scene and danger given (tests). With the radio off both are ignored: today's Captain. */
	void HandleEventIn(FName Event, const FDeepEventPayload& Payload, double Now, EDeepCaptainScene Scene, EDeepCaptainDanger Danger);

	/** Deep.Captain.Radio: pacing, tone, squelch, voice and ducking. 0 = the Captain as before D-040. */
	static bool IsRadioOn();

	/** Where the local diver is and how much danger it is in, read from the world now. */
	EDeepCaptainScene GatherScene() const;
	EDeepCaptainDanger GatherDanger() const;

	/** Lines the pacing held back since the world started (DEEPSTATE, tests). */
	int32 GetHeldBackCount() const { return HeldBack; }

	/** The full subtitle to show now ("CAPTAIN: ..."), or empty. Promotes the pending line when the current ends. */
	FString GetLineAt(double Now);
	FString GetLine();

	/** 0..1 through the current line's display time (for a fade); 1 when nothing shows. */
	float GetLineProgressAt(double Now) const;

	/** The last line shown or queued, without prefix (DEEPSTATE). */
	const FString& GetLastText() const { return LastText; }
	FName GetCurrentEvent() const { return Current.Event; }

	FDeepCaptainLines& GetLines() { return Lines; }

private:
	struct FShown
	{
		FString Text;
		FName Event;
		int32 Urgency = -1;
		double Start = 0.0;
		double End = -1.0;
		/** CaptainLines.csv id (the voice file's name) and whether the radio keys for it (Deep.Captain.Radio). */
		FString LineId;
		bool bRadio = false;
		bool IsValid() const { return !Text.IsEmpty(); }
	};

	void OnBusEvent(FName Event, const FDeepEventPayload& Payload);
	void Show(const FShown& Line, double Now);
	double Now() const;

	FDeepCaptainLines Lines;
	FRandomStream Rng;
	FShown Current;
	FShown Pending;
	FString LastText;
	/** When the last line went on screen (radio pacing). */
	double LastShownAt = -1.0e9;
	int32 HeldBack = 0;
	FDelegateHandle BusHandle;
};
