// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "Async/AsyncAction_AddObjectiveAndWait.h"
#include "Engine/CancellableAsyncAction.h"
#include "ObjectiveAsync_WaitObjectiveInvalidated.generated.h"

/**
 * 
 */
UCLASS()
class OBJECTIVEORIENTEDAI_API UObjectiveAsync_WaitObjectiveInvalidated : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "AIObjective|Async", meta = (BlueprintInternalUseOnly = "True"), DisplayName = "Wait Invalidation")
	static UObjectiveAsync_WaitObjectiveInvalidated* WaitObjectiveInvalidated(UAIObjective* TargetObjective, bool bSuccessOnly = true);
	
	UPROPERTY(BlueprintAssignable, DisplayName = "OnInvalidated")
	FOnObjectiveInvalidated OnObjectiveInvalidated;
	
protected:

	virtual void Activate() override;
	virtual void Cancel() override;
	virtual bool IsActive() const override;

	void ReceiveObjectiveStopped(UAIObjective* UAIObjective, const FStopReasonType& StopReason);

	TWeakObjectPtr<UAIObjective> AIObjective;

	bool bSuccessOnly = false;
};
