

#include "AIObjective/AIO_Wait.h"

#include "TimerManager.h"
#include "Engine/World.h"

UAIO_Wait* UAIO_Wait::MakeInstance(float InTime, bool bInResetOnRestart, bool bInPauseIfQueued)
{
	UAIO_Wait* Objective = NewObject<UAIO_Wait>();

	Objective->Time = InTime;
	Objective->bResetOnRestart = bInResetOnRestart;
	Objective->bPauseIfQueued = bInPauseIfQueued;

	return Objective;
}

bool UAIO_Wait::InvalidateInTime(UAIObjective* Target, float InTime, bool bSuccess, bool bInResetOnRestart, bool bInPauseIfQueued)
{
	if (!IsValid(Target))
	{
		return false;
	}

	UAIO_Wait* WaitObjective = MakeInstance(InTime, bInResetOnRestart, bInPauseIfQueued);

	WaitObjective->ObjectiveToInvalidate = Target;
	WaitObjective->bInvalidateTargetWithSuccess = bSuccess;

	return Target->AddSubObjective(WaitObjective).WasActivated();
}

#if WITH_EDITOR
FText UAIO_Wait::GetDebugDescription_Implementation() const
{
	if (Time <= 0.0)
	{
		return FText::FromString(TEXT("no time limit"));
	}

	if (!GetWorld() || !TimerHandle.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("%.1fs"), Time));
	}

	const bool bPaused = GetWorld()->GetTimerManager().IsTimerPaused(TimerHandle);
	return FText::FromString(FString::Printf(TEXT("%.1fs left%s"), GetRemainingTime(), bPaused ? TEXT(" (paused)") : TEXT("")));
}
#endif

float UAIO_Wait::GetRemainingTime() const
{
	if (!GetWorld())
	{
		return -1.0;
	}

	const FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	return TimerManager.GetTimerRemaining(TimerHandle);
}

void UAIO_Wait::OnStarted_Implementation(bool bWasQueued)
{
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	
	if (bResetOnRestart || !TimerHandle.IsValid())
	{
		// Do nothing if objective starts the first time with Time <= 0
		if (Time <= 0.0)
		{
			return;
		}
		TWeakObjectPtr WeakThis(this);
		TimerManager.SetTimer(TimerHandle, [WeakThis]{ if (WeakThis.IsValid()) WeakThis->Invalidate(true); }, Time, false);
		return;
	}
	
	// Unpause timer if we have one
	if (TimerManager.IsTimerPaused(TimerHandle))
	{
		TimerManager.UnPauseTimer(TimerHandle);
	}
}

void UAIO_Wait::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (StopReason.WasQueued() && bPauseIfQueued)
	{
		GetWorld()->GetTimerManager().PauseTimer(TimerHandle);
		return;
	}

	if (!StopReason.IsPendingInvalidation())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);

	// Only the time running out is this objective completing. Being cancelled by something else, or
	// failing, means the wait never finished, and acting on it would carry that stop straight on to the
	// target - up to and including the objective which is tearing this one down
	if (!StopReason.IsSucceeded())
	{
		return;
	}

	if (UAIObjective* Target = ObjectiveToInvalidate.Get())
	{
		Target->Invalidate(bInvalidateTargetWithSuccess);
	}
}
