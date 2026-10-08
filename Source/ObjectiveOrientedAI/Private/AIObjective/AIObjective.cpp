
#include "AIObjective/AIObjective.h"

#include "AIController.h"
#include "AIObjective/AIObjectiveDefinition.h"
#include "GameplayTasksComponent.h"
#include "ObjectiveComponent.h"
#include "VisualLogger/VisualLogger.h"

AController* UAIObjective::GetOwningController() const
{
	return GetOwningComponent() ? GetOwningComponent()->GetOwningController() : nullptr;
}

AAIController* UAIObjective::GetOwningAIController() const
{
	return GetOwningComponent() ? GetOwningComponent()->GetAIOwner() : nullptr;
}

APawn* UAIObjective::GetControlledPawn() const
{
	// Asked of the component rather than of the controller, because the component is what still knows the
	// pawn while this objective is being stopped for having lost it
	return GetOwningComponent() ? GetOwningComponent()->GetPawn() : nullptr;
}

UWorld* UAIObjective::GetWorld() const
{
	return OwningComponent ? OwningComponent->GetWorld() : nullptr;
}

UGameplayTasksComponent* UAIObjective::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	// An AI controller caches a component of its own, which it puts on the pawn as it possesses it, so that
	// one answers first and the AI case keeps working exactly as it did
	if (const AAIController* AIController = GetOwningAIController())
	{
		if (UGameplayTasksComponent* AITasksComponent = AIController->GetGameplayTasksComponent())
		{
			return AITasksComponent;
		}
	}

	// Every other controller is asked for one directly, which is where the objective component puts the one
	// it is allowed to add
	const AController* Controller = GetOwningController();
	return Controller ? Controller->FindComponentByClass<UGameplayTasksComponent>() : nullptr;
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
	TagContainer = GetAllObjectiveTags();
}

bool UAIObjective::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return ObjectiveTags.HasTag(TagToCheck)
		|| (IsValid(Definition) && Definition->GetDefinitionTags().HasTag(TagToCheck));
}

bool UAIObjective::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	// The two sets have to be answered together rather than one after the other: neither one holding all
	// of the tags on its own says nothing about whether the objective carries them between them
	return GetAllObjectiveTags().HasAll(TagContainer);
}

bool UAIObjective::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return HasAnyObjectiveTags(TagContainer);
}

UAIObjectiveDefinition* UAIObjective::GetDefinition() const
{
	return Definition;
}

UAIObjective* UAIObjective::SetDefinition(UAIObjectiveDefinition* InDefinition)
{
	Definition = InDefinition;

	return this;
}

FGameplayTagContainer UAIObjective::GetAllObjectiveTags() const
{
	FGameplayTagContainer AllTags = ObjectiveTags;

	if (IsValid(Definition))
	{
		AllTags.AppendTags(Definition->GetDefinitionTags());
	}

	return AllTags;
}

bool UAIObjective::HasAnyObjectiveTags(const FGameplayTagContainer& InTags) const
{
	// Answered against each set in turn, since a single match anywhere is enough. That is what keeps the
	// rule checks below off the merged container, which would have to be built for every pair of objectives
	return ObjectiveTags.HasAny(InTags)
		|| (IsValid(Definition) && Definition->HasAnyTags(InTags));
}

bool UAIObjective::MatchesObjectiveTagQuery(const FGameplayTagQuery& Query) const
{
	return GetAllObjectiveTags().MatchesQuery(Query);
}

bool UAIObjective::ShouldCancelObjective(const UAIObjective* InObjective) const
{
	return IsValid(Definition) && IsValid(InObjective)
		&& InObjective->HasAnyObjectiveTags(Definition->GetCancelDefinitionsWithTags());
}

bool UAIObjective::ShouldBlockObjective(const UAIObjective* InObjective) const
{
	return IsValid(Definition) && IsValid(InObjective)
		&& InObjective->HasAnyObjectiveTags(Definition->GetBlockDefinitionsWithTags());
}

bool UAIObjective::ShouldQueueObjective(const UAIObjective* InObjective) const
{
	return IsValid(Definition) && IsValid(InObjective)
		&& InObjective->HasAnyObjectiveTags(Definition->GetEnqueueDefinitionsWithTags());
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
	FString DisplayName = GetName();

#if WITH_EDITOR
	const FText Description = GetDebugDescription();
	if (!Description.IsEmpty())
	{
		DisplayName += TEXT(" - ") + Description.ToString();
	}
#endif

	return FDebugInfo(ObjectiveContainer).SetOwnerName(MoveTemp(DisplayName));
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
	const FNullInitializer NullInitializer;
	return Initialize(ObjectiveComponent, NullInitializer);
}

bool UAIObjective::Initialize(UObjectiveComponent* ObjectiveComponent, TFunctionRef<void(UAIObjective*)> Initializer)
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

	if (IsInitialized())
	{
		return true;
	}

	Rename(nullptr, ObjectiveComponent);
	OwningComponent = ObjectiveComponent;

	// The objective knows its component from here on, so the setup below can already reach the pawn,
	// and OnInitialized runs on an objective which holds every variable it was given
	Initializer(this);

	// Setting an objective up is user code and may well be the last thing it ever does
	if (!IsValid(this))
	{
		return false;
	}

	OnInitialized();

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

#if ENABLE_VISUAL_LOG
		// Measured on the container holding this objective, not on its own: this objective's activity
		// belongs to the level it sits in, while its container is a level further down
		const FObjectiveContainer* HoldingContainer = ObjectiveContainer.GetOwningContainer();

		if (HoldingContainer && HoldingContainer->ShouldVisualLog())
		{
			UE_VLOG(GetOwningComponent(), ObjectiveOrientedAI, Display, TEXT("Restarting objective (%s)"), *GetName());
		}
#endif

		if (bResetSubobjectivesOnRestart)
		{
			ObjectiveContainer.ClearObjectives();
		}
		
		OnStopped(EAIObjectiveStopReason::Restarting);

		if (IsPendingInvalidation())
		{
			return;
		}
	}

	const bool bWasQueued = IsQueued() && WasEverStarted();
	
	State = EAIObjectiveState::Running;
	
	bWasEverStarted = true;

	if (!bRestarting)
	{
		UE_LOG(ObjectiveOrientedAI, Display, TEXT("%s is now running."), *GetName());
	}

	ObjectiveContainer.SetQueued(false);

	// Resuming the sub objectives runs their user code, which is free to invalidate this objective.
	// It has already been through OnStopped and OnInvalidated by then, so there is nothing left to start
	if (IsPendingInvalidation())
	{
		return;
	}

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

	// Pausing the sub objectives runs their user code, which is free to invalidate this objective. That
	// already reported the stop, so reporting a queueing on top of it would be a second stop
	if (!WasEverStarted() || IsPendingInvalidation())
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
	return AddSubObjective(InAIObjective, bMakeCopy).GetObjective();
}

FObjectiveActivationResult UAIObjective::AddSubObjective(UAIObjective* InAIObjective, bool bMakeCopy, FObjectiveStoppedDelegate OnStopped)
{
	if (CanAddSubObjective(InAIObjective))
	{
		return ObjectiveContainer.AddObjective(GetOwningComponent(), InAIObjective, bMakeCopy, MoveTemp(OnStopped));
	}
	return {};
}

void UAIObjective::ClearSubObjectives()
{
	ObjectiveContainer.ClearObjectives();
}
