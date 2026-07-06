
#include "AIObjective/AIObjective.h"

#include "AIController.h"
#include "ObjectiveComponent.h"

AAIController* UAIObjective::GetOwningController() const
{
	return GetOwningComponent() ? GetOwningComponent()->GetAIOwner() : nullptr;
}

APawn* UAIObjective::GetControlledPawn() const
{
	AAIController* Controller = GetOwningController();
	return Controller ? Controller->GetPawn() : nullptr;
}

bool FStopReasonType::WasQueued() const
{
	return Reason == EAIObjectiveStopReason::Queued;
}

bool FStopReasonType::IsSucceeded() const
{
	return Reason == EAIObjectiveStopReason::Succeeded;
}

bool FStopReasonType::IsPendingInvalidation() const
{
	return Reason == EAIObjectiveStopReason::Succeeded ||
		Reason == EAIObjectiveStopReason::Failed ||
		Reason == EAIObjectiveStopReason::Cancelled;
}

bool FStopReasonType::IsRestarting() const
{
	return Reason == EAIObjectiveStopReason::Restarting;
}

UWorld* UAIObjective::GetWorld() const
{
	return OwningComponent ? OwningComponent->GetWorld() : nullptr;
}

UGameplayTasksComponent* UAIObjective::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	AAIController* Controller = GetOwningController();
	return Controller ? Controller->GetGameplayTasksComponent() : nullptr;
}

AActor* UAIObjective::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	return GetOwningController();
}

AActor* UAIObjective::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	return GetControlledPawn();
}

void UAIObjective::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer = ObjectiveTags;
}

bool UAIObjective::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return ObjectiveTags.HasTag(TagToCheck);
}

bool UAIObjective::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return ObjectiveTags.HasAll(TagContainer);
}

bool UAIObjective::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return ObjectiveTags.HasAny(TagContainer);
}

bool UAIObjective::IsQueued() const
{
	return State == EAIObjectiveState::Queued;
}

void UAIObjective::Invalidate(bool bSuccess)
{
	if (ObjectiveContainer.GetOwningContainer())
	{
		ObjectiveContainer.GetOwningContainer()->InvalidateObjective(this, bSuccess);
	}
}

#if WITH_GAMEPLAY_DEBUGGER_MENU
FDebugInfo UAIObjective::GetDebugInfo() const
{
	return FDebugInfo(ObjectiveContainer).SetOwnerName(GetName());
}
#endif


bool UAIObjective::IsPendingInvalidation() const
{
	return State == EAIObjectiveState::PendingInvalidation;
}

bool UAIObjective::IsRunning() const
{
	return State == EAIObjectiveState::Running;
}

UAIObjective* UAIObjective::TryCreateSubObjective(TSubclassOf<UAIObjective> Class) const
{
	if (!IsInitialized() || IsPendingInvalidation())
	{
		return nullptr;
	}
	return ObjectiveContainer.TryCreateAIObjective(GetOwningComponent(), Class);
}

bool UAIObjective::IsInitialized() const
{
	return IsValid(OwningComponent);
}

bool UAIObjective::CanAddSubObjective(const UAIObjective* InObjective) const
{
	return IsInitialized() && IsPendingInvalidation() == false && InObjective != this && State != EAIObjectiveState::None;
}

bool UAIObjective::Initialize(UObjectiveComponent* ObjectiveComponent)
{
	if (!IsValid(ObjectiveComponent))
	{
		UE_LOG(ObjectiveOrientedAI, Warning, TEXT("UAIObjective::Initialize: Attempt to initialize objective (%s) with invalid component."), *GetName());
		return false;
	}
	
	if (IsInitialized() && OwningComponent != ObjectiveComponent)
	{
		UE_LOG(ObjectiveOrientedAI, Warning, TEXT("Attempt to initialize objective (%s) initialized with different component (Objective component: %s, current component: %s)."),
				*GetName(), *GetOwningComponent()->GetName(), *ObjectiveComponent->GetName());
		return false;
	}

	if (!IsInitialized())
	{
		Rename(nullptr, ObjectiveComponent);
		OwningComponent = ObjectiveComponent;
		OnInitialized();
	}
	
	return true;
}

void UAIObjective::Start()
{
	if (IsPendingInvalidation())
	{
		return;
	}
	
	bool bRestarting = false;
	if (IsRunning())
	{
		if (!bAllowRestarting)
		{
			return;
		}

		bRestarting = true;

		UE_LOG(ObjectiveOrientedAI, Display, TEXT("Restarting objective (%s)"), *GetName());
		
		OnStopped(EAIObjectiveStopReason::Restarting);
	}

	const bool bWasQueued = IsQueued() && WasEverStarted();
	
	State = EAIObjectiveState::Running;
	
	bWasEverStarted = true;

	if (!bRestarting)
	{
		UE_LOG(ObjectiveOrientedAI, Display, TEXT("%s is now running."), *GetName());
	}

	ObjectiveContainer.SetQueued(false);
	
	OnStarted(bWasQueued);
}

void UAIObjective::Enqueue()
{
	if (IsPendingInvalidation())
	{
		return;
	}
	
	if (IsQueued())
	{
		return;
	}
	
	State = EAIObjectiveState::Queued;
	
	UE_LOG(ObjectiveOrientedAI, Display, TEXT("%s was queued."), *GetName());

	ObjectiveContainer.SetQueued(true);
	
	if (!WasEverStarted())
	{
		return;
	}
	
	OnStopped(EAIObjectiveStopReason::Queued);
	
	OnObjectiveStoppedEvent.Broadcast(this, EAIObjectiveStopReason::Queued);
}

void UAIObjective::Destroy(const FStopReasonType& StopReason)
{
	if (IsPendingInvalidation())
	{
		return;
	}

	State = EAIObjectiveState::PendingInvalidation;
	
	if (WasEverStarted())
	{
		ObjectiveContainer.ClearObjectives();
		OnStopped(StopReason);
		OnInvalidated(StopReason.IsSucceeded());
		OnObjectiveStoppedEvent.Broadcast(this, StopReason);
		UE_LOG(ObjectiveOrientedAI, Display, TEXT("%s was invalidated."), *GetName());
	}

	MarkAsGarbage();
}

UAIObjective* UAIObjective::BPAddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy)
{
	return AddSubObjective(InAIObjective, bMakeCopy);
}

UAIObjective* UAIObjective::AddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy)
{
	if (CanAddSubObjective(InAIObjective))
	{
		return ObjectiveContainer.AddObjective(GetOwningComponent(), InAIObjective, bMakeCopy);
	}
	return nullptr;
}

void UAIObjective::ClearSubObjectives()
{
	ObjectiveContainer.ClearObjectives();
}
