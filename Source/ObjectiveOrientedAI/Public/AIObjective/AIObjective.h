#pragma once

#include "CoreMinimal.h"
#include "ObjectiveContainer.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayTaskOwnerInterface.h"
#include "AIObjective.generated.h"

class AController;
class UAIObjectiveDefinition;
class UObjectiveComponent;

enum class EAIObjectiveState : uint8
{
	None,
	Running,
	Queued,
	PendingInvalidation
};

UCLASS(meta = (DisplayName = "AI Objective"), BlueprintType, Abstract, Blueprintable, EditInlineNew, CollapseCategories, DefaultToInstanced)
class OBJECTIVEORIENTEDAI_API UAIObjective : public UObject, public IGameplayTaskOwnerInterface, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

	// ----- Public API -----
	
public:

	virtual UWorld* GetWorld() const override;
	
	//~ Begin IGameplayTaskOwnerInterface
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override;
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	virtual uint8 GetGameplayTaskDefaultPriority() const override { return FGameplayTasks::DefaultPriority - 1; }
	//~ End IGameplayTaskOwnerInterface

	//~ Begin IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	//~ End IGameplayTagAssetInterface

	/**
	 * Tags carried by this instance alone, on top of whatever its definition says it is. Free to change at
	 * any time: nothing is reevaluated when they do, so a tag added to a running objective decides how it
	 * is treated from the next add or invalidation onwards rather than settling the queue again there and then
	 */
	const FGameplayTagContainer& GetObjectiveTags() const { return ObjectiveTags; }
	FGameplayTagContainer& GetObjectiveTagsMutable() { return ObjectiveTags; }

	UFUNCTION(BlueprintPure, Category = "AI Objective")
	UAIObjectiveDefinition* GetDefinition() const;

	/**
	 * Hands this objective the definition it plays by, in place of whatever it was built with.
	 *
	 * Takes hold from the next add or invalidation onwards rather than settling the queue again there and then
	 */
	UAIObjective* SetDefinition(UAIObjectiveDefinition* InDefinition);

	/** Every tag this objective answers to: its definition's, plus the ones added to this instance */
	FGameplayTagContainer GetAllObjectiveTags() const;

	/** Whether this objective answers to any of InTags, by definition or by instance tag */
	bool HasAnyObjectiveTags(const FGameplayTagContainer& InTags) const;

	bool MatchesObjectiveTagQuery(const FGameplayTagQuery& Query) const;

	/**
	 * The three rules this objective imposes on another one. They are its definition's rules, so an
	 * objective without a definition imposes nothing, and they are measured against everything the other
	 * objective answers to rather than its definition alone
	 */
	bool ShouldCancelObjective(const UAIObjective* InObjective) const;
	bool ShouldBlockObjective(const UAIObjective* InObjective) const;
	bool ShouldQueueObjective(const UAIObjective* InObjective) const;

	UFUNCTION(BlueprintPure, Category = "AI Objective")
	UObjectiveComponent* GetOwningComponent() const { return OwningComponent; }
	
	/** Controller this objective runs under, whatever kind it is */
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	AController* GetOwningController() const;

	/**
	 * The same controller when it is an AI one, null on every other kind.
	 */
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	class AAIController* GetOwningAIController() const;

	UFUNCTION(BlueprintPure, Category = "AI Objective")
	APawn* GetControlledPawn() const;
	
	EAIObjectiveState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "AI Objective")
	bool IsQueued() const;

	// True if this objective finished its work and will be marked as garbage
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	bool IsPendingInvalidation() const;

	UFUNCTION(BlueprintPure, Category = "AI Objective")
	bool IsRunning() const;

	// Was this objective started at least once
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	bool WasEverStarted() const { return bWasEverStarted; }
	
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	bool IsInitialized() const;
	
	bool CanAddSubObjective(const UAIObjective* InObjective) const;

	UFUNCTION(BlueprintCallable, Category  = "AI Objective")
	UAIObjective* TryCreateSubObjective(TSubclassOf<UAIObjective> Class) const;
	
	template<typename T>
	T* CreateSubObjective();

	UFUNCTION(BlueprintCallable, Category = "AI Objective")
	void ClearSubObjectives();

	const FObjectiveContainer& GetObjectiveContainer() const { return ObjectiveContainer; }

	FObjectiveContainer& GetObjectiveContainer() { return ObjectiveContainer; }
	
	UFUNCTION(BlueprintCallable, Category = "AI Objective", meta = (AdvancedDisplay = bMakeCopy), DisplayName = "Add SubObjective")
	UAIObjective* BPAddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy = false);

	/**
	 * Adds objective instance as a subobjective of this one
	 * @param InAIObjective Objective instance to add
	 * @param bMakeCopy Will make a copy of input objective
	 * @param OnStopped Optional callback subscribed to the added objective before it activates
	 * @return Activation result. Convertible to bool to test activation, GetObjective() returns the copied or
	 *         input objective, or null if it did not survive activation
	 */
	FObjectiveActivationResult AddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy = false, FObjectiveStoppedDelegate OnStopped = {});

	/** User function to invalidate this objective.
	 *  Removes it from owning controller and calls stop if it was ever started
	 *  @param bSuccess Flag indicates objective was finished with success
	 */
	UFUNCTION(BlueprintCallable, Category = "AI Objective")
	void Invalidate(bool bSuccess = true);
	
	/** Broadcast whenever this objective stops for any reason.
	 */
	UPROPERTY(BlueprintAssignable, Category = "AI Objective")
	FOnObjectiveStoppedSignature OnObjectiveStoppedEvent;

#if WITH_EDITOR
	/** 
	 * Short, single line description of this objective's current state, shown inline with its name in the
	 * gameplay debugger. Empty by default
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	FText GetDebugDescription() const;

	virtual FText GetDebugDescription_Implementation() const { return FText::GetEmpty(); }
#endif

#if WITH_GAMEPLAY_DEBUGGER_MENU
	/**
	 * What this objective is called in the debugger: its class, as a reader would name it rather than as
	 * the object is named. Objectives of one class are told apart by their place in the container, which
	 * is printed beside the name
	 */
	FString GetDebugName() const;

	FDebugInfo GetDebugInfo() const;
#endif

protected:

	/** List of overridable methods
	 *  Should be never called directly to avoid crashing
	 *  Does not require call to Super
	 */

	/** Triggered when objective is initialized, usually when it was added to controller or as subobjective.
	 *  By this time, owning controller is valid and all variables should be set
	 *  Called only once per objective lifetime
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	void OnInitialized();

	virtual void OnInitialized_Implementation() {}

	/**
	 * Whether an objective of this class could ever run on this component at all. Asked before anything is
	 * built: on the class default object when the objective is added from a class, on the template when it
	 * is added from an instance, in both cases before any copy is made and before Initialize, so refusing
	 * here costs nothing where refusing from CanActivate costs a copy and an OnInitialized
	 *
	 * Answer on what the component can offer - the kind of controller it is on, what the pawn is, which
	 * components are there to drive. Do not answer on this objective's own data: the object being asked is
	 * a default or a template, and the objective which actually runs may still be given its values by a
	 * subclass default, by ExposeOnSpawn, by the caller's initializer or by OnInitialized. An objective
	 * which wants to read its own state here guards on !HasAnyFlags(RF_ClassDefaultObject), since only a
	 * template carries values worth reading
	 *
	 * Contract: return false only when this class can never run on this component. Everything which can
	 * change over the objective's life belongs in CanActivate
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	bool IsCompatibleWith(const UObjectiveComponent* ObjectiveComponent) const;

	virtual bool IsCompatibleWith_Implementation(const UObjectiveComponent* ObjectiveComponent) const { return true; }

	/** Triggered each time objective is trying to activate
	 * Activation failure leads to objective being invalidated with failure.
	 * Can be called multiple times per objective lifetime
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	bool CanActivate(const UObjectiveComponent* ObjectiveComponent) const;

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const { return true; }

	/** Triggers when objective is successfully activated
	 * @param bWasQueued false if objective was first activated, true if objective was in queue before activation
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	void OnStarted(bool bWasQueued = false);
	
	virtual void OnStarted_Implementation(bool bWasQueued = false) {}

	/** Triggers when objective is stopped by any reason
	 * Can be called multiple times per lifetime, either by objective being queued, invalidated or restarted
	 * @param StopReason contains information about why objective was stopped
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	void OnStopped(const FStopReasonType& StopReason);
	
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) {}

	/** Triggers after objective has been stopped by invalidation
	 * Called only once per lifetime
	 * By this time this objective already removed from owning controller, but reference to it is still valid 
	 * @param bSuccess objective invalidated with success
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "AI Objective")
	void OnInvalidated(bool bSuccess = true);
	
	virtual void OnInvalidated_Implementation(bool bSuccess = true) {}
	
	/**
	 * Definition this objective plays by: what it is, and what it does to the objectives it meets. An
	 * objective without one carries only its instance tags and imposes no rules of its own
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective", meta = (ExposeOnSpawn))
	TObjectPtr<UAIObjectiveDefinition> Definition;

	// Tags this instance carries on top of its definition's, seeded here and free to change at runtime
	UPROPERTY(EditAnywhere, Category = "AI Objective", AdvancedDisplay)
	FGameplayTagContainer ObjectiveTags;

	// Should OnStarted event be called if this objective is restarting
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective", AdvancedDisplay)
	bool bAllowRestarting = true;
	
	UPROPERTY(EditAnywhere, Category = "AI Objective", AdvancedDisplay)
	bool bCanBeQueued = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective", AdvancedDisplay)
	bool bResetSubobjectivesOnRestart = true;

private:

	/** Initializes this objective. Can only be initialized once per controller.
	 *  Further attempts to initialize will do nothing
	 *  Calls OnInitialized event
	 *  @return True if objective was successfully initialized or already was initialized on given controller
	 */
	bool Initialize(UObjectiveComponent* ObjectiveComponent);

	/**
	 *  Initializes this objective, running Initializer on it after it has been given its component but
	 *  before OnInitialized. Setting it up in that window means OnInitialized sees an objective which
	 *  already holds every variable it was given, while the setup itself can still reach the component
	 *  and its pawn
	 *  Initializer is skipped entirely when the objective was already initialized, since there is no
	 *  OnInitialized left to set it up for
	 */
	bool Initialize(UObjectiveComponent* ObjectiveComponent, TFunctionRef<void(UAIObjective*)> Initializer);

	void Start();
	void Enqueue();
	void Destroy(const FStopReasonType& StopReason);

	friend struct FObjectiveContainer;
	
	bool bWasEverStarted = false;
	
	EAIObjectiveState State = EAIObjectiveState::None;
	
	UPROPERTY(Transient)
	TObjectPtr<UObjectiveComponent> OwningComponent;
	
	UPROPERTY()
	FObjectiveContainer ObjectiveContainer;

};

template <typename T>
T* UAIObjective::CreateSubObjective()
{
	return (T*)CreateSubObjective(T::StaticClass());
}
