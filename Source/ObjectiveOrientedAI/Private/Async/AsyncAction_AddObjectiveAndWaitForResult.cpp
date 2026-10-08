
#include "Async/AsyncAction_AddObjectiveAndWaitForResult.h"

UAsyncAction_AddObjectiveAndWaitForResult* UAsyncAction_AddObjectiveAndWaitForResult::AddObjectiveAndWaitForResult(UObjectiveComponent* ObjectiveComponent, UAIObjective* Objective)
{
	UAsyncAction_AddObjectiveAndWaitForResult* MyObj = NewObject<UAsyncAction_AddObjectiveAndWaitForResult>();
	MyObj->Container.Set<TWeakObjectPtr<UObjectiveComponent>>(ObjectiveComponent);
	MyObj->Objective = Objective;
	return MyObj;
}

UAsyncAction_AddObjectiveAndWaitForResult* UAsyncAction_AddObjectiveAndWaitForResult::AddSubObjectiveAndWaitForResult(UAIObjective* ParentObjective, UAIObjective* Objective)
{
	UAsyncAction_AddObjectiveAndWaitForResult* MyObj = NewObject<UAsyncAction_AddObjectiveAndWaitForResult>();
	MyObj->Container.Set<TWeakObjectPtr<UAIObjective>>(ParentObjective);
	MyObj->Objective = Objective;
	return MyObj;
}

void UAsyncAction_AddObjectiveAndWaitForResult::Activate()
{
	Super::Activate();
	if (!Objective.IsValid())
	{
		Failed.Broadcast();
		Cancel();
		return;
	}

	Objective->OnObjectiveStoppedEvent.AddDynamic(this, &UAsyncAction_AddObjectiveAndWaitForResult::OnStopped);

	// Only activation failure is reported here. An objective which activated and then invalidated itself
	// already reported its result through OnStopped
	bool bActivated = false;

	TWeakObjectPtr<UObjectiveComponent>* Comp = Container.TryGet<TWeakObjectPtr<UObjectiveComponent>>();
	if (Comp && Comp->IsValid())
	{
		bActivated = Comp->Get()->AddObjective(Objective.Get()).WasActivated();
	}

	TWeakObjectPtr<UAIObjective>* ParentObjective = Container.TryGet<TWeakObjectPtr<UAIObjective>>();
	if (ParentObjective && ParentObjective->IsValid())
	{
		bActivated = ParentObjective->Get()->AddSubObjective(Objective.Get()).WasActivated();
	}

	if (!bActivated)
	{
		Failed.Broadcast();
		Cancel();
	}
}

void UAsyncAction_AddObjectiveAndWaitForResult::Cancel()
{
	if (Objective.IsValid())
	{
		Objective->OnObjectiveStoppedEvent.RemoveDynamic(this, &UAsyncAction_AddObjectiveAndWaitForResult::OnStopped);
	}
	Super::Cancel();
}

bool UAsyncAction_AddObjectiveAndWaitForResult::IsActive() const
{
	return Super::IsActive() && Objective.IsValid();
}

void UAsyncAction_AddObjectiveAndWaitForResult::OnStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	if (StopReason.IsPendingInvalidation())
	{
		if (StopReason.IsSucceeded())
		{
			Completed.Broadcast();
		}
		else
		{
			Failed.Broadcast();
		}
		Cancel();
	}
}
