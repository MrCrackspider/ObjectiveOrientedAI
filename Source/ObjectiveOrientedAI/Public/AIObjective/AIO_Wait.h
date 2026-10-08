
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "AIObjective/AIObjective.h"
#include "AIO_Wait.generated.h"

/**
 * Wait certain amount of time and then invalidates itself with success. With value of Time less or equal to zero will wait infinitely 
 */
UCLASS(DisplayName = "Wait")
class OBJECTIVEORIENTEDAI_API UAIO_Wait : public UAIObjective
{
	GENERATED_BODY()

public:

	/**
	 * Creates an objective which waits and then finishes with success. The objective comes back unowned
	 * and is only kept alive by the caller, so add it to a container in the same scope
	 * @param InTime              Seconds to wait. Zero or less waits forever
	 * @param bInResetOnRestart   Start the wait over whenever the objective restarts
	 * @param bInPauseIfQueued    Hold the remaining time while the objective is queued
	 */
	static UAIO_Wait* MakeInstance(float InTime, bool bInResetOnRestart = true, bool bInPauseIfQueued = true);

	/**
	 * Adds a wait as a sub objective of Target and invalidates Target once that wait completes. Only the
	 * time running out counts as completing: a wait which is cancelled, or which fails, leaves Target
	 * alone, and a Target which tears down first takes the wait with it
	 * @param Target             Objective to wait inside of, and to invalidate once the wait is up
	 * @param InTime             Seconds to wait. Zero or less waits forever, so Target is never invalidated
	 * @param bSuccess           Whether Target is invalidated with success
	 * @param bInResetOnRestart  Start the wait over whenever Target restarts
	 * @param bInPauseIfQueued   Hold the remaining time while Target is queued
	 * @return True if the wait was activated on Target
	 */
	static bool InvalidateInTime(UAIObjective* Target, float InTime, bool bSuccess = true,
		bool bInResetOnRestart = true, bool bInPauseIfQueued = true);

	UFUNCTION(BlueprintCallable, Category = "Wait")
	float GetRemainingTime() const;

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	// Time to wait for. Zero or less to wait infinitely
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wait", meta=(ExposeOnSpawn, Units = Seconds, UIMin = 0.0, ClampMin = 0.0))
	float Time = 0.0;

	// Reset time each time objective is restarting
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wait", meta=(ExposeOnSpawn))
	bool bResetOnRestart = true;

	// Pause timer if this objective was stopped
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wait", meta=(ExposeOnSpawn))
	bool bPauseIfQueued = true;

	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;

private:

	FTimerHandle TimerHandle;

	/**
	 * Objective to invalidate once the time runs out, set by InvalidateInTime. Held weakly, so a target
	 * which went away first is simply not told, and the wait never keeps its own target alive
	 */
	TWeakObjectPtr<UAIObjective> ObjectiveToInvalidate;

	// Whether ObjectiveToInvalidate is invalidated with success
	bool bInvalidateTargetWithSuccess = true;
};
