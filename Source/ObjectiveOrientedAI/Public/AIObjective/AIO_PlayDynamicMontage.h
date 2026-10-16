
#pragma once

#include "CoreMinimal.h"
#include "AIObjective.h"
#include "AIObjective/MontagePlaySettings.h"
#include "Animation/Skeleton.h"
#include "AIO_PlayDynamicMontage.generated.h"

class UAnimMontage;
class UAnimSequence;

/**
 * Everything a dynamic montage objective is built from, beyond the sequence itself: the playback settings
 * of any montage, plus what the montage wrapping the sequence is built out of.
 *
 * Not reflected - the objective keeps its own exposed on spawn properties, and this is what C++ hands to
 * the factory, chained in any order:
 *
 *   UAIO_PlayDynamicMontage::MakeInstance(Sequence, FDynamicMontagePlaySettings().SetLoop(true).SetPlayRate(1.5f));
 */
struct FDynamicMontagePlaySettings : public FMontagePlaySettings
{
	// Montage slot to play on, which must exist in the pawn's anim graph
	FName SlotNodeName = FAnimSlotGroup::DefaultSlotName;

	// Time the montage takes to blend in
	float BlendInTime = 0.25f;

	// Time the montage takes to blend out
	float BlendOutTime = 0.25f;

	// Play the sequence over and over instead of finishing at its end
	bool bLoop = false;

	MONTAGE_PLAY_SETTINGS_BUILDER(FDynamicMontagePlaySettings)

	FDynamicMontagePlaySettings& SetSlotNodeName(FName InSlotNodeName)
	{
		SlotNodeName = InSlotNodeName;
		return *this;
	}

	FDynamicMontagePlaySettings& SetBlendInTime(float InBlendInTime)
	{
		BlendInTime = InBlendInTime;
		return *this;
	}

	FDynamicMontagePlaySettings& SetBlendOutTime(float InBlendOutTime)
	{
		BlendOutTime = InBlendOutTime;
		return *this;
	}

	FDynamicMontagePlaySettings& SetLoop(bool bInLoop)
	{
		bLoop = bInLoop;
		return *this;
	}
};

/**
 * Plays a plain animation sequence on a montage slot
 */
UCLASS(DisplayName = "Play Dynamic Montage")
class OBJECTIVEORIENTEDAI_API UAIO_PlayDynamicMontage : public UAIObjective
{
	GENERATED_BODY()

public:

	/**
	 * Creates an objective which plays a sequence on a slot and finishes once it ends.
	 * @param InAnimSequence   Sequence to play
	 * @param InSettings       What the montage is built from and how it is played, defaulted throughout
	 */
	static UAIO_PlayDynamicMontage* MakeInstance(UAnimSequence* InAnimSequence,
		const FDynamicMontagePlaySettings& InSettings = FDynamicMontagePlaySettings());

	// Montage built for this objective, null until it first starts
	UFUNCTION(BlueprintPure, Category = "Play Dynamic Montage")
	UAnimMontage* GetDynamicMontage() const { return DynamicMontage; }

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;

	void OnMontageObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

	/**
	 * Every slot registered on the skeleton behind AnimSequence.
	 */
	UFUNCTION()
	TArray<FName> GetSlotNames() const;

	// Sequence to play. A montage is played by the Play Montage objective directly, not wrapped in another
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn))
	TObjectPtr<UAnimSequence> AnimSequence;

	// Montage slot to play on, which must exist in the pawn's anim graph
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn, GetOptions = "GetSlotNames"))
	FName SlotNodeName = FAnimSlotGroup::DefaultSlotName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn, ClampMin = "0.0"))
	float BlendInTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn, ClampMin = "0.0"))
	float BlendOutTime = 0.25f;

	/**
	 * Play the sequence over and over.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn))
	bool bLoop = false;

	/**
	 * How the built montage is played. Section Name does nothing here - a generated montage carries a
	 * single section, so there is nowhere to jump to
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Dynamic Montage", meta = (ExposeOnSpawn))
	FMontagePlaySettings MontageSettings;

private:

	// Builds the montage this objective plays. Returns null when the sequence cannot back one
	UAnimMontage* CreateDynamicMontage() const;

	// Montage built from AnimSequence
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> DynamicMontage;

	// Sub objective actually playing the montage, while there is one
	TWeakObjectPtr<UAIObjective> MontageObjective;
};
