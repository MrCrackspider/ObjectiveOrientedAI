
#pragma once

#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "ObjectiveContainer.h"
#include "AIObjective/AIObjective.h"
#include "ObjectiveComponent.generated.h"

class AController;

/**
 * Runs a tree of objectives.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent), DisplayName = "Objective Component")
class OBJECTIVEORIENTEDAI_API UObjectiveComponent : public UBrainComponent
{
	GENERATED_BODY()

public:

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Controller this component runs on.
	 */
	UFUNCTION(BlueprintPure, Category = "Objective Component")
	AController* GetOwningController() const { return OwningController; }

	/**
	 * Pawn the objectives run on. Normally the controller's own, except while objectives are being stopped
	 * because the pawn changed: there it stays the pawn they ran on, which is what they have work to undo on
	 */
	UFUNCTION(BlueprintPure, Category = "Objective Component")
	APawn* GetPawn() const;

	UFUNCTION(BlueprintCallable, Category = "Objective Component", meta = (DeterminesOutputType = "Class"))
	UAIObjective* TryCreateAIObjective(TSubclassOf<UAIObjective> Class);

	template <typename T>
	T* TryCreateAIObjective();

	/** The same for a class only known at runtime, typed off the class handed in rather than cast after */
	template <typename T>
	T* TryCreateAIObjective(TSubclassOf<T> Class);

	FObjectiveContainer& GetObjectiveContainerMutable() { return ObjectiveContainer; }
	const FObjectiveContainer& GetObjectiveContainer() const { return ObjectiveContainer; }
	
	// Begin UBrainComponent
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual void StopLogic(const FString& Reason) override;
	virtual bool IsRunning() const override;
	virtual bool IsPaused() const override;
	// End UBrainComponent
	
	UFUNCTION(BlueprintCallable, Category = "Objective Component")
	void ClearObjectives();
	
	UFUNCTION(BlueprintCallable, Category = "Objective Component")
	void ClearObjectivesWithTags(FGameplayTagContainer InTags);
	
	UFUNCTION(BlueprintCallable, Category = "Objective Component")
	void ClearObjectivesByTagQuery(FGameplayTagQuery TagQuery);

	/**
	 * Adds objective instance to controller. Objective initialized on different controller will fail to activate.
	 * Already active objective will be restarted
	 * @param InAIObjective Objective instance to add to controller
	 * @param bMakeCopy Will make a copy of input objective
	 * @param OnStopped Optional callback subscribed to the added objective before it activates
	 * @return Activation result. Convertible to bool to test activation, GetObjective() returns the copied or
	 *         input objective, or null if it did not survive activation
	 */
	FObjectiveActivationResult AddObjective(UAIObjective* InAIObjective, bool bMakeCopy = false, FObjectiveStoppedDelegate OnStopped = {});
	FObjectiveActivationResult AddObjective(TSubclassOf<UAIObjective> ObjectiveClass, FObjectiveStoppedDelegate OnStopped = {});
	FObjectiveActivationResult AddObjective(TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer, FObjectiveStoppedDelegate OnStopped = {});

	UFUNCTION(BlueprintCallable, Category = "Objective Component", meta = (AdvancedDisplay = bMakeCopy), DisplayName = "Add Objective")
	UAIObjective* BPAddObjective(UAIObjective* InAIObjective, bool bMakeCopy = false);

	template<class T, typename InitializerType = FNullInitializer>
	FObjectiveActivationResult AddObjective(InitializerType&& Initializer = {}, FObjectiveStoppedDelegate OnStopped = {});

	/**
	 * Adds an objective of a class only known at runtime, handing the initializer an objective which is
	 * already typed. T is read off the TSubclassOf, so nothing is cast at the call site
	 */
	template<class T, typename InitializerType
		UE_REQUIRES(std::is_invocable_v<InitializerType, T*>)>
	FObjectiveActivationResult AddObjective(TSubclassOf<T> AIObjectiveClass, InitializerType&& Initializer,
		FObjectiveStoppedDelegate OnStopped = {});

#if WITH_GAMEPLAY_DEBUGGER_MENU
	FDebugInfo GetDebugInfo() const;
#endif

protected:

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/**
	 * Give the owning controller a path following component when it has none of its own.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Objective Component")
	bool bCreatePathFollowingComponent = false;

	/**
	 * Give the owning controller a gameplay tasks component when it has none of its own.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Objective Component")
	bool bCreateGameplayTasksComponent = false;

private:

	/** Adds whichever  optional components has been asked for and the controller does not have */
	void AddRequestedControllerComponents();

	// Controller this component is a part of, cached as it registers
	UPROPERTY(Transient)
	TObjectPtr<AController> OwningController;

	/**
	 * Pawn on its way out, held only for as long as it takes to stop the objectives which were running on
	 * it. Null at every other time, so it never answers for a pawn the controller no longer has
	 */
	UPROPERTY(Transient)
	TObjectPtr<APawn> OutgoingPawn;

	UPROPERTY()
	FObjectiveContainer ObjectiveContainer;
};

template <typename T>
T* UObjectiveComponent::TryCreateAIObjective()
{
	// Null allowed rather than a failed cast: nothing is created at all when no objective of this class
	// could run on this component
	return CastChecked<T>(TryCreateAIObjective(T::StaticClass()), ECastCheckedType::NullAllowed);
}

template <typename T>
T* UObjectiveComponent::TryCreateAIObjective(TSubclassOf<T> Class)
{
	static_assert(std::is_base_of_v<UAIObjective, T>, "TryCreateAIObjective only takes objective classes");

	// Handed on as a plain UClass*, which keeps this overload from being a candidate for its own call.
	// Get() has already answered null for a class which does not derive from T
	return CastChecked<T>(TryCreateAIObjective(Class.Get()), ECastCheckedType::NullAllowed);
}

template <class T, typename InitializerType>
FObjectiveActivationResult UObjectiveComponent::AddObjective(InitializerType&& Initializer, FObjectiveStoppedDelegate OnStopped)
{
	return ObjectiveContainer.AddObjective<T>(this, Forward<InitializerType>(Initializer), MoveTemp(OnStopped));
}

template <class T, typename InitializerType UE_REQUIRES_DEFINITION(std::is_invocable_v<InitializerType, T*>)>
FObjectiveActivationResult UObjectiveComponent::AddObjective(TSubclassOf<T> AIObjectiveClass, InitializerType&& Initializer,
	FObjectiveStoppedDelegate OnStopped)
{
	return ObjectiveContainer.AddObjective<T>(this, AIObjectiveClass, Forward<InitializerType>(Initializer), MoveTemp(OnStopped));
}
