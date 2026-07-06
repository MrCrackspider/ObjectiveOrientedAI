#pragma once

#include "CoreMinimal.h"
#include "ObjectiveContainer.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayTaskOwnerInterface.h"
#include "AIObjective.generated.h"

class UObjectiveComponent;

enum class EAIObjectiveState : uint8
{
	None,
	Running,
	Queued,
	PendingInvalidation
};

UENUM(BlueprintType)
enum class EAIObjectiveStopReason : uint8
{
	Queued			UMETA(Tooltip = "Objective was queued"),
	Failed			UMETA(Tooltip = "Objective invalidated with failure"),
	Succeeded		UMETA(Tooltip = "Objective invalidated with success"),
	Cancelled		UMETA(Tooltip = "Objective was cancelled by other objective"),
	Restarting		UMETA(Tooltip = "Objective is restarting")
};

USTRUCT(BlueprintType)
struct FAIObjectiveReference
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced)
	TObjectPtr<UAIObjective> Objective;
};

USTRUCT(Blueprintable)
struct FStopReasonType
{
	GENERATED_BODY()
	
	FStopReasonType() = default;
	FStopReasonType(EAIObjectiveStopReason InReason) : Reason(InReason) {}

	UPROPERTY(BlueprintReadOnly, Category = "AI Objective")
	EAIObjectiveStopReason Reason = EAIObjectiveStopReason::Failed;

	bool IsSucceeded() const;
	bool WasQueued() const;
	bool IsPendingInvalidation() const;
	bool IsRestarting() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveStoppedSignature, UAIObjective*, AIObjective, const FStopReasonType&, StopReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveInvalidated, bool, bSuccess);

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
	
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	UObjectiveComponent* GetOwningComponent() const { return OwningComponent; }
	
	UFUNCTION(BlueprintPure, Category = "AI Objective")
	class AAIController* GetOwningController() const;

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
	
	UAIObjective* AddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy = false);
	
	/** Initializes this objective. Can only be initialized once per controller.
	 *  Further attempts to initialize will do nothing
	 *  Calls OnInitialized event
	 *  @return True if objective was successfully initialized or already was initialized on given controller
	 */
	bool Initialize(UObjectiveComponent* ObjectiveComponent);

	/** User function to invalidate this objective.
	 *  Removes it from owning controller and calls stop if it was ever started
	 *  @param bSuccess Flag indicates objective was finished with success
	 */
	UFUNCTION(BlueprintCallable, Category = "AI Objective")
	void Invalidate(bool bSuccess = true);
	
	UPROPERTY(BlueprintAssignable, Category = "AI Objective")
	FOnObjectiveStoppedSignature OnObjectiveStoppedEvent;

#if WITH_GAMEPLAY_DEBUGGER_MENU
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
	
	// Tags this objective has
	UPROPERTY(EditAnywhere, Category = "AI Objective")
	FGameplayTagContainer ObjectiveTags;

	// All objectives contain any of this tags will be canceled when this objective added
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective")
	FGameplayTagContainer CancelObjectivesWithTags;

	// All objectives contain any of this tags will be blocked while this objective is running
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective")
	FGameplayTagContainer BlockObjectivesWithTags;

	// All objectives contain any of this tags will be enqueued while this objective is running
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective")
	FGameplayTagContainer EnqueueObjectivesWithTags;

	// Should OnStarted event be called if this objective is restarting
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective")
	bool bAllowRestarting = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI Objective")
	bool bCanBeQueued = true;

private:
	
	void Start();
	void Enqueue();
	void Destroy(const FStopReasonType& StopReason);
	
	friend struct FObjectiveContainer;
	
	UPROPERTY(Transient)
	TObjectPtr<UObjectiveComponent> OwningComponent;
	
	bool bWasEverStarted = false;
	
	EAIObjectiveState State = EAIObjectiveState::None;
	
	UPROPERTY()
	FObjectiveContainer ObjectiveContainer;

};

template <typename T>
T* UAIObjective::CreateSubObjective()
{
	return (T*)CreateSubObjective(T::StaticClass());
}
