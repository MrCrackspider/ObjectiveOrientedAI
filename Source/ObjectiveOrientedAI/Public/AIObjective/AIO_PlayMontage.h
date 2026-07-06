// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIObjective.h"
#include "Animation/AnimMontage.h"
#include "AIO_PlayMontage.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Play Montage")
class OBJECTIVEORIENTEDAI_API UAIO_PlayMontage : public UAIObjective
{
	GENERATED_BODY()

protected:
	
	virtual void OnInitialized_Implementation() override;
	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;
	
	UFUNCTION()
	TArray<FName> GetMontageSections() const;

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* InAnimMontage, bool bInterrupted);

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	TObjectPtr<UAnimInstance> OptionalAnimInstance;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn))
	TObjectPtr<UAnimMontage> AnimMontage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn, GetOptions = "GetMontageSections"))
	FName SectionName = NAME_None;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn))
	float PlayRate = 1.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn))
	float StartingPosition = 0.0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn))
	bool bInvalidateOnBlendOut = true;
	
};
