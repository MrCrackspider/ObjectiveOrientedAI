
#include "Async/AsyncAction_AddObjectiveAndWait.h"

UAsyncAction_AddObjectiveAndWait* UAsyncAction_AddObjectiveAndWait::AddObjectiveAndWait(UObjectiveComponent* ObjectiveComponent, UAIObjective* Objective, bool bSuccessOnly, bool bInvalidateOnCancel)
{
	UAsyncAction_AddObjectiveAndWait* MyObj = NewObject<UAsyncAction_AddObjectiveAndWait>();
	MyObj->Container.Set<TWeakObjectPtr<UObjectiveComponent>>(ObjectiveComponent);
	MyObj->Objective = Objective;
	MyObj->bSuccessOnly = bSuccessOnly;
	MyObj->bInvalidateOnCancel = bInvalidateOnCancel;
	return MyObj;
}

UAsyncAction_AddObjectiveAndWait* UAsyncAction_AddObjectiveAndWait::AddSubObjectiveAndWait(
	UAIObjective* ParentObjective, UAIObjective* ObjectiveToAdd, bool bSuccessOnly, bool bInvalidateOnCancel)
{
	UAsyncAction_AddObjectiveAndWait* MyObj = NewObject<UAsyncAction_AddObjectiveAndWait>();
	MyObj->Container.Set<TWeakObjectPtr<UAIObjective>>(ParentObjective);
	MyObj->Objective = ObjectiveToAdd;
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
	
	TWeakObjectPtr<UObjectiveComponent>* Comp = Container.TryGet<TWeakObjectPtr<UObjectiveComponent>>();
	if (Comp && Comp->IsValid())
	{
		Objective = Comp->Get()->AddObjective(Objective.Get());
	}

	TWeakObjectPtr<UAIObjective>* ParentObjective = Container.TryGet<TWeakObjectPtr<UAIObjective>>();
	if (ParentObjective && ParentObjective->IsValid())
	{
		Objective = ParentObjective->Get()->AddSubObjective(Objective.Get());
	}
	
	if (!Objective.IsValid())
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
