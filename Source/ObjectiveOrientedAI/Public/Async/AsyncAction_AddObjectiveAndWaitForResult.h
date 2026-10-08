#pragma once

#include "CoreMinimal.h"
#include "ObjectiveComponent.h"
#include "Misc/TVariant.h"
#include "Engine/CancellableAsyncAction.h"
#include "AsyncAction_AddObjectiveAndWaitForResult.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAddObjectiveResult);

UCLASS(meta = (HasDedicatedAsyncNode))
class OBJECTIVEORIENTEDAI_API UAsyncAction_AddObjectiveAndWaitForResult : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "AIObjective|Async", meta = (DefaultToSelf = ObjectiveComponent, BlueprintInternalUseOnly = "TRUE"))
	static UAsyncAction_AddObjectiveAndWaitForResult* AddObjectiveAndWaitForResult(UObjectiveComponent* ObjectiveComponent, UAIObjective* Objective);

	UFUNCTION(BlueprintCallable, Category = "AIObjective|Async", meta = (DefaultToSelf = ParentObjective, BlueprintInternalUseOnly = "TRUE"))
	static UAsyncAction_AddObjectiveAndWaitForResult* AddSubObjectiveAndWaitForResult(UAIObjective* ParentObjective, UAIObjective* Objective);
	
	
	UPROPERTY(BlueprintAssignable)
	FOnAddObjectiveResult Completed;
	
	UPROPERTY(BlueprintAssignable)
	FOnAddObjectiveResult Failed;

	virtual void Activate() override;
	virtual void Cancel() override;
	virtual bool IsActive() const override;

protected:

	UFUNCTION()
	void OnStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

private:

	TVariant<TWeakObjectPtr<UObjectiveComponent>, TWeakObjectPtr<UAIObjective>> Container;
	TWeakObjectPtr<UAIObjective> Objective;
};
