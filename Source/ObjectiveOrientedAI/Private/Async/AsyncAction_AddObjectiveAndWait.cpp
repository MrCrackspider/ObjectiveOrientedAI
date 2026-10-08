
#include "Async/AsyncAction_AddObjectiveAndWait.h"

UAsyncAction_AddObjectiveAndWait* UAsyncAction_AddObjectiveAndWait::AddObjectiveAndWait(UObjectiveComponent* ObjectiveComponent, UAIObjective* Objective, 
	bool bSuccessOnly, bool bCopy, bool bInvalidateOnCancel)
{
	UAsyncAction_AddObjectiveAndWait* MyObj = NewObject<UAsyncAction_AddObjectiveAndWait>();
	MyObj->Container.Set<TWeakObjectPtr<UObjectiveComponent>>(ObjectiveComponent);
	MyObj->Objective = bCopy ? DuplicateObject(Objective, ObjectiveComponent) : Objective;
	MyObj->bSuccessOnly = bSuccessOnly;
	MyObj->bInvalidateOnCancel = bInvalidateOnCancel;
	return MyObj;
}

UAsyncAction_AddObjectiveAndWait* UAsyncAction_AddObjectiveAndWait::AddSubObjectiveAndWait(UAIObjective* ParentObjective, UAIObjective* ObjectiveToAdd, 
	bool bSuccessOnly, bool bCopy, bool bInvalidateOnCancel)
{
	UAsyncAction_AddObjectiveAndWait* MyObj = NewObject<UAsyncAction_AddObjectiveAndWait>();
	MyObj->Container.Set<TWeakObjectPtr<UAIObjective>>(ParentObjective);
	MyObj->Objective = bCopy ? DuplicateObject(ObjectiveToAdd, ParentObjective) : ObjectiveToAdd;
	MyObj->bSuccessOnly = bSuccessOnly;
	MyObj->bInvalidateOnCancel = bInvalidateOnCancel;
	return MyObj;
}

void UAsyncAction_AddObjectiveAndWait::Activate()
{
	Super::Activate();
	if (!Objective.IsValid())
	{
		OnFailed.Broadcast(false);
		Cancel();
		return;
	}
	
	Objective->OnObjectiveStoppedEvent.AddDynamic(this, &UAsyncAction_AddObjectiveAndWait::OnStopped);

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
		OnFailed.Broadcast(false);
		Cancel();
	}
}

void UAsyncAction_AddObjectiveAndWait::Cancel()
{
	if (Objective.IsValid())
	{
		Objective->OnObjectiveStoppedEvent.RemoveDynamic(this, &UAsyncAction_AddObjectiveAndWait::OnStopped);
		if (bInvalidateOnCancel)
		{
			Objective->Invalidate(false);
		}
	}
	Super::Cancel();
}

bool UAsyncAction_AddObjectiveAndWait::IsActive() const
{
	return Super::IsActive() && Objective.IsValid();
}

void UAsyncAction_AddObjectiveAndWait::OnStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	if (StopReason.IsPendingInvalidation())
	{
		const bool bSucceeded = StopReason.IsSucceeded();
		if (!bSuccessOnly || (bSuccessOnly && bSucceeded))
		{
			OnInvalidated.Broadcast(bSucceeded);
		}
		Cancel();
	}
}
