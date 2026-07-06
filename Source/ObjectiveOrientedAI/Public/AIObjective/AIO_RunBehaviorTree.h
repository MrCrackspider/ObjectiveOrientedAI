
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

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Behavior Tree", meta = (ExposeOnSpawn))
	TObjectPtr<UBehaviorTree> BehaviorTree;
	
};
