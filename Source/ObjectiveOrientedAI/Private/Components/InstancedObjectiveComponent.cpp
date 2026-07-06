
#include "Components/InstancedObjectiveComponent.h"

#include "ObjectiveComponent.h"
#include "OOAIBlueprintHelperLibrary.h"

UInstancedObjectiveComponent::UInstancedObjectiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInstancedObjectiveComponent::BeginPlay()
{
	if (bAutoStart)
	{
		if (UObjectiveComponent* ObjectiveComponent = UOOAIBlueprintHelperLibrary::GetObjectiveComponent(GetOwner()))
		{
			ObjectiveComponent->AddObjective(Objective, true);
		}
	}
	Super::BeginPlay();
}
