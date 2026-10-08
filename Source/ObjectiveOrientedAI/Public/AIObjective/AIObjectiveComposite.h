#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "AIObjectiveComposite.generated.h"

/**
 * Runs a list of objectives in parallel. Completes once every entry has finished, or as soon as any
 * lead entry finishes
 */
UCLASS(DisplayName = "Objective Composite")
class OBJECTIVEORIENTEDAI_API UAIObjectiveComposite : public UAIObjective
{
	GENERATED_BODY()

public:

	/**
	 * Creates a composite which runs the given objectives side by side. The objective comes back unowned
	 * and is only kept alive by the caller, so add it to a container in the same scope
	 * @param InComposite             Entries to run. They are templates and are copied on activation
	 * @param InLeadObjectiveIndexes  Indexes into InComposite whose entries end the composite as soon as they finish
	 * @param bInRestartWhenUnqueued  Whether coming back from the queue runs every entry again from the top
	 */
	static UAIObjectiveComposite* MakeInstance(const TArray<UAIObjective*>& InComposite,
		const TArray<int32>& InLeadObjectiveIndexes = {}, bool bInRestartWhenUnqueued = true);

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

protected:

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;

	virtual void OnStarted_Implementation(bool bWasQueued) override;

	bool IsLeadObjective(int32 EntryIndex) const { return LeadObjectiveIndexes.Contains(EntryIndex); }

	// Invalidates this objective once nothing of it is left running
	void CheckCompletion();

	UFUNCTION()
	void OnEntryStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

	// Bound to lead entries in place of OnEntryStopped, so recognising one costs no bookkeeping
	UFUNCTION()
	void OnLeadEntryStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);

private:

	// Entries still running or queued in this objective's own container
	int32 CountAliveEntries() const;

	// Array of objectives which will be executed in parallel as subobjectives. Read only once the
	// objective exists, since changing it while it runs would desync it from its copies
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "AI Objective Composite", meta = (ExposeOnSpawn, AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UAIObjective>> Composite;

	// This objective will be invalidated when any Composite objective under these indexes invalidates
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Composite", meta = (ArrayClamp = "Composite", ExposeOnSpawn, AllowPrivateAccess = "true"))
	TArray<int32> LeadObjectiveIndexes;

	/**
	 * Whether coming back from the queue runs every entry again from the top, dropping whatever progress
	 * they had made. With this off the entries resume where they left off
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Composite", meta = (ExposeOnSpawn, AllowPrivateAccess = "true"))
	bool bRestartWhenUnqueued = true;

	// True while OnStarted is still filling the container, when it runs empty only by accident
	bool bStartingEntries = false;
};
