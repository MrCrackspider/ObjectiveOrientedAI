
#include "AIObjective/AIO_RunBehaviorTree.h"

#include "AIController.h"
#include "ObjectiveComponent.h"

UAIO_RunBehaviorTree* UAIO_RunBehaviorTree::MakeInstance(UBehaviorTree* InBehaviorTree)
{
	UAIO_RunBehaviorTree* Objective = NewObject<UAIO_RunBehaviorTree>();

	Objective->BehaviorTree = InBehaviorTree;

	return Objective;
}

#if WITH_EDITOR
FText UAIO_RunBehaviorTree::GetDebugDescription_Implementation() const
{
	return FText::FromString(BehaviorTree ? BehaviorTree->GetName() : TEXT("no tree"));
}
#endif

bool UAIO_RunBehaviorTree::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return IsValid(BehaviorTree) && ObjectiveComponent && ObjectiveComponent->GetAIOwner();
}

void UAIO_RunBehaviorTree::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued)
	{
		GetOwningAIController()->BrainComponent->ResumeLogic(FString::Printf(TEXT("AIO_RunBehaviorTree resumed")));
	}
	else
	{
		GetOwningAIController()->RunBehaviorTree(BehaviorTree);
	}
}

void UAIO_RunBehaviorTree::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (StopReason.WasQueued())
	{
		GetOwningAIController()->BrainComponent->PauseLogic(FString::Printf(TEXT("AIO_RunBehaviorTree was queued")));
	}
	else
	{
		GetOwningAIController()->BrainComponent->StopLogic(FString::Printf(TEXT("AIO_RunBehaviorTree was stopped")));
	}
}
