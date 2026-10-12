
#include "ObjectiveComponent.h"

#include "GameplayTasksComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

namespace ObjectiveComponentUtils
{
	/**
	 * Adds a component of this class to the controller, or nothing at all when it already has one. The new
	 * component is left unnamed so that it can never collide with a subobject the controller declared itself
	 * @return The component which was added, or null when the controller already had one
	 */
	template <typename ComponentType>
	ComponentType* AddComponentIfMissing(AController& Controller)
	{
		if (Controller.FindComponentByClass<ComponentType>())
		{
			return nullptr;
		}

		ComponentType* NewComponent = NewObject<ComponentType>(&Controller);
		NewComponent->RegisterComponent();

		return NewComponent;
	}
}

void UObjectiveComponent::OnRegister()
{
	Super::OnRegister();
	
	OwningController = Cast<AController>(GetOwner());

	if (OwningController)
	{
		OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);
		AddRequestedControllerComponents();
	}
}

void UObjectiveComponent::OnUnregister()
{
	if (OwningController)
	{
		OwningController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChanged);
	}
	Super::OnUnregister();
}

void UObjectiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearObjectives();
	Super::EndPlay(EndPlayReason);
}

void UObjectiveComponent::AddRequestedControllerComponents()
{
	// Only worth adding to a controller which is about to run. Doing it in the editor would leave the
	// components behind as part of the actor rather than as something built for one run of the game
	const UWorld* World = GetWorld();

	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (bCreatePathFollowingComponent)
	{
		if (UPathFollowingComponent* PathFollowing = ObjectiveComponentUtils::AddComponentIfMissing<UPathFollowingComponent>(*OwningController))
		{
			PathFollowing->Initialize();
		}
	}

	if (bCreateGameplayTasksComponent)
	{
		ObjectiveComponentUtils::AddComponentIfMissing<UGameplayTasksComponent>(*OwningController);
	}
}

APawn* UObjectiveComponent::GetPawn() const
{
	if (OutgoingPawn)
	{
		return OutgoingPawn;
	}

	return OwningController ? OwningController->GetPawn() : nullptr;
}

void UObjectiveComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (OldPawn)
	{
		// The controller unpossesses before it broadcasts, so without this the objectives about to be
		// stopped would ask for their pawn and be told there is none.
		const TGuardValue<TObjectPtr<APawn>> PawnGuard(OutgoingPawn, OldPawn);

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

FObjectiveActivationResult UObjectiveComponent::AddObjective(UAIObjective* InAIObjective, bool bMakeCopy, FObjectiveStoppedDelegate OnStopped)
{
	return ObjectiveContainer.AddObjective(this, InAIObjective, bMakeCopy, MoveTemp(OnStopped));
}

FObjectiveActivationResult UObjectiveComponent::AddObjective(TSubclassOf<UAIObjective> ObjectiveClass, FObjectiveStoppedDelegate OnStopped)
{
	return ObjectiveContainer.AddObjective(this, ObjectiveClass, MoveTemp(OnStopped));
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
	return AddObjective(InAIObjective, bMakeCopy).GetObjective();
}

FObjectiveActivationResult UObjectiveComponent::AddObjective(TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer, FObjectiveStoppedDelegate OnStopped)
{
	return ObjectiveContainer.AddObjective(this, AIObjectiveClass, Initializer, MoveTemp(OnStopped));
}

void UObjectiveComponent::ClearObjectives()
{
	ObjectiveContainer.ClearObjectives();
}

#if WITH_GAMEPLAY_DEBUGGER_MENU
FDebugInfo UObjectiveComponent::GetDebugInfo() const
{
	// Both named at the root rather than left to be inferred: the component runs on any kind of controller
	// now, and an objective which turns out to be inadmissible is usually inadmissible because of one of
	// these two. The pawn is asked of the component, so it reads the outgoing one during a swap
	return FDebugInfo(ObjectiveContainer)
		.SetOwnerName(GetName())
		.AddDetail(TEXT("Controller"), GetNameSafe(OwningController))
		.AddDetail(TEXT("Pawn"), GetNameSafe(GetPawn()));
}
#endif
