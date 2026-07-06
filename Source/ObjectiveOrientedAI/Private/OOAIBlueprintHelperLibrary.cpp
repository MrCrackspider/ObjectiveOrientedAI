
#include "OOAIBlueprintHelperLibrary.h"

#include "ObjectiveComponent.h"
#include "ObjectiveComponentOwnerInterface.h"

UAIObjective* UOOAIBlueprintHelperLibrary::CreateObjectiveInstance(UObjectiveComponent* Target, TSubclassOf<UAIObjective> Class)
{
	return IsValid(Target) ? Target->TryCreateAIObjective(Class) : nullptr;
}

void UOOAIBlueprintHelperLibrary::AddObjective(UObjectiveComponent* Target, UAIObjective* TargetObjective)
{
	if (IsValid(Target)) Target->AddObjective(TargetObjective);
}

UAIObjective* UOOAIBlueprintHelperLibrary::CreateSubObjectiveInstance(UAIObjective* Target, TSubclassOf<UAIObjective> Class)
{
	return IsValid(Target) ? Target->TryCreateSubObjective(Class) : nullptr;
}

void UOOAIBlueprintHelperLibrary::AddSubObjective(UAIObjective* Target, UAIObjective* TargetObjective)
{
	if (!IsValid(Target) || !TargetObjective) return;
	Target->AddSubObjective(TargetObjective);
}

UObjectiveComponent* UOOAIBlueprintHelperLibrary::GetObjectiveComponent(AActor* Actor)
{
	AActor* Target = Actor;
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (AController* Controller = Pawn->GetController())
		{
			Target = Controller;
		}
	}

	if (Target == nullptr)
	{
		return nullptr;
	}
	
	if (const IObjectiveComponentOwnerInterface* ObjCompOwner = Cast<IObjectiveComponentOwnerInterface>(Target))
	{
		return ObjCompOwner->GetObjectiveComponent();
	}
	return Target->FindComponentByClass<UObjectiveComponent>();
}

void UOOAIBlueprintHelperLibrary::SwitchOnStatus(UAIObjective* Target, EAIObjectiveStatus& ObjectiveStatus)
{
	if (!IsValid(Target))
	{
		ObjectiveStatus = EAIObjectiveStatus::Invalid;
		return;
	}
	if (Target->IsQueued())
	{
		ObjectiveStatus = EAIObjectiveStatus::Queued;
		return;
	}
	if (Target->IsRunning())
	{
		ObjectiveStatus = EAIObjectiveStatus::Running;
		return;
	}
	ObjectiveStatus = EAIObjectiveStatus::Invalid;
}
