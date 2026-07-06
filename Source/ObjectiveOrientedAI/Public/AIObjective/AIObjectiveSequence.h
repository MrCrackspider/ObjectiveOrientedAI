// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIObjective.h"
#include "AIObjectiveSequence.generated.h"

UENUM(Blueprintable)
enum class EObjectiveSequenceMode : uint8
{
	Loop		UMETA(Tooltip = "Run sequence from its beggining after loop is completed"),
	PingPong	UMETA(Tooltip = "Run sequence to opposite direction after loop is completed")
};

/**
 * Runs list of objectives sequentially
 */
UCLASS(DisplayName = "Objective Sequence")
class OBJECTIVEORIENTEDAI_API UAIObjectiveSequence : public UAIObjective
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	EObjectiveSequenceMode ExecutionMode = EObjectiveSequenceMode::Loop;

	// Num loops to run this sequence. Zero or less to loop infinitely
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true", ClampMin = 0))
	int32 NumLoops = 0;

	// Index to start sequence from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true", ClampMin = 0))
	int32 StartIndex = 0;

	// Direction sequence running to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	bool Inverse = false;

	// Array of objectives which will be executed sequentially as subobjectives
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	TArray<TObjectPtr<UAIObjective>> Sequence;

	UFUNCTION(BlueprintPure, Category = "AI Objective Sequence")
	UAIObjective* GetCurrentObjective() const;
	
protected:

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;

	virtual void StartSequence(int32 InStartIndex = 0);
	
	int32 GetCurrentIndexClamped() const;
	bool HasAnyValidObjective() const;
	bool IsEndPoint() const;
	bool IsLoop() const;
	//int32 FindNextValidIndex() const;

	UFUNCTION()
	void OnObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);
	
private:

	int32 CurrentObjectiveIndex = 0;
	int32 CurrentLoopNum = 0;
};
