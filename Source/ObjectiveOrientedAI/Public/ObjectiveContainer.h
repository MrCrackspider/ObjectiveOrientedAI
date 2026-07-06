#pragma once

#include "ObjectiveContainer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ObjectiveOrientedAI, Log, All);

struct FGameplayTagQuery;
struct FObjectiveContainer;
struct FGameplayTagContainer;
class UAIObjective;
class UObjectiveComponent;

struct FDebugInfo
{
	FDebugInfo() {}
	
	explicit FDebugInfo(const FObjectiveContainer& InObjectiveContainer);

	FDebugInfo(const FDebugInfo& Other);

	FDebugInfo& SetOwnerName(FString&& InName);

	FDebugInfo& Tabulate();

	void Clear();
	
	FString ToString();
	
	FDebugInfo& operator=(FDebugInfo& Other);

	FDebugInfo& operator=(FDebugInfo&& Other);
	
private:
	FString OwnerName;
	FString CurrentObjectivesLine = "Current:";
	TArray<FDebugInfo> CurrentObjectivesInfo;
	FString StackedObjectivesLine = "Pending:";
	TArray<FDebugInfo> StackedObjectivesInfo;
};

struct FNullInitializer
{
	template<typename TObject>
	void operator()(TObject*) const {}
};


USTRUCT()
struct FObjectiveContainer
{
	GENERATED_BODY()

	const TArray<TObjectPtr<UAIObjective>>& GetCurrentObjectives() const { return CurrentObjectives; }
	const TArray<TObjectPtr<UAIObjective>>& GetQueuedObjectives() const { return QueuedObjectives; }
	FObjectiveContainer* GetOwningContainer() const { return OwningContainer; }
	bool IsQueued() const { return bQueued; }
	
	TArray<UAIObjective*> GetObjectivesByTag(const FGameplayTagContainer& InTags) const;
	TArray<UAIObjective*> GetObjectivesByTagQuery(const FGameplayTagQuery& Query) const;
	TArray<UAIObjective*> GetCurrentObjectivesByTag(const FGameplayTagContainer& InTags) const;
	TArray<UAIObjective*> GetQueuedObjectivesByTag(const FGameplayTagContainer& InTags) const;
	
	UAIObjective* TryCreateAIObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> Class) const;
	
	UAIObjective* AddObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* AIObjective, bool bMakeCopy = false);
	UAIObjective* AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass);
	UAIObjective* AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer);
	
	template<class T, typename InitializerFunc = FNullInitializer>
	T* AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerFunc&& Initializer = {});
	
	void ClearObjectives();
	void ClearObjectives(const FGameplayTagQuery& Query);
	void ClearObjectives(const FGameplayTagContainer& Tags);
	
	void SetQueued(bool bNewQueued);
	
private:
	
	friend UAIObjective;
	
	void InvalidateObjective(UAIObjective* AIObjective, bool bSuccess);
	
	static TArray<UAIObjective*> GetObjectivesByTag(const TArray<UAIObjective*>& AIObjectives, const FGameplayTagContainer& InTags);
	
	OBJECTIVEORIENTEDAI_API bool CanAddObjective(const UObjectiveComponent* ObjectiveComponent, const UAIObjective* AIObjective, bool* OutBlocked = nullptr) const;
	bool ShouldObjectiveBeBlocked(TNotNull<const UAIObjective*> InObjective) const;
	bool ShouldObjectiveBeQueued(TNotNull<const UAIObjective*> InObjective) const;
	
	OBJECTIVEORIENTEDAI_API UAIObjective* ActivateObjective(UAIObjective* InAIObjective);
	void ReevaluateQueue(const UObject* LogOwner = nullptr);
	
	// Get objectives and remove them from their containers
	TArray<UAIObjective*> ObtainObjectivesByTag(const FGameplayTagContainer& Tags, TArray<TObjectPtr<UAIObjective>>& SourceObjectives, bool bPreserveOrder = true);
	TArray<UAIObjective*> ObtainObjectivesByTag(const FGameplayTagContainer& Tags);
	TArray<UAIObjective*> ObtainObjectivesByTagQuery(const FGameplayTagQuery& Query);
	TArray<UAIObjective*> ObtainActivatableObjectives();
	
	static void StartObjectives(const TArray<UAIObjective*>& InObjectives);
	static void DestroyObjectives(const TArray<UAIObjective*>& InObjectives, bool bActivationFailure = false);
	static void EnqueueObjectives(const TArray<UAIObjective*>& InObjectives);
	
	static FString ObjectivesToString(const TArray<UAIObjective*>& Objectives);
	
	FObjectiveContainer* OwningContainer = nullptr;
	bool bQueued = false;
	bool bCleaningDown = false;

	UPROPERTY()
	TArray<TObjectPtr<UAIObjective>> CurrentObjectives;
	
	UPROPERTY()
	TArray<TObjectPtr<UAIObjective>> QueuedObjectives;
};

template <class T, typename InitializerType>
T* FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, InitializerType&& Initializer)
{
	if (!CanAddObjective(ObjectiveComponent, T::StaticClass()->template GetDefaultObject<T>()))
	{
		return nullptr;
	}
	
	T* NewObjective = NewObject<T>(ObjectiveComponent);
	NewObjective->Initialize(ObjectiveComponent);
	Initializer(NewObjective);
	if (!IsValid(NewObjective))
	{
		return nullptr;
	}
	return ActivateObjective(NewObjective);
}
