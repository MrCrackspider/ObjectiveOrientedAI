
#include "AIObjective/AIO_MoveTo.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "ObjectiveComponent.h"
#include "GameFramework/Controller.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/PathFollowingComponent.h"

namespace AIO_MoveToUtils
{
	/**
	 * Path following component the controller moves with. An AI controller builds one as a named subobject
	 * and hands it out itself, while any other controller is searched for one, which is where the objective
	 * component puts the component it is allowed to add
	 */
	UPathFollowingComponent* FindPathFollowingComponent(const AController& Controller)
	{
		if (const AAIController* AIController = Cast<const AAIController>(&Controller))
		{
			return AIController->GetPathFollowingComponent();
		}

		return Controller.FindComponentByClass<UPathFollowingComponent>();
	}

	/**
	 * Builds the pathfinding query for a request, the way an AI controller builds one for its own moves.
	 * Everything it needs of the agent is asked for through INavAgentInterface, which every controller
	 * implements by forwarding to its pawn, so nothing here is particular to an AI one
	 * @return False when there is no navigation data to query, leaving OutQuery untouched
	 */
	bool BuildPathfindingQuery(const AController& Controller, UPathFollowingComponent& PathFollowing, const FAIMoveRequest& MoveRequest, FPathFindingQuery& OutQuery)
	{
		const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller.GetWorld());

		if (!NavigationSystem)
		{
			return false;
		}

		// A request which has turned pathfinding off still goes through navigation data, just the abstract
		// kind, which answers every query with the straight line between the two points
		const ANavigationData* NavData = MoveRequest.IsUsingPathfinding()
			? NavigationSystem->GetNavDataForProps(Controller.GetNavAgentPropertiesRef(), Controller.GetNavAgentLocation())
			: NavigationSystem->GetAbstractNavData();

		if (!NavData)
		{
			return false;
		}

		FVector GoalLocation = MoveRequest.GetGoalLocation();

		if (MoveRequest.IsMoveToActorRequest())
		{
			// A goal which is a navigation agent itself is approached where it asks to be approached rather
			// than at its origin, which is what stops a move onto another pawn from aiming inside it
			if (const INavAgentInterface* NavGoal = Cast<const INavAgentInterface>(MoveRequest.GetGoalActor()))
			{
				const FVector Offset = NavGoal->GetMoveGoalOffset(&Controller);
				GoalLocation = FQuatRotationTranslationMatrix(MoveRequest.GetGoalActor()->GetActorQuat(), NavGoal->GetNavAgentLocation()).TransformPosition(Offset);
			}
			else
			{
				GoalLocation = MoveRequest.GetGoalActor()->GetActorLocation();
			}
		}

		const FSharedConstNavQueryFilter NavFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, &Controller, MoveRequest.GetNavigationFilter());

		OutQuery = FPathFindingQuery(Controller, *NavData, Controller.GetNavAgentLocation(), GoalLocation, NavFilter);
		OutQuery.SetAllowPartialPaths(MoveRequest.IsUsingPartialPaths());
		OutQuery.SetRequireNavigableEndLocation(MoveRequest.IsNavigableEndLocationRequired());

		if (MoveRequest.IsApplyingCostLimitFromHeuristic())
		{
			const float HeuristicScale = NavFilter->GetHeuristicScale();
			OutQuery.CostLimit = FPathFindingQuery::ComputeCostLimitFromHeuristic(
				OutQuery.StartLocation, OutQuery.EndLocation, HeuristicScale, MoveRequest.GetCostLimitFactor(), MoveRequest.GetMinimumCostLimit());
		}

		// Lets a component which pathfinds differently, a crowd following one above all, have its say on the
		// query before it is run
		PathFollowing.OnPathfindingQuery(OutQuery);

		return true;
	}

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
				// Not const: the overload which projects for a nav agent rather than against named
				// navigation data is the non const one
				UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller.GetWorld());
				FNavLocation ProjectedLocation;

				if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(
						MoveRequest.GetGoalLocation(), ProjectedLocation, INVALID_NAVEXTENT, &Controller.GetNavAgentPropertiesRef()))
				{
					MoveRequest.UpdateGoalLocation(ProjectedLocation.Location);
				}
				else
				{
					bCanRequestMove = false;
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

			if (BuildPathfindingQuery(Controller, PathFollowing, MoveRequest, Query))
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

#if WITH_EDITOR
FText UAIO_MoveTo::GetDebugDescription_Implementation() const
{
	const FVector GoalLocation = TargetActor ? TargetActor->GetActorLocation() : TargetLocation;
	const FString Goal = TargetActor ? TargetActor->GetName() : TargetLocation.ToCompactString();

	if (const APawn* Pawn = GetControlledPawn())
	{
		return FText::FromString(FString::Printf(TEXT("to %s, %.0fcm left"), *Goal, FVector::Dist(Pawn->GetActorLocation(), GoalLocation)));
	}

	return FText::FromString(FString::Printf(TEXT("to %s"), *Goal));
}
#endif

bool UAIO_MoveTo::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	const AController* Controller = ObjectiveComponent ? ObjectiveComponent->GetOwningController() : nullptr;

	if (!Controller)
	{
		return false;
	}

	// Asked of the component rather than tested for on the controller: a path following component which has
	// no movement component to drive, because nothing is possessed or the pawn cannot move, would take the
	// request and then never go anywhere
	const UPathFollowingComponent* PathFollowing = AIO_MoveToUtils::FindPathFollowingComponent(*Controller);

	return PathFollowing && PathFollowing->IsPathFollowingAllowed();
}

void UAIO_MoveTo::OnStarted_Implementation(bool bWasQueued)
{
	AController* Controller = GetOwningController();
	UPathFollowingComponent* PathFollowing = Controller ? AIO_MoveToUtils::FindPathFollowingComponent(*Controller) : nullptr;

	if (!PathFollowing)
	{
		// CanActivate found one, so losing it between then and now means the controller is being taken apart
		Invalidate(false);
		return;
	}

	PFComp = PathFollowing;

	// An AI controller carries a default filter for its own moves, which is kept for it. No other controller
	// has one, and a request naming no filter is served the navigation data's own default
	TSubclassOf<UNavigationQueryFilter> NavigationFilter = FilterClass;

	if (!NavigationFilter)
	{
		if (const AAIController* AIController = GetOwningAIController())
		{
			NavigationFilter = AIController->GetDefaultNavigationFilterClass();
		}
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetNavigationFilter(NavigationFilter)
	.SetAllowPartialPath(bAllowPartialPath)
	.SetAcceptanceRadius(AcceptableRadius)
	.SetCanStrafe(bAllowStrafe)
	.SetReachTestIncludesAgentRadius(bReachTestIncludesAgentRadius)
	.SetReachTestIncludesGoalRadius(bReachTestIncludesGoalRadius)
	.SetRequireNavigableEndLocation(bRequireNavigableEndLocation)
	.SetProjectGoalLocation(bProjectGoalLocation)
	.SetUsePathfinding(bUsePathfinding);

	TargetActor ? MoveRequest.SetGoalActor(TargetActor) : MoveRequest.SetGoalLocation(TargetLocation);

	const FPathFollowingRequestResult RequestResult = AIO_MoveToUtils::RequestMove(*Controller, *PathFollowing, MoveRequest);

	// The flag the request carries is only read off the AI controller, which is what sets it in
	// AAIController::MoveTo, so the AI case keeps turning the same way it did
	if (AAIController* AIController = GetOwningAIController())
	{
		AIController->bAllowStrafe = MoveRequest.CanStrafe();
	}

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
