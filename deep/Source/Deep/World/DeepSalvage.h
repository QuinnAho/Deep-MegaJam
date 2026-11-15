// A piece of salvage. Its kind (size, kg, value, look) comes from UDeepTuningSettings::SalvageKinds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepSalvage.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class DEEP_API ADeepSalvage : public AActor
{
	GENERATED_BODY()

public:
	ADeepSalvage();

	/** Server: sets what this piece is. Replicates to clients. */
	void SetKind(const FDeepSalvageKind& InKind);

	const FDeepSalvageKind& GetKind() const { return Kind; }

	/** What the diver gets for picking this up. */
	FDeepCargoItem ToCargo() const;

	/** Half the stand-in's size, for reach checks. */
	float GetRadiusM() const { return Kind.SizeM * 0.5f; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Kind();

	void ApplyKind();

	UPROPERTY(ReplicatedUsing = OnRep_Kind, VisibleAnywhere, Category = "Deep")
	FDeepSalvageKind Kind;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Cube;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> Glint;

	float GlintScale = 1.f;

	/** The relic's extra shapes (hidden on other kinds): a gold double cone around a pale core, four shards circling. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> RelicPivot;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> RelicParts;

	bool IsRelic() const { return Kind.Size == EDeepSalvageSize::Relic; }

public:
	/** Scales the glint with depth (style guide 5: 100% at 100 m, 30% at 300 m, 15% at 500 m). The relic keeps its light. */
	void SetGlintScale(float Scale);

	virtual void Tick(float DeltaSeconds) override;
};
