// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "AIO_Wait.generated.h"

/**
 * Wait certain amount of time and then invalidates itself with success. With value of Time less or equal to zero will wait infinitely 
 */
UCLASS(DisplayName = "Wait")
class OBJECTIVEORIENTEDAI_API UAIO_Wait : public UAIObjective
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "Wait")
	float GetRemainingTime() const;
	
protected:

	// Time to wait for. Zero or less to wait infinitely
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta=(ExposeOnSpawn, Units = Seconds, UIMin = 0.0, ClampMin = 0.0))
	float Time = 0.0;

	// Reset time each time objective is restarting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta=(ExposeOnSpawn))
	bool bResetOnRestart = true;

	// Pause timer if this objective was stopped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta=(ExposeOnSpawn))
	bool bPauseIfQueued = true;

	DECLARE_DELEGATE(FOnCompleted)
	FOnCompleted OnCompleted;

	// TODO: Testing
	static bool InvalidateInTime(UAIObjective* Target, float Time, bool bSuccess = true, bool bResetOnRestart = true, bool bPauseIfQueued = true);
	
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;
	
private:
	
	FTimerHandle TimerHandle;
};
