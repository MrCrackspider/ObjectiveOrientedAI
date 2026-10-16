#include "AIObjective/AIObjectiveComposite.h"

UAIObjectiveComposite* UAIObjectiveComposite::MakeInstance(const TArray<UAIObjective*>& InComposite,
	const TArray<int32>& InLeadObjectiveIndexes, bool bInRestartWhenUnqueued)
{
	UAIObjectiveComposite* Objective = NewObject<UAIObjectiveComposite>();

	Objective->Composite = InComposite;
	Objective->LeadObjectiveIndexes = InLeadObjectiveIndexes;
	Objective->bRestartWhenUnqueued = bInRestartWhenUnqueued;

	return Objective;
}

#if WITH_EDITOR
FText UAIObjectiveComposite::GetDebugDescription_Implementation() const
{
	if (Composite.IsEmpty())
	{
		return FText::FromString(TEXT("empty"));
	}

	const int32 DoneCount = Composite.Num() - CountAliveEntries();
	return FText::FromString(FString::Printf(TEXT("%d/%d done"), DoneCount, Composite.Num()));
}
#endif

bool UAIObjectiveComposite::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	for (const UAIObjective* Objective : Composite)
	{
		if (IsValid(Objective))
		{
			return true;
		}
	}
	return false;
}

void UAIObjectiveComposite::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued && !bRestartWhenUnqueued)
	{
		// Entries are queued alongside the composite and resume with it, so there is nothing to
		// start here. Only completion is worth rechecking
		CheckCompletion();
		return;
	}

	// Guards the teardown below as well as the pass which follows: an entry cancelled here is not an
	// entry which finished, so it must not be read as the composite emptying out
	bStartingEntries = true;

	// The entries which were queued alongside the composite have already been resumed by the time this
	// runs, so starting over means dropping their copies first
	ClearSubObjectives();

	for (int32 EntryIndex = 0; EntryIndex < Composite.Num(); EntryIndex++)
	{
		// A lead entry can end the composite from inside its own activation, so the rest of the
		// list must not be started on top of a teardown
		if (!IsRunning())
		{
			break;
		}

		UAIObjective* Entry = Composite[EntryIndex];

		if (!IsValid(Entry))
		{
			continue;
		}

		// Subscribing through AddSubObjective means the copy reports back even if it stops while
		// activating. Lead entries get their own handler, so nothing has to map a copy back to its slot
		FObjectiveStoppedDelegate Callback;
		if (IsLeadObjective(EntryIndex))
		{
			Callback.BindUObject(this, &UAIObjectiveComposite::OnLeadEntryStopped);
		}
		else
		{
			Callback.BindUObject(this, &UAIObjectiveComposite::OnEntryStopped);
		}

		AddSubObjective(Entry, true, Callback);
	}

	bStartingEntries = false;

	// Entries which never ran, or stopped on the way up, leave nothing behind to wait for
	CheckCompletion();
}

void UAIObjectiveComposite::CheckCompletion()
{
	// The container empties and refills while entries are being added, so it only means anything
	// once the whole list has been started
	if (bStartingEntries || !IsRunning())
	{
		return;
	}

	if (CountAliveEntries() == 0)
	{
		Invalidate(true);
	}
}

void UAIObjectiveComposite::OnEntryStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	// Cancelled entries were stopped from the outside, most often by this composite restarting or
	// tearing down, and completing on those would end the composite in the middle of its own restart
	const bool bRanToCompletion = StopReason.Reason == EAIObjectiveStopReason::Succeeded
		|| StopReason.Reason == EAIObjectiveStopReason::Failed;

	if (!bRanToCompletion)
	{
		return;
	}

	CheckCompletion();
}

void UAIObjectiveComposite::OnLeadEntryStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	const bool bRanToCompletion = StopReason.Reason == EAIObjectiveStopReason::Succeeded
		|| StopReason.Reason == EAIObjectiveStopReason::Failed;

	if (!bRanToCompletion || !IsRunning())
	{
		return;
	}

	// A lead entry finishing is the whole composite finishing, whatever else is still running
	Invalidate(true);
}

int32 UAIObjectiveComposite::CountAliveEntries() const
{
	const FObjectiveContainer& Container = GetObjectiveContainer();
	return Container.GetCurrentObjectives().Num() + Container.GetQueuedObjectives().Num();
}
