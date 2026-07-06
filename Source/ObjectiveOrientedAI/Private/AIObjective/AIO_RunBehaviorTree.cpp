
#include "AIObjective/AIO_RunBehaviorTree.h"

#include "AIController.h"

bool UAIO_RunBehaviorTree::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return IsValid(BehaviorTree);
}

void UAIO_RunBehaviorTree::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued)
	{
		GetOwningController()->BrainComponent->ResumeLogic(FString::Printf(TEXT("AIO_RunBehaviorTree resumed")));
	}
	else
	{
		GetOwningController()->RunBehaviorTree(BehaviorTree);
	}
}

void UAIO_RunBehaviorTree::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (StopReason.WasQueued())
	{
		GetOwningController()->BrainComponent->PauseLogic(FString::Printf(TEXT("AIO_RunBehaviorTree was queued")));
	}
	else
	{
		GetOwningController()->BrainComponent->StopLogic(FString::Printf(TEXT("AIO_RunBehaviorTree was stopped")));
	}
}
