// The helm's sonar (Sonar lane, Nov 15). Local and client-side: it lives on the local player's HUD actor, works
// only while that player's diver is seated at the helm, and never touches replicated state.
// Each frame the sweep turns (one turn per SweepPeriodS, a ping each turn); every beam the sweep passed is traced
// on ECC_Visibility from the sub's real centre (the sub never moves; the world slides past it), once at the sub's
// depth and once on the dim slice below, at most MaxTracesPerFrame traces a frame (a backlog carries over).
// Results are cached per beam with the time they were swept, so the scope fades them like phosphor.
// Salvage and the serpent are read from the actors themselves (the serpent has no collision by design) when the
// sweep passes their bearing. The nearest wall at depth drives a proximity tone. Rules: Sonar/DeepSonarRules.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepSonarComponent.generated.h"

class ADeepSubmarine;
struct FDeepHudSonar;

/** One beam on one slice: the range of its return (negative = nothing in range) and when the sweep traced it. */
struct FDeepSonarSample
{
	float RangeM = -1.f;
	double At = -1.0e9;
};

enum class EDeepSonarContact : uint8 { Salvage, Relic, Serpent };

/** A contact as the sweep last saw it. */
struct FDeepSonarContactSample
{
	TWeakObjectPtr<AActor> Actor;
	EDeepSonarContact Kind = EDeepSonarContact::Salvage;
	float BearingDeg = 0.f;
	float RangeM = 0.f;
	/** Height above the sub's centre, metres (negative = below). */
	float DzM = 0.f;
	double At = -1.0e9;
};

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepSonarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepSonarComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Deep.Sonar 0|1. Off = today's helm: no traces, no panel, no sound. */
	void SetSonarEnabled(bool bInEnabled);
	bool IsSonarEnabled() const { return bSonarEnabled; }

	/** True while the owner's local diver is seated at the helm (or a test forces it). */
	bool IsManned() const;
	/** Enabled and manned: the panel shows. */
	bool IsWorking() const { return bSonarEnabled && IsManned(); }

	/** Tests: run as if manned, from a given point and bow, with a given hull half size (metres, x bow, y beam). */
	void SetTestFrame(const FVector& OriginCm, const FVector& Forward, const FVector2D& HullHalfM = FVector2D::ZeroVector);
	void SetPlaySounds(bool bInPlay) { bPlaySounds = bInPlay; }

	/** Where the sonar sits and which way is up on the scope. False with no sub (and no test frame). */
	bool GetFrame(FVector& OriginCm, FVector& Forward, FVector2D& HullHalfM) const;

	/** One frame of sonar: turn the sweep, trace the beams it passed (within the budget), contacts, ping, tone. */
	void StepSonar(float DeltaSeconds);
	/** Traces every beam of both slices and reads every contact now (tests, and a fresh helm). */
	void SweepNow();

	/** Traces one horizontal beam; range in metres, negative for nothing within RangeM. */
	float TraceBeam(const FVector& FromCm, const FVector& Dir) const;

	const TArray<FDeepSonarSample>& GetSlice(int32 Slice) const { return Slice == 0 ? Upper : Lower; }
	const TArray<FDeepSonarContactSample>& GetContacts() const { return Contacts; }
	double GetClock() const { return Clock; }
	float GetSweepDeg() const { return SweepDegNow; }
	/** Distance from the sub's centre straight down to the bottom, metres; negative = none within range. */
	float GetBedBelowM() const { return BedBelowM; }
	float GetLowerBelowM() const { return LowerBelowM; }
	/** Nearest live return at the sub's depth from the centre, and from the hull's side, metres; negative = none. */
	float GetNearestM() const { return NearestM; }
	float GetClearanceM() const { return ClearanceM; }
	float GetProximity01() const { return Proximity01; }
	FVector2D GetHullHalfM() const { return HullHalfM; }
	int32 GetPings() const { return Pings; }
	/** D2: the pilot pinged (on demand, the server's word): counts it and refreshes the whole picture. */
	void NoteHelmPing();
	/** How many times the sonar raised sonar_contact (the serpent's first contact at a stop). */
	int32 GetContactCalls() const { return ContactCalls; }
	int32 GetBeeps() const { return Beeps; }
	int32 GetTracesLastStep() const { return TracesLastStep; }

	/** Logs `DEEPSONAR bench {...}` after Seconds: mean frame time, and the sonar's own cost per frame. */
	void StartBench(float Seconds);

	/** `{on, manned, sweep_deg, nearest_m, clearance_m, bed_m, lower_m, returns, contacts, pings}` for DEEPSTATE and the cheats. */
	FString StateJson() const;

	/** The scope's picture for the HUD frame: each return and contact with its phosphor strength now. */
	void FillHud(FDeepHudSonar& Out) const;

private:
	void EnsureBeams();
	void BuildQuery(struct FCollisionQueryParams& Params) const;
	void TraceBeamIndex(int32 Beam, const FVector& OriginCm, const FVector& Forward, const struct FCollisionQueryParams& Params);
	void UpdateContacts(const FVector& OriginCm, const FVector& Forward, float PrevDeg, float NewDeg, bool bAll);
	void UpdateNearest();
	void PlaySonarSound(const TCHAR* Event, float VolumeScale, float PitchScale);

	bool bSonarEnabled = true;
	bool bPlaySounds = true;
	bool bTestFrame = false;
	FVector TestOrigin = FVector::ZeroVector;
	FVector TestForward = FVector::ForwardVector;
	FVector2D TestHullHalfM = FVector2D::ZeroVector;

	mutable TWeakObjectPtr<ADeepSubmarine> Sub;

	TArray<FDeepSonarSample> Upper;
	TArray<FDeepSonarSample> Lower;
	TArray<FDeepSonarContactSample> Contacts;

	double Clock = 0.0;
	float SweepDegNow = 0.f;
	/** Next beam to trace, and how many beams the sweep has passed that are not traced yet. */
	int32 Cursor = 0;
	int32 Backlog = 0;
	float BedBelowM = -1.f;
	float LowerBelowM = 20.f;
	float NearestM = -1.f;
	float ClearanceM = -1.f;
	float Proximity01 = 0.f;
	FVector2D HullHalfM = FVector2D::ZeroVector;
	float SinceBeep = 1000.f;
	int32 Pings = 0;
	int32 Beeps = 0;
	int32 ContactCalls = 0;
	/** The stop of the last sonar_contact call; -1 = none yet. Kept when the helm is left: once per stop. */
	int32 SerpentCalledStop = -1;
	int32 TracesLastStep = 0;
	bool bWasWorking = false;

	float BenchLeft = 0.f;
	int32 BenchFrames = 0;
	double BenchFrameS = 0.0;
	double BenchSonarS = 0.0;
};
