
#include "AIObjective/AIO_MoveTo.h"

#include "NavigationSystem.h"
#include "ObjectiveComponent.h"
#include "AIObjective/ObjectiveNavigationUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

namespace AIO_MoveToUtils
{
	/** Runs the query and keeps the path only when it came back usable */
	void FindPath(const AController& Controller, const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath)
	{
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller.GetWorld());

		if (!NavigationSystem)
		{
			return;
		}

		const FPathFindingResult PathResult = NavigationSystem->FindPathSync(Query);

		if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
		{
			return;
		}

		// Keeps the path chasing a goal which moves, and lets the navigation system rebuild it when the data
		// underneath changes. Without both of these the pawn walks the path it was given and no other
		if (MoveRequest.IsMoveToActorRequest())
		{
			PathResult.Path->SetGoalActorObservation(*MoveRequest.GetGoalActor(), 100.0f);
		}

		PathResult.Path->EnableRecalculationOnInvalidation(true);

		OutPath = PathResult.Path;
	}

	/**
	 * Serves a move request through the controller's path following component, doing for any controller what
	 * AAIController::MoveTo does for an AI one
	 *
	 * Two things it does are left out, because outside an AI controller there is nothing to do them to: the
	 * strafing flag, which lives on AAIController and is set by the caller for that case, and starting from
	 * the end of the previous path, which this objective never asks for
	 */
	FPathFollowingRequestResult RequestMove(const AController& Controller, UPathFollowingComponent& PathFollowing, const FAIMoveRequest& MoveRequest)
	{
		FPathFollowingRequestResult ResultData;
		ResultData.Code = EPathFollowingRequestResult::Failed;

		bool bCanRequestMove = MoveRequest.IsValid();

		if (bCanRequestMove && !MoveRequest.IsMoveToActorRequest())
		{
			if (MoveRequest.GetGoalLocation().ContainsNaN() || !FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()))
			{
				bCanRequestMove = false;
			}

			// A goal off the navigation data is pulled onto it first when the request asks for that, and the
			// move is turned down when it cannot be: pathfinding towards somewhere the agent can never stand
			// would only fail later and further in
			if (bCanRequestMove && MoveRequest.IsProjectingGoal())
			{
				FVector GoalLocation = MoveRequest.GetGoalLocation();
				bCanRequestMove = ObjectiveNavigationUtils::ProjectToNavigation(Controller, GoalLocation);

				if (bCanRequestMove)
				{
					MoveRequest.UpdateGoalLocation(GoalLocation);
				}
			}
		}

		if (bCanRequestMove && PathFollowing.HasReached(MoveRequest))
		{
			// Reported as a finished move rather than as nothing happening, so that whoever asked for it
			// hears the same answer whether the pawn had to walk or was already standing there
			ResultData.MoveId = PathFollowing.RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
			ResultData.Code = EPathFollowingRequestResult::AlreadyAtGoal;
		}
		else if (bCanRequestMove)
		{
			FPathFindingQuery Query;

			if (ObjectiveNavigationUtils::BuildPathfindingQuery(Controller, PathFollowing, MoveRequest, Query))
			{
				FNavPathSharedPtr Path;
				FindPath(Controller, MoveRequest, Query, Path);

				const FAIRequestID RequestID = Path.IsValid() ? PathFollowing.RequestMove(MoveRequest, Path) : FAIRequestID::InvalidRequest;

				if (RequestID.IsValid())
				{
					ResultData.MoveId = RequestID;
					ResultData.Code = EPathFollowingRequestResult::RequestSuccessful;
				}
			}
		}

		if (ResultData.Code == EPathFollowingRequestResult::Failed)
		{
			ResultData.MoveId = PathFollowing.RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
		}

		return ResultData;
	}
}

UAIO_MoveTo* UAIO_MoveTo::MakeInstance(AActor* InTargetActor, const FMoveToSettings& InSettings)
{
	UAIO_MoveTo* Objective = NewObject<UAIO_MoveTo>();

	Objective->TargetActor = InTargetActor;
	Objective->MoveSettings = InSettings;

	return Objective;
}

UAIO_MoveTo* UAIO_MoveTo::MakeInstance(const FVector& InTargetLocation, const FMoveToSettings& InSettings)
{
	UAIO_MoveTo* Objective = NewObject<UAIO_MoveTo>();

	Objective->TargetLocation = InTargetLocation;
	Objective->MoveSettings = InSettings;

	return Objective;
}

#if WITH_EDITOR
FText UAIO_MoveTo::GetDebugDescription_Implementation() const
{
	const FVector GoalLocation = TargetActor ? TargetActor->GetActorLocation() : TargetLocation;
	const FString Goal = TargetActor ? TargetActor->GetName() : TargetLocation.ToCompactString();

	if (const APawn* Pawn = GetControlledPawn())
	{
		return FText::FromString(FString::Printf(TEXT("To %s, %.0fcm left"), *Goal, FVector::Dist(Pawn->GetActorLocation(), GoalLocation)));
	}

	return FText::FromString(FString::Printf(TEXT("To %s"), *Goal));
}
#endif

bool UAIO_MoveTo::IsCompatibleWith_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	const AController* Controller = ObjectiveComponent ? ObjectiveComponent->GetOwningController() : nullptr;

	if (!Controller)
	{
		return false;
	}

	// Asked of the component rather than tested for on the controller: a path following component which has
	// no movement component to drive, because nothing is possessed or the pawn cannot move, would take the
	// request and then never go anywhere
	const UPathFollowingComponent* PathFollowing = ObjectiveNavigationUtils::FindPathFollowingComponent(*Controller);

	return IsValid(PathFollowing);
}

void UAIO_MoveTo::OnStarted_Implementation(bool bWasQueued)
{
	AController* Controller = GetOwningController();
	UPathFollowingComponent* PathFollowing = Controller ? ObjectiveNavigationUtils::FindPathFollowingComponent(*Controller) : nullptr;

	if (!PathFollowing)
	{
		// IsCompatibleWith found one, so losing it between then and now means the controller is being taken apart
		Invalidate(false);
		return;
	}

	PFComp = PathFollowing;

	FAIMoveRequest MoveRequest = MoveSettings.ToMoveRequest();
	ObjectiveNavigationUtils::ApplyDefaultNavigationFilter(*Controller, MoveRequest);

	TargetActor ? MoveRequest.SetGoalActor(TargetActor) : MoveRequest.SetGoalLocation(TargetLocation);

	const FPathFollowingRequestResult RequestResult = AIO_MoveToUtils::RequestMove(*Controller, *PathFollowing, MoveRequest);

	ObjectiveNavigationUtils::ApplyStrafing(*Controller, MoveRequest);

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
