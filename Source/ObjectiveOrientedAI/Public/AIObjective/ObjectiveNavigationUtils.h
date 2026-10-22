
#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"

class AController;
class UPathFollowingComponent;
struct FPathFindingQuery;

/**
 * What every moving objective needs to drive any controller through its path following component, doing
 * for it what AAIController does for its own moves
 */
namespace ObjectiveNavigationUtils
{
	/**
	 * Path following component the controller moves with. An AI controller builds one as a named subobject
	 * and hands it out itself, while any other controller is searched for one, which is where the objective
	 * component puts the component it is allowed to add
	 */
	OBJECTIVEORIENTEDAI_API UPathFollowingComponent* FindPathFollowingComponent(const AController& Controller);

	/**
	 * Gives a request naming no filter the AI controller's default one, which it carries for its own moves.
	 * No other controller has one, and such a request is served the navigation data's own default
	 */
	OBJECTIVEORIENTEDAI_API void ApplyDefaultNavigationFilter(const AController& Controller, FAIMoveRequest& MoveRequest);

	/**
	 * Hands the request's strafing flag to the controller. Path following only reads it off an AI
	 * controller, which is what sets it in AAIController::MoveTo, so on any other controller this does nothing
	 */
	OBJECTIVEORIENTEDAI_API void ApplyStrafing(AController& Controller, const FAIMoveRequest& MoveRequest);

	/**
	 * Pulls a location onto the navigation data the controller's agent moves on
	 * @return False when there is no navigation data near enough, leaving InOutLocation untouched
	 */
	OBJECTIVEORIENTEDAI_API bool ProjectToNavigation(const AController& Controller, FVector& InOutLocation);

	/**
	 * Builds the pathfinding query for a request, the way an AI controller builds one for its own moves.
	 * Everything it needs of the agent is asked for through INavAgentInterface, which every controller
	 * implements by forwarding to its pawn, so nothing here is particular to an AI one
	 * @return False when there is no navigation data to query, leaving OutQuery untouched
	 */
	OBJECTIVEORIENTEDAI_API bool BuildPathfindingQuery(const AController& Controller, UPathFollowingComponent& PathFollowing,
		const FAIMoveRequest& MoveRequest, FPathFindingQuery& OutQuery);
}
