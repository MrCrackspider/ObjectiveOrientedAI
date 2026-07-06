// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "ObjectiveContainer.h"
#include "AIObjective/AIObjective.h"
#include "ObjectiveComponent.generated.h"


UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent), DisplayName = "Objective Component")
class OBJECTIVEORIENTEDAI_API UObjectiveComponent : public UBrainComponent
{
	GENERATED_BODY()
	
public:
	
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	
	UFUNCTION(BlueprintPure, Category = "UObjective Component")
	APawn* GetPawn() const;
	
	UFUNCTION(BlueprintCallable, Category = "UObjective Component", meta = (DeterminesOutputType = "Class"))
	UAIObjective* TryCreateAIObjective(TSubclassOf<UAIObjective> Class);
	
	template <typename T>
	T* TryCreateAIObjective();
	
	FObjectiveContainer& GetObjectiveContainerMutable() { return ObjectiveContainer; }
	const FObjectiveContainer& GetObjectiveContainer() const { return ObjectiveContainer; }
	
	// Begin UBrainComponent
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual void StopLogic(const FString& Reason) override;
	virtual bool IsRunning() const override;
	virtual bool IsPaused() const override;
	// End UBrainComponent
	
	UFUNCTION(BlueprintCallable, Category = "UObjective Component")
	void ClearObjectives();
	
	UFUNCTION(BlueprintCallable, Category = "UObjective Component")
	void ClearObjectivesWithTags(FGameplayTagContainer InTags);
	
	UFUNCTION(BlueprintCallable, Category = "UObjective Component")
	void ClearObjectivesByTagQuery(FGameplayTagQuery TagQuery);

	/**
	 * Adds objective instance to controller. Objective initialized on different controller will fail to activate.
	 * Already active objective will be restarted
	 * @param InAIObjective Objective instance to add to controller
	 * @param bMakeCopy Will make a copy of input objective
	 * @return Copied or input objective
	 */
	UAIObjective* AddObjective(UAIObjective* InAIObjective, bool bMakeCopy = false);
	UAIObjective* AddObjective(TSubclassOf<UAIObjective> ObjectiveClass);
	UAIObjective* AddObjective(TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer);
	
	UFUNCTION(BlueprintCallable, Category = "UObjective Component", meta = (AdvancedDisplay = bMakeCopy), DisplayName = "Add Objective")
	UAIObjective* BPAddObjective(UAIObjective* InAIObjective, bool bMakeCopy = false);
	
	template<class T, typename InitializerType = FNullInitializer>
	T* AddObjective(InitializerType&& Initializer = {});

#if WITH_GAMEPLAY_DEBUGGER_MENU
	FDebugInfo GetDebugInfo() const;
#endif

protected:

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	
	UPROPERTY()
	FObjectiveContainer ObjectiveContainer;
};

template <typename T>
T* UObjectiveComponent::TryCreateAIObjective()
{
	return (T*)TryCreateAIObjective(T::StaticClass());
}

template <class T, typename InitializerType>
T* UObjectiveComponent::AddObjective(InitializerType&& Initializer)
{
	return ObjectiveContainer.AddObjective<T>(this, Forward<InitializerType>(Initializer));
}
