#include "AIObjective/AIObjectiveComposite.h"

void UAIObjectiveComposite::SetComposite(const TArray<UAIObjective*>& InComposite)
{
	if (WasEverStarted())
	{
		return;
	}
	Composite = InComposite;
}

bool UAIObjectiveComposite::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	for (const TObjectPtr<UAIObjective> Objective : Composite)
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
	// All subobjectives will start automatically, so skip non-initial start
	if (bWasQueued)
	{
		return;
	}
	InvalidatedCount = 0;
	
	for (int i = 0; i < Composite.Num(); i++)
	{
		TObjectPtr<UAIObjective> Objective = Composite[i];
		
		if (!Objective)
		{
			// Consider invalid objectives as already completed
			InvalidatedCount++;
			continue;
		}
		
		Objective->OnObjectiveStoppedEvent.AddDynamic(this, &UAIObjectiveComposite::OnObjectiveStopped);

		AddSubObjective(Objective, false);
	}
	
	// Check objective completion in case all of subobjectives "completed" on start up
	CheckInvalidationCount();
}

void UAIObjectiveComposite::CheckInvalidationCount()
{
	if(InvalidatedCount == Composite.Num())
	{
		Invalidate(true);
	}
}

void UAIObjectiveComposite::OnObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	if (!AIObjective->IsPendingInvalidation())
	{
		return;
	}
	
	InvalidatedCount++;
	
	if (IsLeadObjective(AIObjective))
	{
		Invalidate(true);
	}
	else
	{
		CheckInvalidationCount();
	}
}

bool UAIObjectiveComposite::IsLeadObjective(UAIObjective* AIObjective) const
{
	int32 Index = Composite.Find(AIObjective);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return LeadObjectiveIndexes.Find(Index) != INDEX_NONE;
}
