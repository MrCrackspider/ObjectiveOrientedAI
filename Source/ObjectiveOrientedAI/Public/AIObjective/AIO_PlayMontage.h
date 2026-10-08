
#pragma once

#include "CoreMinimal.h"
#include "AIObjective.h"
#include "AIObjective/MontagePlaySettings.h"
#include "Animation/AnimMontage.h"
#include "AIO_PlayMontage.generated.h"

class UAnimInstance;

/**
 * Plays a montage on the controlled pawn and finishes once that montage ends
 */
UCLASS(DisplayName = "Play Montage")
class OBJECTIVEORIENTEDAI_API UAIO_PlayMontage : public UAIObjective
{
	GENERATED_BODY()

public:

	/**
	 * Creates an objective which plays a montage and finishes once it ends. The objective comes back
	 * unowned and is only kept alive by the caller, so add it to a container in the same scope
	 * @param InAnimMontage  Montage to play
	 * @param InSettings     How to play it, defaulted throughout
	 */
	static UAIO_PlayMontage* MakeInstance(UAnimMontage* InAnimMontage,
		const FMontagePlaySettings& InSettings = FMontagePlaySettings());

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	virtual void OnInitialized_Implementation() override;
	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;
	
	UFUNCTION()
	TArray<FName> GetMontageSections() const;

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* InAnimMontage, bool bInterrupted);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(ExposeOnSpawn))
	TObjectPtr<UAnimMontage> AnimMontage;

	// How the montage is played
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(ExposeOnSpawn))
	FMontagePlaySettings MontageSettings;

private:

	// Resolves the anim instance of the controlled pawn. Returns false when there is none to play on
	bool CacheAnimInstance();

	// Position to play the montage from, which is StartingPosition until a stop is remembered
	float GetPlayPosition() const;

	// Where the montage stood when this objective last stopped it, or negative when nothing is remembered
	float StoppedPosition = -1.f;

	// Anim instance of the controlled pawn, resolved on initialization and refreshed on start
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> CachedAnimInstance;
};
