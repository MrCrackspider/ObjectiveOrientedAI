#pragma once

#include "EngineDefines.h"

#include "ObjectiveContainer.generated.h"

OBJECTIVEORIENTEDAI_API DECLARE_LOG_CATEGORY_EXTERN(ObjectiveOrientedAI, Log, All);

struct FGameplayTagQuery;
struct FObjectiveContainer;
struct FGameplayTagContainer;
class UAIObjective;
class UObjectiveComponent;

UENUM(BlueprintType)
enum class EAIObjectiveStopReason : uint8
{
	Queued			UMETA(Tooltip = "Objective was queued"),
	Failed			UMETA(Tooltip = "Objective invalidated with failure"),
	Succeeded		UMETA(Tooltip = "Objective invalidated with success"),
	Cancelled		UMETA(Tooltip = "Objective was cancelled by other objective"),
	Restarting		UMETA(Tooltip = "Objective is restarting")
};

USTRUCT(Blueprintable)
struct FStopReasonType
{
	GENERATED_BODY()

	FStopReasonType() = default;
	FStopReasonType(EAIObjectiveStopReason InReason) : Reason(InReason) {}

	UPROPERTY(BlueprintReadOnly, Category = "AI Objective")
	EAIObjectiveStopReason Reason = EAIObjectiveStopReason::Failed;
	
	OBJECTIVEORIENTEDAI_API bool IsCompleted() const;
	OBJECTIVEORIENTEDAI_API bool IsSucceeded() const;
	OBJECTIVEORIENTEDAI_API bool WasQueued() const;
	// TODO: Deprecate 
	OBJECTIVEORIENTEDAI_API bool IsPendingInvalidation() const;
	OBJECTIVEORIENTEDAI_API bool IsRestarting() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveStoppedSignature, UAIObjective*, AIObjective, const FStopReasonType&, StopReason);

using FObjectiveStoppedDelegate = FOnObjectiveStoppedSignature::FDelegate;

namespace ObjectiveDelegates
{
	/**
	 * Builds a stopped callback out of an object and one of its UFUNCTIONs.
	 *
	 * Not called directly: MakeStoppedDelegate below is what supplies the reflected name, exactly as
	 * BindDynamic does for a delegate which already exists
	 */
	template <typename UserClass>
	FObjectiveStoppedDelegate MakeStopped(UserClass* Object,
		typename FObjectiveStoppedDelegate::TMethodPtrResolver<UserClass>::FMethodPtr Method, FName FunctionName)
	{
		FObjectiveStoppedDelegate Delegate;
		Delegate.__Internal_BindDynamic(Object, Method, FunctionName);

		return Delegate;
	}
}

/**
 * A stopped callback, bound and ready to be handed to AddObjective in the same expression.
 *
 * A dynamic delegate can only be bound through a macro, since the function's reflected name comes from
 * stringifying it at the call site - which is also why one cannot simply be constructed inline. This is
 * that macro the other way round: it yields the delegate rather than binding one already declared
 *
 * @param UserObject Object holding the function
 * @param FuncName Address of a member UFUNCTION, in the form &UClassName::FunctionName
 */
#define MakeStoppedDelegate(UserObject, FuncName) \
	ObjectiveDelegates::MakeStopped(UserObject, FuncName, STATIC_FUNCTION_FNAME(#FuncName))

#if WITH_GAMEPLAY_DEBUGGER_MENU

/**
 * Snapshot of one objective container, rendered by the gameplay debugger as an indented, coloured tree
 */
struct FDebugInfo
{
	FDebugInfo() = default;

	explicit FDebugInfo(const FObjectiveContainer& InObjectiveContainer);

	/** Name shown for whatever owns the container: the objective component or the parent objective */
	FDebugInfo& SetOwnerName(FString InName);

	/** A labelled line of its own, printed under the owner's name and above any of its objectives */
	FDebugInfo& AddDetail(FString InLabel, FString InValue);

	//~ Begin what an objective adds about itself, printed on the same line as its name
	FDebugInfo& SetDefinitionName(FString InName);
	FDebugInfo& SetTags(FString InTags);
	FDebugInfo& SetDescription(FString InDescription);
	//~ End what an objective adds about itself

	/** Nothing was collected at all, which is what an actor carrying no objective component gives */
	bool IsEmpty() const;

	void Clear();

	FString ToString() const;

private:

	/**
	 * @param StateColor Colour the objective is drawn in, which is decided by the group it sits in rather
	 *                   than by the objective, since the same objective reads differently once its
	 *                   container is paused
	 * @param Prefix	 Printed before the name, carrying the objective's place in its group
	 */
	void AppendTo(FString& OutString, int32 IndentLevel, const TCHAR* StateColor, const TCHAR* Prefix) const;

	// The single line this owner prints about itself, before its own objectives are printed under it
	FString GetHeadline(const TCHAR* StateColor, const TCHAR* Prefix) const;

	static void AppendGroup(FString& OutString, int32 IndentLevel, const TCHAR* HeaderColor, const TCHAR* Header,
		const TCHAR* StateColor, const TArray<FDebugInfo>& Objectives);
	static void AppendLine(FString& OutString, int32 IndentLevel, FStringView Line);

	/** Takes out what the debugger would read as markup or as layout, leaving a name safe to print */
	static FString Sanitize(FString InString);

	FString OwnerName;

	// What the objective owning this container is: its definition asset, its instance tags, its own say
	FString DefinitionName;
	FString Tags;
	FString Description;

	// Lines the owner has to say about itself, as label and value
	TArray<TPair<FString, FString>> Details;

	// Running objective whose rule is keeping this one waiting, empty unless a rule is what put it there
	FString HeldBackBy;

	// Objectives held under this one which the nesting depth preference left out of the snapshot
	int32 TruncatedCount = 0;

	// First in the queue, so the one which resumes next
	bool bIsNext = false;

	// Owning container is paused, so its current objectives are not actually running
	bool bQueued = false;

	TArray<FDebugInfo> CurrentObjectives;
	TArray<FDebugInfo> QueuedObjectives;
};

#endif // WITH_GAMEPLAY_DEBUGGER_MENU

struct FNullInitializer
{
	template<typename TObject>
	void operator()(TObject*) const {}
};

/** 
 * Result of an attempt to add an objective to a container.
 */
USTRUCT()
struct FObjectiveActivationResult
{
	GENERATED_BODY()

	FObjectiveActivationResult() = default;

	static FObjectiveActivationResult Activated(UAIObjective* InObjective) { return FObjectiveActivationResult(InObjective, true); }

	/** True if the objective was accepted by the container and was either started or queued */
	bool WasActivated() const { return bActivated; }

	/** Activated objective, or null if activation failed or the objective invalidated itself while activating */
	OBJECTIVEORIENTEDAI_API UAIObjective* GetObjective() const;

	/** Typed version of GetObjective() */
	template<typename T>
	T* GetObjective() const { return Cast<T>(GetObjective()); }

	/** True if the objective was activated but did not survive activation, i.e. it invalidated itself from OnStarted */
	OBJECTIVEORIENTEDAI_API bool WasInvalidatedDuringActivation() const;

	explicit operator bool() const { return WasActivated(); }

private:

	FObjectiveActivationResult(UAIObjective* InObjective, bool bInActivated) : Objective(InObjective), bActivated(bInActivated) {}

	UPROPERTY()
	TObjectPtr<UAIObjective> Objective = nullptr;

	bool bActivated = false;
};


USTRUCT()
struct FObjectiveContainer
{
	GENERATED_BODY()

	const TArray<TObjectPtr<UAIObjective>>& GetCurrentObjectives() const { return CurrentObjectives; }
	const TArray<TObjectPtr<UAIObjective>>& GetQueuedObjectives() const { return QueuedObjectives; }
	FObjectiveContainer* GetOwningContainer() const { return OwningContainer; }
	bool IsQueued() const { return bQueued; }

	// How far down the objective tree this container sits. The objective component's own container is zero
	int32 GetNestingLevel() const;

#if WITH_GAMEPLAY_DEBUGGER_MENU

	/**
	 * Whether this container is shallow enough for the gameplay debugger to draw what it holds, per the
	 * nesting depth in the editor preferences
	 */
	bool ShouldGameplayDebug() const;

#endif // WITH_GAMEPLAY_DEBUGGER_MENU

#if ENABLE_VISUAL_LOG

	/**
	 * Whether this container is shallow enough to report to the visual log, per the nesting depth in the
	 * project settings
	 */
	bool ShouldVisualLog() const;

	/**
	 * What the container holds right now, as indented lines to hang under a log headline. Empty groups are
	 * left out, so a container holding nothing adds nothing
	 */
	FString GetStateString() const;

#endif // ENABLE_VISUAL_LOG
	
	TArray<UAIObjective*> GetObjectivesByTag(const FGameplayTagContainer& InTags) const;
	TArray<UAIObjective*> GetObjectivesByTagQuery(const FGameplayTagQuery& Query) const;
	TArray<UAIObjective*> GetCurrentObjectivesByTag(const FGameplayTagContainer& InTags) const;
	TArray<UAIObjective*> GetQueuedObjectivesByTag(const FGameplayTagContainer& InTags) const;
	
	UAIObjective* TryCreateAIObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> Class) const;
	
	FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent, 
		UAIObjective* AIObjective, bool bMakeCopy = false, FObjectiveStoppedDelegate OnStopped = {});
	FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent, 
		TSubclassOf<UAIObjective> AIObjectiveClass, FObjectiveStoppedDelegate OnStopped = {});
	/**
	 * Creates an objective of the given class and adds it. Every other way of adding one from a class,
	 * the templated overload below included, comes through here
	 * @param Initializer Runs on the new objective after it is initialized and before it activates
	 */
	OBJECTIVEORIENTEDAI_API FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent,
		TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer, FObjectiveStoppedDelegate OnStopped = {});

	template<class T, typename InitializerFunc = FNullInitializer>
	FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerFunc&& Initializer = {}, FObjectiveStoppedDelegate OnStopped = {});
	
	void ClearObjectives();
	void ClearObjectives(const FGameplayTagQuery& Query);
	void ClearObjectives(const FGameplayTagContainer& Tags);
	
private:
	
	friend UAIObjective;
	friend UObjectiveComponent;
	
	void SetQueued(bool bNewQueued);
	
	void InvalidateObjective(UAIObjective* AIObjective, bool bSuccess);

	static TArray<UAIObjective*> GetObjectivesByTag(const TArray<UAIObjective*>& AIObjectives, const FGameplayTagContainer& InTags);

	bool CanAddObjective(const UObjectiveComponent* ObjectiveComponent, const UAIObjective* AIObjective) const;

	// The running objective whose definition refuses this one, or null when nothing is holding it out
	const UAIObjective* FindBlockingObjective(TNotNull<const UAIObjective*> InObjective) const;

	// True while an objective can still take part in anything: valid, and not on its way out
	static bool IsObjectiveValid(const UAIObjective* InObjective);

	/**
	 * Brings an objective which has never been initialized all the way up: it is given its component,
	 * handed to the caller's initializer, asked whether it can activate, and then activated. Every step
	 * runs user code which is free to invalidate it, so it is rechecked on the way through
	 */
	FObjectiveActivationResult AddNewObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* InObjective,
		TFunctionRef<void(UAIObjective*)> Initializer, const FObjectiveStoppedDelegate& OnStopped);

	/**
	 * Adds an objective which is already initialized, so it is being re-added rather than created. One
	 * which is waiting stays where it is, one which is running restarts, and one which is held by
	 * nothing goes through activation again
	 */
	FObjectiveActivationResult AddInitializedObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* InObjective,
		const FObjectiveStoppedDelegate& OnStopped);

	FObjectiveActivationResult ActivateObjective(UAIObjective* InAIObjective);

	/**
	 * Applies the enqueue rules to everything the container holds and brings the objectives in line with
	 * the outcome: whatever is no longer held back starts, whatever is now held back steps into the
	 * queue. Objectives already in the state they resolved to are left untouched
	 */
	void ReevaluateQueue(TWeakObjectPtr<const UObject> LogOwner = nullptr);

	/**
	 * Works out which objectives may run side by side. An objective is held back whenever any other
	 * objective which would be running has a definition whose enqueue rule matches its tags
	 * @param OutToRun	 Objectives which should be running, in priority order
	 * @param OutToQueue Objectives which should be waiting, the first one to resume at the front
	 */
	void ResolveEnqueueRules(TArray<TObjectPtr<UAIObjective>>& OutToRun, TArray<TObjectPtr<UAIObjective>>& OutToQueue) const;

	// Index of the lowest priority candidate which another candidate wants queued, or INDEX_NONE
	static int32 FindHeldBackCandidate(const TArray<TObjectPtr<UAIObjective>>& Candidates);

	// Appends the objectives which are still able to run or wait, skipping the ones on their way out
	static void AppendValidObjectives(TArray<TObjectPtr<UAIObjective>>& OutObjectives, const TArray<TObjectPtr<UAIObjective>>& SourceObjectives);

	// Subscribe OnStopped, if bound, to the objective's stopped event
	static void BindStoppedCallback(UAIObjective* InObjective, const FObjectiveStoppedDelegate& OnStopped);

	// Moves every objective matching Predicate out of SourceObjectives, keeping the order of both arrays
	template<typename PredicateType>
	static void ObtainObjectives(TArray<TObjectPtr<UAIObjective>>& OutObjectives, TArray<TObjectPtr<UAIObjective>>& SourceObjectives, PredicateType Predicate);

	// Get objectives and remove them from their containers
	TArray<TObjectPtr<UAIObjective>> ObtainObjectivesByTag(const FGameplayTagContainer& Tags);
	TArray<TObjectPtr<UAIObjective>> ObtainObjectivesByTagQuery(const FGameplayTagQuery& Query);

	// The same, for the objectives InObjective's definition cancels on sight
	TArray<TObjectPtr<UAIObjective>> ObtainObjectivesCancelledBy(const UAIObjective* InObjective);

	// Start / enqueue a single objective. Invalidates it and returns false if it cannot be activated this way
	static bool StartObjective(UAIObjective* InObjective);
	static bool EnqueueObjective(UAIObjective* InObjective);

	/**
	 * Whether the container, as it stands right now, still means for this objective to run or to wait.
	 * The batch helpers below walk a snapshot taken before any of it was applied, and the user code they
	 * run is free to move an objective between the two arrays or drop it altogether, so every entry is
	 * measured against the container again on its turn rather than trusted from the snapshot
	 */
	bool ShouldObjectiveRun(UAIObjective* InObjective) const;
	bool ShouldObjectiveWait(UAIObjective* InObjective) const;

	void StartObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives) const;
	static void DestroyObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives, bool bActivationFailure = false);
	void EnqueueObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives) const;

	FObjectiveContainer* OwningContainer = nullptr;
	bool bQueued = false;
	bool bCleaningDown = false;

	// How deep reevaluation is nested. Starting an objective can lead straight back into this container
	int32 ReevaluationDepth = 0;

	UPROPERTY()
	TArray<TObjectPtr<UAIObjective>> CurrentObjectives;

	UPROPERTY()
	TArray<TObjectPtr<UAIObjective>> QueuedObjectives;
};

template <class T, typename InitializerType>
FObjectiveActivationResult FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerType&& Initializer, FObjectiveStoppedDelegate OnStopped)
{
	// All this adds over the class based overload is the caller's typed initializer, wrapped so the one
	// path from a class to a running objective does not have to be a template itself
	auto TypedInitializer = [&Initializer](UAIObjective* InObjective)
	{
		Initializer(CastChecked<T>(InObjective));
	};

	return AddObjective(ObjectiveComponent, T::StaticClass(), TypedInitializer, MoveTemp(OnStopped));
}
