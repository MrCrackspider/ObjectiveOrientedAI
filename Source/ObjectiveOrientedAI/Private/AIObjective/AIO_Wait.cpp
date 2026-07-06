

#include "AIObjective/AIO_Wait.h"

bool UAIO_Wait::InvalidateInTime(UAIObjective* Target, float Time, bool bSuccess, bool bResetOnRestart, bool bPauseIfQueued)
{
	if (!IsValid(Target))
	{
		return false;
	}
	UAIO_Wait* WaitObjective = NewObject<UAIO_Wait>(Target);
	WaitObjective->Time = Time;
	WaitObjective->bResetOnRestart = bResetOnRestart;
	WaitObjective->bPauseIfQueued = bPauseIfQueued;
	WaitObjective->OnCompleted.BindWeakLambda(Target, [Target, bSuccess]()
	{
		if (Target)
		{
			Target->Invalidate(bSuccess);
		}
	});
	UAIObjective* Result = Target->AddSubObjective(WaitObjective, false);
	return IsValid(Result);
}

float UAIO_Wait::GetRemainingTime() const
{
	if (!GetWorld())
	{
		return -1.0;
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

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

	if (StopReason.IsPendingInvalidation())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		[[maybe_unused]] bool Temp = OnCompleted.ExecuteIfBound();
	}
}
