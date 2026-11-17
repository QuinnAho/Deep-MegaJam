// The submarine's outside dress: a believable cheap work submersible built from engine basic shapes round the room's
// colliding box. Visual only: no part has collision, every part stays tight to the box (the nose and window visor sit
// over the bow, which already carries the nose), and nothing sits in the hatch column but the thin tether bridle.
// Reference (dev log, 9:20 AM Nov 15): an open tube frame round a pressure hull, buoyancy foam high, heavy things low
// (ballast tanks, battery pods), ducted thrusters at the stern, skids, racked gas cylinders, lamp housings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

#include "DeepHull.generated.h"

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

	/** BT-01, the bathyscaphe (Oliv1e, Sketchfab, CC BY 4.0), imported to Content/Deep/Subs with Nanite. */
	DEEP_API extern const TCHAR* ModelMesh;

	/** The cave tool keeps clearance round a 12.3 x 3.3 x 5.0 m box (CaveGen cavestring.py, HULL_HALF): the model fits in it. */
	inline const FVector ModelEnvelopeCm(1230.0, 330.0, 500.0);

	/** Where BT-01's belly hatch is in the mesh's own space (cm, x and y; z is the mesh's lowest point), measured on import. */
	inline const FVector2D ModelHatchLocalCm(45.5, 0.0);

	/**
	 * The model's sub-local transform: one uniform scale, as large as fits the envelope (12.3 m long unless the beam or
	 * the height would pass 3.3 or 5.0 m), placed so its belly hatch sits on the hatch column (x 0, y 0) at the belly
	 * of the colliding box (z -OuterCm.Z). MeshBox is the mesh's local bounding box.
	 */
	DEEP_API FTransform ModelTransform(const FBox& MeshBox, const FVector& OuterCm);

	/** Deep.Sub.Model: 1 (default) shows BT-01, 0 the dress of basic shapes. */
	DEEP_API bool WantsModel();
}

/**
 * BT-01 on the hull. Visual only: no collision, so the room's box stays the collider. Deep.Sub.Model picks it or the
 * old dress; the switch hides one or the other in game (SetHiddenInGame, so the hatch's and Deep.Hull's SetVisibility
 * calls are untouched). With the model shown every other mesh on the sub is hidden but the hatch cover and water.
 */
UCLASS(ClassGroup = Deep)
class DEEP_API UDeepHullModel : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDeepHullModel();

	/** Shows BT-01 and hides the box dress (true), or the reverse. Collision is untouched. */
	void ApplySwitch(bool bModel);

	/** 1 model shown, 0 old dress, -1 not applied yet. */
	int32 GetShown() const { return Shown; }

	/** Places the model from the room tuning (DeepHull::ModelTransform). */
	void Fit();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	int32 Shown = -1;
};
