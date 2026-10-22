
#pragma once

#include "CoreMinimal.h"
#include "AISystem.h"
#include "AITypes.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "MoveToSettings.generated.h"

/**
 * How a move to objective moves, everything of an FAIMoveRequest except the goal. Held by the objective
 * itself, so these are the values the details panel and the exposed on spawn pin edit.
 *
 * The goal is left out on purpose: it changes with every move, while these are usually set once and shared.
 * From C++ it is built by chaining onto a default instance, leaving anything untouched at its default:
 *
 *   UAIO_MoveTo::MakeInstance(Target, FMoveToSettings().SetAcceptanceRadius(50.f).SetAllowPartialPath(false));
 */
USTRUCT(BlueprintType)
struct OBJECTIVEORIENTEDAI_API FMoveToSettings
{
	GENERATED_BODY()

	// Fixed distance added to the threshold between AI and goal location in the destination reach test
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AcceptanceRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);

	// "None" falls back to the AI controller's default filter, or the navigation data's own one without an AI controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);

	// Use an incomplete path when the goal can't be reached
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bAllowPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);

	// The goal location needs to be navigable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bRequireNavigableEndLocation = true;

	// Project the goal location onto navigation data (navmesh) before using it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bProjectGoalLocation = true;

	// Pathfind towards the goal. When off, the pawn moves in a straight line
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bUsePathfinding = true;

	// Add the radius of the AI's capsule to the threshold in the destination reach test
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bReachTestIncludesAgentRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);

	// Add the radius of the goal's capsule to the threshold in the destination reach test
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move To")
	bool bReachTestIncludesGoalRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);

	/** Native request carrying these settings, with no goal set yet */
	FAIMoveRequest ToMoveRequest() const
	{
		FAIMoveRequest MoveRequest;
		MoveRequest.SetNavigationFilter(FilterClass)
			.SetAcceptanceRadius(AcceptanceRadius)
			.SetCanStrafe(bAllowStrafe)
			.SetAllowPartialPath(bAllowPartialPath)
			.SetRequireNavigableEndLocation(bRequireNavigableEndLocation)
			.SetProjectGoalLocation(bProjectGoalLocation)
			.SetUsePathfinding(bUsePathfinding)
			.SetReachTestIncludesAgentRadius(bReachTestIncludesAgentRadius)
			.SetReachTestIncludesGoalRadius(bReachTestIncludesGoalRadius);

		return MoveRequest;
	}

	FMoveToSettings& SetAcceptanceRadius(float InAcceptanceRadius)
	{
		AcceptanceRadius = InAcceptanceRadius;
		return *this;
	}

	FMoveToSettings& SetFilterClass(TSubclassOf<UNavigationQueryFilter> InFilterClass)
	{
		FilterClass = InFilterClass;
		return *this;
	}

	FMoveToSettings& SetAllowStrafe(bool bInAllowStrafe)
	{
		bAllowStrafe = bInAllowStrafe;
		return *this;
	}

	FMoveToSettings& SetAllowPartialPath(bool bInAllowPartialPath)
	{
		bAllowPartialPath = bInAllowPartialPath;
		return *this;
	}

	FMoveToSettings& SetRequireNavigableEndLocation(bool bInRequireNavigableEndLocation)
	{
		bRequireNavigableEndLocation = bInRequireNavigableEndLocation;
		return *this;
	}

	FMoveToSettings& SetProjectGoalLocation(bool bInProjectGoalLocation)
	{
		bProjectGoalLocation = bInProjectGoalLocation;
		return *this;
	}

	FMoveToSettings& SetUsePathfinding(bool bInUsePathfinding)
	{
		bUsePathfinding = bInUsePathfinding;
		return *this;
	}

	FMoveToSettings& SetReachTestIncludesAgentRadius(bool bInReachTestIncludesAgentRadius)
	{
		bReachTestIncludesAgentRadius = bInReachTestIncludesAgentRadius;
		return *this;
	}

	FMoveToSettings& SetReachTestIncludesGoalRadius(bool bInReachTestIncludesGoalRadius)
	{
		bReachTestIncludesGoalRadius = bInReachTestIncludesGoalRadius;
		return *this;
	}
};
