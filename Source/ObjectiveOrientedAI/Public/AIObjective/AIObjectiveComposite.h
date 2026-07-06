#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "AIObjectiveComposite.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Objective Composite")
class OBJECTIVEORIENTEDAI_API UAIObjectiveComposite : public UAIObjective
{
	GENERATED_BODY()
	
public:
	/**
	 * Sets the composite for this Objective. Only if it was never activated
	 * @param InComposite Composite of objectives to set
	 */
	void SetComposite(const TArray<UAIObjective*>& InComposite);

protected:

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;

	virtual void OnStarted_Implementation(bool bWasQueued) override;
	
	void CheckInvalidationCount();

	UFUNCTION()
	void OnObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason);
	
	bool IsLeadObjective(UAIObjective* AIObjective) const;

	// Array of objectives which will be executed in parallel as subobjectives
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "AI Objective Composite", meta = (ExposeOnSpawn))
	TArray<TObjectPtr<UAIObjective>> Composite;
	
	// This objective will be invalidated when any Composite objective under these indexes invalidates 
	UPROPERTY(EditAnywhere, Category = "AI Objective Composite", BlueprintReadWrite, meta = (ArrayClamp = "Composite", ExposeOnSpawn))
	TArray<int32> LeadObjectiveIndexes;
	
private:
	
	int32 InvalidatedCount = 0;
};
