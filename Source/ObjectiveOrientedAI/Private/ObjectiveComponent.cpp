// © Volga Studios. All rights reserved.


#include "ObjectiveComponent.h"

#include "AIController.h"

void UObjectiveComponent::OnRegister()
{
	Super::OnRegister();
	if (AIOwner)
	{
		AIOwner->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
}

void UObjectiveComponent::OnUnregister()
{
	if (AIOwner)
	{
		AIOwner->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
	Super::OnUnregister();
}

APawn* UObjectiveComponent::GetPawn() const
{
	return GetAIOwner() ? GetAIOwner()->GetPawn() : nullptr;
}

void UObjectiveComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (OldPawn)
	{
		ClearObjectives();
	}
}

UAIObjective* UObjectiveComponent::TryCreateAIObjective(TSubclassOf<UAIObjective> Class)
{
	return ObjectiveContainer.TryCreateAIObjective(this, Class);
}

void UObjectiveComponent::PauseLogic(const FString& Reason)
{
	ObjectiveContainer.SetQueued(true);
}

EAILogicResuming::Type UObjectiveComponent::ResumeLogic(const FString& Reason)
{
	ObjectiveContainer.SetQueued(false);
	return EAILogicResuming::Continue;
}

void UObjectiveComponent::StopLogic(const FString& Reason)
{
	ObjectiveContainer.ClearObjectives();
}

bool UObjectiveComponent::IsRunning() const
{
	return ObjectiveContainer.GetCurrentObjectives().Num() > 0;
}

bool UObjectiveComponent::IsPaused() const
{
	return ObjectiveContainer.IsQueued();
}

UAIObjective* UObjectiveComponent::AddObjective(UAIObjective* InAIObjective, bool bMakeCopy)
{
	return ObjectiveContainer.AddObjective(this, InAIObjective, bMakeCopy);
}

UAIObjective* UObjectiveComponent::AddObjective(TSubclassOf<UAIObjective> ObjectiveClass)
{
	return ObjectiveContainer.AddObjective(this, ObjectiveClass);
}

void UObjectiveComponent::ClearObjectivesWithTags(FGameplayTagContainer InTags)
{
	ObjectiveContainer.ClearObjectives(InTags);
}

void UObjectiveComponent::ClearObjectivesByTagQuery(FGameplayTagQuery TagQuery)
{
	ObjectiveContainer.ClearObjectives(TagQuery);
}

UAIObjective* UObjectiveComponent::BPAddObjective(UAIObjective* InAIObjective, bool bMakeCopy)
{
	return AddObjective(InAIObjective, bMakeCopy);
}

UAIObjective* UObjectiveComponent::AddObjective(TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer)
{
	return ObjectiveContainer.AddObjective(this, AIObjectiveClass, Initializer);
}

void UObjectiveComponent::ClearObjectives()
{
	ObjectiveContainer.ClearObjectives();
}

#if WITH_GAMEPLAY_DEBUGGER_MENU
FDebugInfo UObjectiveComponent::GetDebugInfo() const
{
	return FDebugInfo(ObjectiveContainer).SetOwnerName(GetName());
}
#endif
