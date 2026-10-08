
#pragma once

#include "CoreMinimal.h"
#include "ObjectiveComponent.h"
#include "Misc/TVariant.h"
#include "Engine/CancellableAsyncAction.h"
#include "AsyncAction_AddObjectiveAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveInvalidated, bool, bSuccess);

/**
 *
 */
UCLASS()
class OBJECTIVEORIENTEDAI_API UAsyncAction_AddObjectiveAndWait : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "AIObjective|Async", meta = (DefaultToSelf = ObjectiveComponent, BlueprintInternalUseOnly = "TRUE"))
	static UAsyncAction_AddObjectiveAndWait* AddObjectiveAndWait(UObjectiveComponent* ObjectiveComponent, UAIObjective* Objective,
		bool bSuccessOnly = true, bool bCopy = false, bool bInvalidateOnCancel = false);

	UFUNCTION(BlueprintCallable, Category = "AIObjective|Async", meta = (DefaultToSelf = ParentObjective, BlueprintInternalUseOnly = "TRUE"))
	static UAsyncAction_AddObjectiveAndWait* AddSubObjectiveAndWait(UAIObjective* ParentObjective, UAIObjective* ObjectiveToAdd,
		bool bSuccessOnly = true, bool bCopy = false, bool bInvalidateOnCancel = false);

	UPROPERTY(BlueprintAssignable)
	FOnObjectiveInvalidated OnInvalidated;

	UPROPERTY(BlueprintAssignable)
	FOnObjectiveInvalidated OnFailed;

	virtual void Activate() override;
	virtual void Cancel() override;
	virtual bool IsActive() const override;

protected:

	UFUNCTION()
	void OnStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

private:

	TVariant<TWeakObjectPtr<UObjectiveComponent>, TWeakObjectPtr<UAIObjective>> Container;
	TWeakObjectPtr<UAIObjective> Objective;

	bool bInvalidateOnCancel : 1 = false;
	bool bSuccessOnly : 1 = true;
};
