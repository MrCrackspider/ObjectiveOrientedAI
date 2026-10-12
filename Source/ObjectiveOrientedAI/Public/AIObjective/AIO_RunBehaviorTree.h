
#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AIO_RunBehaviorTree.generated.h"

// TODO: Add separate component

/**
 * 
 */
UCLASS(DisplayName = "Run Behavior Tree")
class OBJECTIVEORIENTEDAI_API UAIO_RunBehaviorTree : public UAIObjective
{
	GENERATED_BODY()

	virtual bool IsCompatibleWith_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;

public:

	/**
	 * Creates an objective which runs a behavior tree. The objective comes back unowned and is only
	 * kept alive by the caller, so add it to a container in the same scope
	 * @param InBehaviorTree Tree to run
	 */
	static UAIO_RunBehaviorTree* MakeInstance(UBehaviorTree* InBehaviorTree);

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Behavior Tree", meta = (ExposeOnSpawn))
	TObjectPtr<UBehaviorTree> BehaviorTree;

};
