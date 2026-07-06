
#include "AIObjective/AIO_MoveTo.h"

#include "AIController.h"
#include "ObjectiveComponent.h"
#include "Navigation/PathFollowingComponent.h"

bool UAIO_MoveTo::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	if (ObjectiveComponent)
	{
		if (ObjectiveComponent->GetAIOwner())
		{
			return IsValid(ObjectiveComponent->GetAIOwner()->GetPathFollowingComponent());
		}
	}
	return false;
}

void UAIO_MoveTo::OnStarted_Implementation(bool bWasQueued)
{
	AAIController* Controller = GetOwningController();
	PFComp = Controller->GetPathFollowingComponent();
	
	FAIMoveRequest MoveRequest;
	MoveRequest.SetNavigationFilter(FilterClass ? FilterClass : Controller->GetDefaultNavigationFilterClass())
	.SetAllowPartialPath(bAllowPartialPath)
	.SetAcceptanceRadius(AcceptableRadius)
	.SetCanStrafe(bAllowStrafe)
	.SetReachTestIncludesAgentRadius(bReachTestIncludesAgentRadius)
	.SetReachTestIncludesGoalRadius(bReachTestIncludesGoalRadius)
	.SetRequireNavigableEndLocation(bRequireNavigableEndLocation)
	.SetProjectGoalLocation(bProjectGoalLocation)
	.SetUsePathfinding(bUsePathfinding);
	
	TargetActor ? MoveRequest.SetGoalActor(TargetActor) : MoveRequest.SetGoalLocation(TargetLocation);
	
	const FPathFollowingRequestResult RequestResult = Controller->MoveTo(MoveRequest);

	switch (RequestResult.Code)
	{
	case EPathFollowingRequestResult::Failed:
		OnRequestFinished(RequestResult.MoveId, FPathFollowingResult(EPathFollowingResult::Blocked, FPathFollowingResultFlags::Blocked));
		break;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		OnRequestFinished(RequestResult.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
		break;

	case EPathFollowingRequestResult::RequestSuccessful:
		PathFinishDelegateHandle = PFComp->OnRequestFinished.AddUObject(this, &UAIO_MoveTo::OnRequestFinished);
		break;
	
	default:
		checkNoEntry();
		break;
	}
}

void UAIO_MoveTo::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (PFComp.IsValid())
	{
		PFComp->OnRequestFinished.Remove(PathFinishDelegateHandle);
		PFComp->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}
}

void UAIO_MoveTo::OnRequestFinished(FAIRequestID Req, const FPathFollowingResult& Res)
{
	Invalidate(Res.IsSuccess());
}
