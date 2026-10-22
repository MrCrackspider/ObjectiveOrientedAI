// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "AIObjective/MoveToSettings.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIO_MoveTo.generated.h"

struct FPathFollowingResult;
struct FAIRequestID;

/**
 * Moves the controlled pawn to an actor or a location and finishes once it gets there
 */
UCLASS(DisplayName = "Move To")
class OBJECTIVEORIENTEDAI_API UAIO_MoveTo : public UAIObjective
{
	GENERATED_BODY()

	virtual bool IsCompatibleWith_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;

public:

	/**
	 * Creates an objective which moves to an actor, following it if it moves. The objective comes back
	 * unowned and is only kept alive by the caller, so add it to a container in the same scope
	 * @param InTargetActor  Actor to move to
	 * @param InSettings     How to move, defaulted throughout
	 */
	static UAIO_MoveTo* MakeInstance(AActor* InTargetActor, const FMoveToSettings& InSettings = FMoveToSettings());

	/**
	 * Creates an objective which moves to a location. The objective comes back unowned and is only kept
	 * alive by the caller, so add it to a container in the same scope
	 * @param InTargetLocation  Location to move to
	 * @param InSettings        How to move, defaulted throughout
	 */
	static UAIO_MoveTo* MakeInstance(const FVector& InTargetLocation, const FMoveToSettings& InSettings = FMoveToSettings());

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

	// Actor to move to. Takes precedence over TargetLocation when set
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Move To", meta = (ExposeOnSpawn))
	TObjectPtr<AActor> TargetActor = nullptr;

	// Location to move to, used when there is no TargetActor
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Move To", meta = (ExposeOnSpawn))
	FVector TargetLocation = FVector::ZeroVector;

	// How to move
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Move To", meta = (ExposeOnSpawn))
	FMoveToSettings MoveSettings;

protected:
	
	void OnRequestFinished(FAIRequestID Req, const FPathFollowingResult& Res);

	FDelegateHandle PathFinishDelegateHandle;
	
	TWeakObjectPtr<UPathFollowingComponent> PFComp;
};
