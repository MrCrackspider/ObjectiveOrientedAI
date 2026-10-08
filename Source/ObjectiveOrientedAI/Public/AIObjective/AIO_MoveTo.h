// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AISystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AIObjective/AIObjective.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIO_MoveTo.generated.h"

/**
 * 
 */
struct FPathFollowingResult;
struct FAIRequestID;

UCLASS(DisplayName = "Move To")
class OBJECTIVEORIENTEDAI_API UAIO_MoveTo : public UAIObjective
{
	GENERATED_BODY()

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;
	
public:

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	AActor* TargetActor = nullptr;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	FVector TargetLocation{};

	/** fixed distance added to threshold between AI and goal location in destination reach test */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn, ClampMin = "0.0", UIMin="0.0"))
	float AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);

	/** "None" will result in default filter being used */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);

	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bAllowPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);

	/** if set, the goal location will need to be navigable */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bRequireNavigableEndLocation = true;

	/** if set, goal location will be projected on navigation data (navmesh) before using */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bProjectGoalLocation = true;
	
	/** if set, goal location will be projected on navigation data (navmesh) before using */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bUsePathfinding = true;

	/** if set, radius of AI's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bReachTestIncludesAgentRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	
	/** if set, radius of goal's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "MoveTo", meta = (ExposeOnSpawn))
	bool bReachTestIncludesGoalRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);

protected:
	
	void OnRequestFinished(FAIRequestID Req, const FPathFollowingResult& Res);

	FDelegateHandle PathFinishDelegateHandle;
	
	TWeakObjectPtr<UPathFollowingComponent> PFComp;
};
