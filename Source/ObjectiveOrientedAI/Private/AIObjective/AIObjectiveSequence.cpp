
#include "AIObjective/AIObjectiveSequence.h"

#include "Engine/World.h"
#include "TimerManager.h"

UAIObjectiveSequence* UAIObjectiveSequence::MakeInstance(const TArray<UAIObjective*>& InSequence,
	EObjectiveSequenceMode InExecutionMode, int32 InNumLoops, int32 InStartIndex, bool bInInverse, bool bInRestartWhenUnqueued)
{
	UAIObjectiveSequence* Objective = NewObject<UAIObjectiveSequence>();

	Objective->Sequence = InSequence;
	Objective->ExecutionMode = InExecutionMode;
	Objective->NumLoops = InNumLoops;
	Objective->StartIndex = InStartIndex;
	Objective->bInverse = bInInverse;
	Objective->bRestartWhenUnqueued = bInRestartWhenUnqueued;

	return Objective;
}

UAIObjective* UAIObjectiveSequence::GetCurrentObjective() const
{
	return Sequence.IsValidIndex(CurrentObjectiveIndex) ? Sequence[CurrentObjectiveIndex] : nullptr;
}

#if WITH_EDITOR
FText UAIObjectiveSequence::GetDebugDescription_Implementation() const
{
	if (Sequence.IsEmpty())
	{
		return FText::FromString(TEXT("Empty"));
	}

	FString Description = FString::Printf(TEXT("%d/%d"), CurrentObjectiveIndex + 1, Sequence.Num());

	if (const UAIObjective* Running = GetRunningObjective())
	{
		Description += TEXT(" ") + Running->GetName();
	}
	else if (const UAIObjective* Entry = GetCurrentObjective())
	{
		Description += TEXT(" ") + Entry->GetName() + TEXT(" (starting)");
	}

	if (NumLoops > 0)
	{
		Description += FString::Printf(TEXT(", loop %d/%d"), CurrentLoopNum + 1, NumLoops);
	}

	if (bInverse)
	{
		Description += TEXT(", inverse");
	}

	return FText::FromString(Description);
}
#endif

bool UAIObjectiveSequence::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return HasAnyValidObjective();
}

void UAIObjectiveSequence::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued && !bRestartWhenUnqueued)
	{
		// A queued sequence keeps its entry queued alongside it, so it resumes on its own. It only
		// needs a push when it was queued between entries, with nothing left to resume
		if (!RunningObjective.IsValid())
		{
			RunCurrentObjective();
		}
		return;
	}

	// The entry which was queued alongside the sequence has already been resumed by the time this runs,
	// so starting over means dropping it first. Cancelling it is not a completion, so it does not step
	// the index on its way out
	ClearSubObjectives();

	RunningObjective = nullptr;
	CurrentLoopNum = 0;
	StartSequence(GetInitialIndex());
}

void UAIObjectiveSequence::StartSequence(int32 InStartIndex)
{
	CurrentObjectiveIndex = InStartIndex;
	RunCurrentObjective();
}

void UAIObjectiveSequence::RunCurrentObjective()
{
	// Not running covers being queued and being torn down, and a valid running entry means the
	// sequence is already busy, which is what a stale retry timer would find
	if (!IsRunning() || bStartingEntry || RunningObjective.IsValid())
	{
		return;
	}

	bStartingEntry = true;
	const bool bStalled = RunEntries();
	bStartingEntry = false;

	if (bStalled)
	{
		RetryNextTick();
	}
}

bool UAIObjectiveSequence::RunEntries()
{
	// An entry which stops the moment it starts sends the sequence straight back here, so a single
	// burst never walks further than one pass over the sequence
	int32 AttemptsLeft = Sequence.Num();
	bool bSawRunnableEntry = false;

	while (AttemptsLeft-- > 0)
	{
		if (!IsRunning())
		{
			return false;
		}

		if (!Sequence.IsValidIndex(CurrentObjectiveIndex))
		{
			Invalidate(false);
			return false;
		}

		UAIObjective* Entry = Sequence[CurrentObjectiveIndex];

		if (!IsValid(Entry))
		{
			AdvanceToNextEntry();
			continue;
		}

		bSawRunnableEntry = true;

		bEntryStoppedWhileActivating = false;

		// Subscribing through AddSubObjective means the copy reports back even if it stops while activating
		const FObjectiveActivationResult Result = AddSubObjective(Entry, true,
			FObjectiveStoppedDelegate::CreateUObject(this, &UAIObjectiveSequence::OnEntryStopped));

		if (UAIObjective* StartedEntry = Result.GetObjective())
		{
			// Running or queued, so the sequence moves on when the copy reports back
			RunningObjective = StartedEntry;
			return false;
		}

		// The copy either never ran or did not survive activation. Only the latter reports back,
		// and its callback has already stepped the index for us
		if (!bEntryStoppedWhileActivating)
		{
			AdvanceToNextEntry();
		}
	}

	// A whole pass with nothing left to run at all is a dead sequence, not a stalled one
	if (!bSawRunnableEntry)
	{
		Invalidate(false);
		return false;
	}

	return true;
}

void UAIObjectiveSequence::AdvanceToNextEntry()
{
	if (Sequence.IsEmpty())
	{
		Invalidate(false);
		return;
	}

	if (!IsEndPoint())
	{
		CurrentObjectiveIndex += bInverse ? -1 : 1;
		return;
	}

	// Reached the far end, so this pass over the sequence is done
	if (NumLoops > 0 && ++CurrentLoopNum >= NumLoops)
	{
		Invalidate(true);
		return;
	}

	if (IsLoop())
	{
		CurrentObjectiveIndex = bInverse ? Sequence.Num() - 1 : 0;
		return;
	}

	// PingPong walks back the way it came, and stays put when there is nowhere to turn around
	bInverse = !bInverse;
	CurrentObjectiveIndex = FMath::Clamp(CurrentObjectiveIndex + (bInverse ? -1 : 1), 0, Sequence.Num() - 1);
}

void UAIObjectiveSequence::RetryNextTick()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		Invalidate(false);
		return;
	}

	// Every entry of this pass stopped the instant it started. Retrying next tick keeps the
	// sequence alive without locking up the game thread
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAIObjectiveSequence::RunCurrentObjective));
}

int32 UAIObjectiveSequence::GetInitialIndex() const
{
	if (Sequence.IsEmpty())
	{
		return INDEX_NONE;
	}

	// StartIndex counts entries from wherever the sequence begins, which is the far end when inverted
	const int32 Offset = FMath::Clamp(StartIndex, 0, Sequence.Num() - 1);
	return bInverse ? Sequence.Num() - 1 - Offset : Offset;
}

bool UAIObjectiveSequence::HasAnyValidObjective() const
{
	for (const UAIObjective* Objective : Sequence)
	{
		if (IsValid(Objective))
		{
			return true;
		}
	}
	return false;
}

bool UAIObjectiveSequence::IsEndPoint() const
{
	return bInverse ? CurrentObjectiveIndex == 0 : CurrentObjectiveIndex == Sequence.Num() - 1;
}

bool UAIObjectiveSequence::IsLoop() const
{
	return ExecutionMode == EObjectiveSequenceMode::Loop;
}

void UAIObjectiveSequence::OnEntryStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	// Cancelled entries were stopped from the outside, most often by this sequence restarting or
	// tearing down, and must not be mistaken for an entry which ran to completion
	const bool bRanToCompletion = StopReason.Reason == EAIObjectiveStopReason::Succeeded
		|| StopReason.Reason == EAIObjectiveStopReason::Failed;

	if (!bRanToCompletion)
	{
		return;
	}

	// A copy of an earlier entry can report back late, and must not step the sequence a second time.
	// RunningObjective is still unset while an entry is activating, so its report is always accepted
	if (RunningObjective.IsValid() && RunningObjective != AIObjective)
	{
		return;
	}

	RunningObjective = nullptr;
	AdvanceToNextEntry();

	if (bStartingEntry)
	{
		// Re-entered from inside AddSubObjective. The pass in RunEntries picks the new index up,
		// so the sequence never recurses once per entry
		bEntryStoppedWhileActivating = true;
		return;
	}

	RunCurrentObjective();
}
