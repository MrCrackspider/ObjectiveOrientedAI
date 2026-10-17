#pragma once

#include "EngineDefines.h"
#include "Templates/Requires.h"
#include "Templates/SubclassOf.h"

#include <type_traits>

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

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnObjectiveStopped, UAIObjective* /*AIObjective*/, const FStopReasonType& /*StopReason*/);

/**
 * A stopped callback, as handed to AddObjective. Built with the delegate's own Create functions -
 * CreateUObject for a member function, CreateLambda or CreateWeakLambda where there is nothing to name -
 * and the function it points at needs no UFUNCTION of its own
 */
using FObjectiveStoppedDelegate = FOnObjectiveStopped::FDelegate;

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
 * Result of an attempt to add an objective to a container, typed as the objective the caller added. The
 * add paths which know that type - a typed instance, a TSubclassOf<T>, an explicit T - return it typed;
 * the rest return FObjectiveActivationResult. A typed result converts to one typed as any base class, so
 * either can be held as the untyped one.
 *
 * Holds the objective weakly, so a result kept past the add call is still safe to read: once the objective
 * is invalidated or collected it simply answers null. It holds nothing alive either, so keeping one around
 * never delays an objective which has been dropped
 */
template<typename T = UAIObjective>
struct TObjectiveActivationResult
{
	TObjectiveActivationResult() = default;

	template<typename OtherType
		UE_REQUIRES(std::is_convertible_v<OtherType*, T*>)>
	TObjectiveActivationResult(const TObjectiveActivationResult<OtherType>& Other)
		: Objective(Other.Objective)
		, bActivated(Other.bActivated)
	{
	}

	static TObjectiveActivationResult Activated(T* InObjective) { return TObjectiveActivationResult(InObjective, true); }

	/** True if the objective was accepted by the container and was either started or queued */
	bool WasActivated() const { return bActivated; }

	/** Activated objective, or null if activation failed or the objective invalidated itself while activating */
	T* GetObjective() const { return Objective.Get(); }

	/** True if the objective was activated but did not survive activation, i.e. it invalidated itself from OnStarted */
	bool WasInvalidatedDuringActivation() const { return bActivated && !Objective.IsValid(); }

	explicit operator bool() const { return WasActivated(); }

private:

	template<typename> friend struct TObjectiveActivationResult;
	friend struct FObjectiveContainer;
	friend class UAIObjective;

	TObjectiveActivationResult(T* InObjective, bool bInActivated) : Objective(InObjective), bActivated(bInActivated) {}

	/**
	 * The same result typed as T. Only an add path may claim this, since only it knows the objective is a
	 * T: it was handed one, or built it from a class deriving from T, and a copy keeps its class. That is
	 * also what makes the cast below unable to fail. An objective which did not survive activation reads as
	 * null here, which is what the typed result would answer for it anyway
	 */
	static TObjectiveActivationResult CastFrom(const TObjectiveActivationResult<UAIObjective>& Result)
	{
		return TObjectiveActivationResult(CastChecked<T>(Result.Objective.Get(), ECastCheckedType::NullAllowed), Result.bActivated);
	}

	TWeakObjectPtr<T> Objective;

	bool bActivated = false;
};

using FObjectiveActivationResult = TObjectiveActivationResult<UAIObjective>;


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
	
	// Exported, as is every untyped overload: the typed templates below compile into whichever module calls
	// them, and it is from there that they call these
	OBJECTIVEORIENTEDAI_API FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent,
		UAIObjective* AIObjective, bool bMakeCopy = false, FObjectiveStoppedDelegate OnStopped = {});
	OBJECTIVEORIENTEDAI_API FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent,
		TSubclassOf<UAIObjective> AIObjectiveClass, FObjectiveStoppedDelegate OnStopped = {});
	/**
	 * Creates an objective of the given class and adds it. Every other way of adding one from a class,
	 * the templated overload below included, comes through here
	 * @param Initializer Runs on the new objective after it is initialized and before it activates
	 */
	OBJECTIVEORIENTEDAI_API FObjectiveActivationResult AddObjective(UObjectiveComponent* ObjectiveComponent,
		TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer, FObjectiveStoppedDelegate OnStopped = {});

	/**
	 * The instance overload above, for an instance whose type is known here: the result hands back the
	 * objective - or its copy, which keeps its class - as that type. A plain UAIObjective* still goes to the
	 * untyped overload, as the better match of the two
	 */
	template<typename T
		UE_REQUIRES(std::is_convertible_v<T*, UAIObjective*>)>
	TObjectiveActivationResult<T> AddObjective(UObjectiveComponent* ObjectiveComponent,
		T* AIObjective, bool bMakeCopy = false, FObjectiveStoppedDelegate OnStopped = {});

	/** The class overload above, typed off the class handed in, so the result comes back as that type */
	template<class T>
	TObjectiveActivationResult<T> AddObjective(UObjectiveComponent* ObjectiveComponent,
		TSubclassOf<T> AIObjectiveClass, FObjectiveStoppedDelegate OnStopped = {});

	template<class T, typename InitializerFunc = FNullInitializer>
	TObjectiveActivationResult<T> AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerFunc&& Initializer = {}, FObjectiveStoppedDelegate OnStopped = {});

	/**
	 * The same, for a class which is only known at runtime: the initializer is handed the new objective
	 * already typed, with T read off the TSubclassOf rather than written out. A class property is enough
	 * to get a typed initializer, and one written against the wrong type is a compile error instead of a
	 * cast at the call site which nothing checks until it runs
	 * @param Initializer Runs on the new objective after it is initialized and before it activates
	 */
	template<class T, typename InitializerType
		UE_REQUIRES(std::is_invocable_v<InitializerType, T*>)>
	TObjectiveActivationResult<T> AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<T> AIObjectiveClass,
		InitializerType&& Initializer, FObjectiveStoppedDelegate OnStopped = {});

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

template <typename T UE_REQUIRES_DEFINITION(std::is_convertible_v<T*, UAIObjective*>)>
TObjectiveActivationResult<T> FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent,
	T* AIObjective, bool bMakeCopy, FObjectiveStoppedDelegate OnStopped)
{
	// Handed on as a plain UAIObjective*, for which the untyped overload is the exact match and wins
	return TObjectiveActivationResult<T>::CastFrom(
		AddObjective(ObjectiveComponent, static_cast<UAIObjective*>(AIObjective), bMakeCopy, MoveTemp(OnStopped)));
}

template <class T>
TObjectiveActivationResult<T> FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent,
	TSubclassOf<T> AIObjectiveClass, FObjectiveStoppedDelegate OnStopped)
{
	static_assert(std::is_base_of_v<UAIObjective, T>, "AddObjective only takes objective classes");

	// Handed on as a plain UClass*, which keeps this overload from being a candidate for its own call.
	// Get() answers null for a class which does not derive from T, and nothing is created from a null class
	return TObjectiveActivationResult<T>::CastFrom(
		AddObjective(ObjectiveComponent, TSubclassOf<UAIObjective>(AIObjectiveClass.Get()), MoveTemp(OnStopped)));
}

template <class T, typename InitializerType>
TObjectiveActivationResult<T> FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerType&& Initializer, FObjectiveStoppedDelegate OnStopped)
{
	// All this adds over the class based overload is the caller's typed initializer, wrapped so the one
	// path from a class to a running objective does not have to be a template itself
	auto TypedInitializer = [&Initializer](UAIObjective* InObjective)
	{
		Initializer(CastChecked<T>(InObjective));
	};

	return TObjectiveActivationResult<T>::CastFrom(
		AddObjective(ObjectiveComponent, T::StaticClass(), TypedInitializer, MoveTemp(OnStopped)));
}

template <class T, typename InitializerType UE_REQUIRES_DEFINITION(std::is_invocable_v<InitializerType, T*>)>
TObjectiveActivationResult<T> FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent,
	TSubclassOf<T> AIObjectiveClass, InitializerType&& Initializer, FObjectiveStoppedDelegate OnStopped)
{
	static_assert(std::is_base_of_v<UAIObjective, T>, "AddObjective only takes objective classes");

	auto TypedInitializer = [&Initializer](UAIObjective* InObjective)
	{
		// Cannot fail: TSubclassOf<T>::Get() below answers null for a class which does not derive from T,
		// and a null class is turned away before anything is created
		Initializer(CastChecked<T>(InObjective));
	};

	// Handed on as a plain UClass*, which is what keeps this overload from being a candidate for its own
	// call: TSubclassOf<T> cannot be deduced from a UClass*, so the one below is the untyped overload
	return TObjectiveActivationResult<T>::CastFrom(
		AddObjective(ObjectiveComponent, AIObjectiveClass.Get(), TypedInitializer, MoveTemp(OnStopped)));
}
