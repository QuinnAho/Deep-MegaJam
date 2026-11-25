// Immersion I1 (D-072), one per world on every machine that renders (never a dedicated server), local only: nothing
// here is replicated or changes the game. It reads the replicated sub state and the local diver and:
//  - the hull's voice: creaks (hull_creak, a family of three) by depth, turns and depth changes, and a groan
//    (hull_groan) the first time each 100 m band is passed, heard aboard (the room and the helm). The flow loop is
//    gathered by UDeepAudioSubsystem (loop_flow) from the same rules.
//  - the sonar's echo: on each local sonar_ping, sonar_echo plays back from the walls the ping's beams traced (the helm
//    sweep's own samples; a diver's ping traces one ring of beams once).
//  - the silt actor: spawns ADeepSilt once an arena exists.
// Rules: Sim/DeepImmersionRules. Tuning: DEEP Tuning > Immersion. Cheats: Immersion/DeepImmersionCheats.cpp.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Sim/DeepImmersionRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepImmersion.generated.h"

class FJsonObject;
struct FDeepEventPayload;

UCLASS()
class DEEP_API UDeepImmersion : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepImmersion* Get(const UObject* WorldContext);
	/** Deep.Immersion 1 (default): the hull's voice, the flow, the echo and the silt. 0: none of it. */
	static bool IsOn();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** One step of the hull's voice (tests call it directly). */
	void StepVoice(float DeltaSeconds);

	/** Plays a creak or the groan now as the rules would at this depth (cheats). */
	void PlayCreakNow(bool bStress);
	void PlayGroanNow();

	/** The echo of a ping from the ranges its beams traced (m; < 0 = no return). Returns the returns scheduled. */
	TArray<FDeepEchoTap> ScheduleEcho(const TArray<float>& RangesM);

	/** True while the local diver is aboard and the run is on (the voice is heard). */
	bool IsHearingHull() const;

	int32 GetCreaks() const { return Creaks; }
	int32 GetGroans() const { return Groans; }
	int32 GetEchoes() const { return Echoes; }
	int32 GetDeepestBand() const { return DeepestBand; }
	const TArray<float>& GetLastEchoRanges() const { return LastEchoRanges; }
	const TArray<FDeepEchoTap>& GetLastEcho() const { return LastEcho; }

	/** Tests: forget the clocks (a new run). */
	void ResetVoice();

	/** `{on, hearing, creaks, groans, band, echoes, echo:[{m,delay_s,gain,lp_hz}], silt:{...}}` for DEEPSTATE. */
	TSharedRef<FJsonObject> BuildStateJson() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleEvent(FName Event, const FDeepEventPayload& Payload);
	/** The ranges the local helm sonar's sweep holds now (both slices); empty when it is not working. */
	TArray<float> HelmSonarRanges() const;
	/** One ring of level beams from a point (a diver's ping), once. */
	TArray<float> RingRanges(const FVector& OriginCm, const AActor* Ignore) const;

	FDelegateHandle EventHandle;
	FDeepCreakClock CreakClock;
	FRandomStream Rand{ 0x1DEE };
	int32 DeepestBand = -1;
	float LastDepthM = -1.f;
	float LastYawDeg = 0.f;
	float LastRunSeconds = -1.f;
	bool bWasHearing = false;
	int32 Creaks = 0;
	int32 Groans = 0;
	int32 Echoes = 0;
	TArray<float> LastEchoRanges;
	TArray<FDeepEchoTap> LastEcho;
};
