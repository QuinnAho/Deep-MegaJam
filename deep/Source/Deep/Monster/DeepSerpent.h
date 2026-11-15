// The sea serpent's body. A head steers (speed + max turn rate); the body follows the path the head actually
// travelled (follow-the-leader), with a travelling sideways weave that banks into each curve.
// Look (Style Guide, section 6): one continuous tapering tube with a dorsal ridge, rebuilt each frame as a
// procedural mesh along the trail (parallel-transport ring frames, so it never twists); a code-built wedge head
// with a hinged jaw on a head socket, which a static mesh head can replace from the settings; near-black wet
// skin; markings (rim + two rows of spots pulsing faster with threat) as an unlit additive layer drawn just over
// whatever the body wears; eyes that shine back only while a diver's lamp is on them. Motion cues: a flinch on
// recoil, a lunge and thrash on the bite. bTubeBody = false falls back to the old instanced spheres.
// A Pawn possessed by ADeepSerpentController, which decides where it goes (server), calls SwimToward and SetMood.
// The body lays itself out every tick on every machine; nothing here changes how it hunts.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Monster/DeepMonsterTypes.h"
#include "DeepSerpent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;

UCLASS()
class DEEP_API ADeepSerpent : public APawn
{
	GENERATED_BODY()

public:
	ADeepSerpent();

	/** Server: turn toward Aim by at most TurnRate * dt, then swim forward. Never overshoots Aim. */
	void SwimToward(const FVector& AimCm, float SpeedMps, float TurnRateDegPerS, float DeltaSeconds);

	/** Places the head and lays the body out straight behind it. */
	void PlaceHead(const FVector& HeadCm, const FVector& Facing);

	/**
	 * Server: moves the path into Keep's room (the cavern): the head and the newest trail point, or the whole trail when a
	 * body was just placed (it lies straight and may cross rock). Keep maps a world point (cm) to where it may be.
	 */
	void KeepPath(TFunctionRef<FVector(const FVector&)> Keep, bool bWholeTrail);

	/** Server: what the mind is doing, for the look (replicated). Does not affect behaviour. */
	void SetMood(EDeepMonsterState InState, float InThreat01);

	/** Shows the markings now, skipping the first-minute rule (cheats and shots). */
	void RevealNow() { bForceReveal = true; Reveal01 = 1.f; }

	/** Shots only: shows this state's motion and markings (threat 1) whatever the brain is doing; -1 = off. The brain is untouched. */
	void SetPoseOverride(int32 InState) { PoseOverride = InState; }

	/** The visible head: the head socket (the skull of a mesh head), which leads the actor in a lunge and trails it in a flinch. */
	FVector GetHeadLocation() const;

	/** World point between the eyes (tests: the gameplay head must sit at the visible head). */
	FVector GetEyeMidpoint() const;

	/** Rebuilds the head, eyes and materials from the settings and `Deep.Monster.MeshHead` (the switch calls it). */
	void RefreshLook();

	/** Look-dev: the body skin's or the mesh head's dynamic material (Deep.Monster.Param). */
	UMaterialInstanceDynamic* GetLookMID(bool bHead) const { return bHead ? HeadMeshMID.Get() : SkinMID.Get(); }

	/** `Deep.Monster.MeshHead` (default 1): 0 = always the code-built head. */
	static bool IsMeshHeadAllowed();
	FVector GetHeading() const { return Heading; }

	/** World positions of the body segments as last laid out (head excluded). Hit spheres for the harpoon. */
	const TArray<FTransform>& GetSegmentTransforms() const { return SegmentTransforms; }

	/**
	 * Where the body is drawn, as spheres (world cm) that hold it: one per tube ring (centre and widest point of the ring
	 * as drawn, so it includes the arm's-length clearance and the ridge), plus a few along the head. The serpent has no
	 * collision by design, so a light that must know what it shines on (the diver's lamp) asks here. Appends to Out.
	 */
	void GetDrawnSpheres(TArray<FSphere>& Out) const;

	/** Look state, for tests and DEEPMONSTER debugging. */
	bool IsTubeBody() const { return bTube; }
	int32 GetTubeVertexCount() const { return TubeVerts.Num(); }
	float GetEyeShine01() const { return FMath::Max(EyeShine[0], EyeShine[1]); }
	float GetReveal01() const { return Reveal01; }
	bool HasMeshHead() const { return bMeshHead; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** Adds the head position to the trail when it has moved far enough; trims the trail to the body length. */
	void RecordTrail();

	/** Hit-sphere segments along the trail (and the sphere fallback's instances); the tube when it is on. */
	void LayBody();

	void SetupLook();
	/** Places a mesh head from its landmarks; false (and nothing changed) when its mesh does not load. */
	bool SetupMeshHead(const FDeepSerpentBodyTuning& Tuning);
	void BuildHeadShells();
	/** Body length behind the neck (metres): the whole length less the head in front of and behind the socket. */
	float BodyLengthM() const;
	void UpdateMotion(float Dt);
	void BuildTube();
	void UpdateEyes(float Dt);
	void UpdateMarks(float Dt);

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	/** The attach point at the front of the body: the code-built head, a mesh head and the eyes hang from it. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> HeadSocket;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UProceduralMeshComponent> HeadShell;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> JawHinge;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UProceduralMeshComponent> JawShell;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HeadMeshComp;

	/** The continuous body, in world space (the component sits at the origin with absolute transform). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UProceduralMeshComponent> Tube;

	// ---- The sphere fallback (bTubeBody = false) ----

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Jaw;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UInstancedStaticMeshComponent> Body;

	// ---- Eyes ----

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> EyeLeft;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> EyeRight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> EyeLightLeft;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> EyeLightRight;

	/** Its own photophores' light, under the jaw: lights its head and the line in a bite, faintly in the first pass. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> GlowLight;

	void UpdateGlow(float Dt);
	float Glow01 = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkinMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarksMID;

	/** The code-built head's teeth: the skin material, bone-coloured. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TeethMID;

	/** The code-built jaw: the skin, darker, so an open mouth reads as a hole. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MouthMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeMIDs;

	/** The dorsal spikes: dark matte horn. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpikeMID;

	/** The mesh head's clip and grade (M_DragonHead). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeadMeshMID;

	/** Mood from the server: EDeepMonsterState as a byte, and the brain's threat 0..1. */
	UPROPERTY(Replicated)
	uint8 MoodState = 0;

	UPROPERTY(Replicated)
	float MoodThreat = 0.f;

	/** Head positions, newest first. */
	TArray<FVector> Trail;
	TArray<FTransform> SegmentTransforms;
	FVector Heading = FVector::ForwardVector;

	// ---- Local look state (every machine) ----
	bool bTube = false;
	bool bMeshHead = false;
	bool bTubeCreated = false;
	bool bForceReveal = false;
	bool bHeardOnce = false;
	int32 PoseOverride = -1;
	uint8 SeenState = 0;
	float StateAge = 0.f;
	float LifeAge = 0.f;
	float Reveal01 = 0.f;
	float PulsePhase = 0.f;
	float WeavePhase = 0.f;
	float Flinch01 = 0.f;
	float LungeCm = 0.f;
	float GapeDeg = 0.f;
	float YawRateDegPerS = 0.f;
	FVector LastHeading = FVector::ForwardVector;
	float EyeShine[2] = { 0.f, 0.f };

	// ---- Where the body meets the head (socket space, cm), set by SetupLook ----
	/** Centre of the neck where the body starts; the body runs back from it along -X. */
	FVector NeckCutLocalCm = FVector(-80.f, 0.f, 0.f);
	/** Head length behind and in front of the socket (cm). */
	float HeadBackCm = 80.f;
	float HeadFrontCm = 120.f;
	/** The mesh head's mouth (between the tooth rows) in socket space at rest; the bite holds the line there. */
	FVector MouthLocalCm = FVector::ZeroVector;
	/** 0..1: how far the head has settled into holding the line by the mouth (eases in and out of the bite). */
	float BiteHold01 = 0.f;
	/** Mesh head biting: the extra yaw that turns it side-on to the diver (eases back to 0 after). */
	float BiteFaceYaw = 0.f;

	TArray<FVector> TubeVerts, TubeNormals, ShellVerts;
	TArray<FVector2D> TubeUV0, TubeUV1;
	TArray<FLinearColor> TubeColors;
	TArray<FVector> TubeTangents;
	TArray<int32> TubeTris;
};
