
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
	return FText::FromString(BehaviorTree ? BehaviorTree->GetName() : TEXT("No tree"));
}
#endif

bool UAIO_RunBehaviorTree::IsCompatibleWith_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	// A behavior tree is run by an AI controller and by nothing else, which is a property of the component
	// rather than of this objective: no amount of setting it up makes it runnable on a player controller
	return ObjectiveComponent && ObjectiveComponent->GetAIOwner();
}

bool UAIO_RunBehaviorTree::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	// Stays here rather than moving up with the controller check: the tree can still be given to this
	// objective after it is created, by ExposeOnSpawn or by MakeInstance, so a template without one yet
	// says nothing about whether the objective which runs will have one
	return IsValid(BehaviorTree);
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
