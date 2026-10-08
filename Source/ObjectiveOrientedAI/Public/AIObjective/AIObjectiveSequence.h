
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

	/**
	 * Creates a sequence which runs the given objectives one after another. The objective comes back
	 * unowned and is only kept alive by the caller, so add it to a container in the same scope
	 * @param InSequence              Entries to run in order. They are templates and are copied on activation
	 * @param InExecutionMode         What happens once the sequence reaches its far end
	 * @param InNumLoops              How many passes to run. Zero or less loops forever
	 * @param InStartIndex            Entry to begin on, counted from whichever end the sequence runs from
	 * @param bInInverse              Run the sequence back to front
	 * @param bInRestartWhenUnqueued  Whether coming back from the queue runs the sequence again from InStartIndex
	 */
	static UAIObjectiveSequence* MakeInstance(const TArray<UAIObjective*>& InSequence,
		EObjectiveSequenceMode InExecutionMode = EObjectiveSequenceMode::Loop, int32 InNumLoops = 0,
		int32 InStartIndex = 0, bool bInInverse = false, bool bInRestartWhenUnqueued = true);

	// Copy of the current entry which is running right now, null while the sequence is between entries
	UFUNCTION(BlueprintPure, Category = "AI Objective Sequence")
	UAIObjective* GetRunningObjective() const { return RunningObjective.Get(); }

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	EObjectiveSequenceMode ExecutionMode = EObjectiveSequenceMode::Loop;

	// Num loops to run this sequence. Zero or less to loop infinitely
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true", ClampMin = 0))
	int32 NumLoops = 0;

	// Index to start sequence from, counted from whichever end the sequence runs from
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true", ClampMin = 0))
	int32 StartIndex = 0;

	// Direction sequence running to. PingPong flips this as it runs, so it is a starting direction
	// rather than a fixed one
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	bool bInverse = false;

	/**
	 * Whether coming back from the queue runs the sequence again from StartIndex, dropping the entry it
	 * was queued on and resetting the loop count. With this off the sequence picks up where it left off
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	bool bRestartWhenUnqueued = true;

	// Array of objectives which will be executed sequentially as subobjectives
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "AI Objective Sequence", meta = (ExposeOnSpawn = "true"))
	TArray<TObjectPtr<UAIObjective>> Sequence;

	// Entry the sequence currently stands on. This is the template, not the copy which is actually running
	UAIObjective* GetCurrentObjective() const;

	// Moves the sequence to InStartIndex and runs the entry found there
	virtual void StartSequence(int32 InStartIndex = 0);

	bool HasAnyValidObjective() const;
	bool IsEndPoint() const;
	bool IsLoop() const;

	UFUNCTION()
	void OnObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

private:

	// Runs entries from the current index until one of them stays alive
	void RunCurrentObjective();

	// Body of RunCurrentObjective. Returns true if the whole pass failed to keep anything running
	bool RunEntries();

	// Steps the index, handling direction, wrapping and the loop counter
	void AdvanceToNextEntry();

	void RetryNextTick();

	// Index the sequence starts its first pass from
	int32 GetInitialIndex() const;

	int32 CurrentObjectiveIndex = 0;
	int32 CurrentLoopNum = 0;

	TWeakObjectPtr<UAIObjective> RunningObjective;

	// True while RunEntries is activating an entry, so a re-entrant stop does not recurse
	bool bStartingEntry = false;

	// Set by a re-entrant stop to tell RunEntries the index has already moved on
	bool bEntryStoppedWhileActivating = false;
};
