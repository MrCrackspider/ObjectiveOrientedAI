
#include "AIObjective/ObjectiveNavigationUtils.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Controller.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/PathFollowingComponent.h"

namespace ObjectiveNavigationUtils
{
	UPathFollowingComponent* FindPathFollowingComponent(const AController& Controller)
	{
		if (const AAIController* AIController = Cast<const AAIController>(&Controller))
		{
			return AIController->GetPathFollowingComponent();
		}

		return Controller.FindComponentByClass<UPathFollowingComponent>();
	}

	void ApplyDefaultNavigationFilter(const AController& Controller, FAIMoveRequest& MoveRequest)
	{
		if (MoveRequest.GetNavigationFilter())
		{
			return;
		}

		if (const AAIController* AIController = Cast<const AAIController>(&Controller))
		{
			MoveRequest.SetNavigationFilter(AIController->GetDefaultNavigationFilterClass());
		}
	}

	void ApplyStrafing(AController& Controller, const FAIMoveRequest& MoveRequest)
	{
		if (AAIController* AIController = Cast<AAIController>(&Controller))
		{
			AIController->bAllowStrafe = MoveRequest.CanStrafe();
		}
	}

	bool ProjectToNavigation(const AController& Controller, FVector& InOutLocation)
	{
		// Not const: the overload which projects for a nav agent rather than against named navigation data
		// is the non const one
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller.GetWorld());
		FNavLocation ProjectedLocation;

		if (!NavigationSystem || !NavigationSystem->ProjectPointToNavigation(
				InOutLocation, ProjectedLocation, INVALID_NAVEXTENT, &Controller.GetNavAgentPropertiesRef()))
		{
			return false;
		}

		InOutLocation = ProjectedLocation.Location;
		return true;
	}

	bool BuildPathfindingQuery(const AController& Controller, UPathFollowingComponent& PathFollowing,
		const FAIMoveRequest& MoveRequest, FPathFindingQuery& OutQuery)
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
}
