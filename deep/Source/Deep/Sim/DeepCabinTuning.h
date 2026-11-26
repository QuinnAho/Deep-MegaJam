// D2 (Nov 25): BT-01's own cabin as the pilot's view. Every number for it lives here, one property in
// UDeepTuningSettings (DEEP Tuning > Cabin). The eye and the look are in FDeepHelmViewTuning (CabinEyeCm, the servo);
// World/DeepSubmarine builds the cabin's lights and the bow ports' glass from this (ApplyCabin), Sim/DeepCabinRules fits
// a port's pane from traces.

#pragma once

#include "CoreMinimal.h"
#include "DeepCabinTuning.generated.h"

/** One of the cabin's own lights: a fitting or an instrument's glow, in the sub's frame (cm, x ahead, y starboard, z up). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCabinLight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	FVector AtCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	FLinearColor Color = FLinearColor(FColor(0xBF, 0xE3, 0xEA));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	float Intensity = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	float RadiusCm = 250.f;

	/** The fitting's size (cm): a soft highlight on the cabin's worn paint, not a pin point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	float SourceRadiusCm = 6.f;

	/** Casts shadows (the main fitting only: the props' shapes; the glows stay cheap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin")
	bool bShadows = false;
};

USTRUCT(BlueprintType)
struct DEEP_API FDeepCabinTuning
{
	GENERATED_BODY()

	FDeepCabinTuning();

	/**
	 * The bow ports. SM_BT01 is Nanite and every slot is opaque, so its port glass hides the view: the Glass slot wears
	 * PortCutMaterial (the same glass look, masked away inside the ports' box, Nanite-safe) and a pane of PortGlassMaterial
	 * (translucent, lit per pixel, Lumen's front-layer reflections, grime toward the rim) fills each port. Off: the model's
	 * own opaque glass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	bool bPorts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	FName GlassSlot = TEXT("Glass");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	FSoftObjectPath PortCutMaterial = FSoftObjectPath(TEXT("/Game/Deep/Drive/M_PortCut.M_PortCut"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	FSoftObjectPath PortGlassMaterial = FSoftObjectPath(TEXT("/Game/Deep/Drive/M_PortGlass.M_PortGlass"));

	/**
	 * The starboard port's pane (the port side mirrors it in y): its centre and outward normal in the sub's frame and its
	 * radius, measured on BT-01's Glass slot with traces (Deep.World.Cabin.Ports fits them again and checks).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	FVector PortCentreCm = FVector(450.0, 46.4, 133.8);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	FVector PortNormal = FVector(0.535, 0.85, 0.01);

	/** The glass is small: 31 cm across (a grid of traces 3 cm apart meets it out to 14 cm from its middle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	float PortRadiusCm = 15.5f;

	/** The pane's thickness (cm): thick pressure glass, its edge catches the light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	float PortThicknessCm = 5.f;

	/** The cut box round both panes grows by this (cm), so no sliver of the old glass stays at the rim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Ports")
	float PortCutMarginCm = 4.f;

	/** The glass (M_PortGlass's parameters): nearly clear face on, a mirror at a slant, a faint film, grime toward the rim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float OpacityFacing = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float OpacityGrazing = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float Roughness = 0.04f;

	/** Grime starts this share of the radius out and is thickest at the rim; a film this strong lies over all of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float GrimeStart = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float GrimeFilm = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	float GrimeOpacity = 0.8f;

	/** Grime: dried silt and old sealant, dark grey-brown (saturation under 35%, Style Guide 4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	FLinearColor GrimeColor = FLinearColor(0.045f, 0.04f, 0.032f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Glass")
	FLinearColor GlassTint = FLinearColor(0.012f, 0.016f, 0.016f);

	/**
	 * The cabin's own lights (on while the helm is manned): cool, low fittings (the sub's strips, #BFE3EA) and the amber
	 * glow of the instruments (#FFB45A, the only warm light, Style Guide 3). None lights the fog: the cabin is dry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabin|Lights")
	TArray<FDeepCabinLight> Lights;
};
