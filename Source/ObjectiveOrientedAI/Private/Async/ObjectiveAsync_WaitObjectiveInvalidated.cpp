// Fill out your copyright notice in the Description page of Project Settings.


#include "Async/ObjectiveAsync_WaitObjectiveInvalidated.h"

UObjectiveAsync_WaitObjectiveInvalidated* UObjectiveAsync_WaitObjectiveInvalidated::WaitObjectiveInvalidated(UAIObjective* TargetObjective, bool bSuccessOnly)
{
	UObjectiveAsync_WaitObjectiveInvalidated* MyObj = NewObject<UObjectiveAsync_WaitObjectiveInvalidated>();
	MyObj->AIObjective = TargetObjective;
	MyObj->bSuccessOnly = bSuccessOnly;
	return MyObj;
}

void UObjectiveAsync_WaitObjectiveInvalidated::Activate()
{
	Super::Activate();
	if (!AIObjective.IsValid() || AIObjective->IsPendingInvalidation())
	{
		SetReadyToDestroy();
		return;
	}
	AIObjective->OnObjectiveStopped.AddUObject(this, &UObjectiveAsync_WaitObjectiveInvalidated::ReceiveObjectiveStopped);
}

void UObjectiveAsync_WaitObjectiveInvalidated::Cancel()
{
	if (AIObjective.IsValid())
	{
		AIObjective->OnObjectiveStopped.RemoveAll(this);
	}
	AIObjective.Reset();
	Super::Cancel();
}

bool UObjectiveAsync_WaitObjectiveInvalidated::IsActive() const
{
	return Super::IsActive() && AIObjective.IsValid();
}

void UObjectiveAsync_WaitObjectiveInvalidated::ReceiveObjectiveStopped(UAIObjective* InAIObjective, const FStopReasonType& StopReason)
{
	if (!InAIObjective->IsPendingInvalidation())
	{
		return;
	}

	if (bSuccessOnly && !StopReason.IsSucceeded())
	{
		return;
	}
	
	OnObjectiveInvalidated.Broadcast(StopReason.IsSucceeded());
	
	SetReadyToDestroy();
}
