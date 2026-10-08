
#include "OOAIBlueprintHelperLibrary.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "ObjectiveComponent.h"
#include "ObjectiveComponentOwnerInterface.h"

UAIObjective* UOOAIBlueprintHelperLibrary::CreateObjectiveInstance(UObjectiveComponent* ObjectiveOwner, TSubclassOf<UAIObjective> Class)
{
	return IsValid(ObjectiveOwner) ? ObjectiveOwner->TryCreateAIObjective(Class) : nullptr;
}

void UOOAIBlueprintHelperLibrary::AddObjective(UObjectiveComponent* ObjectiveOwner, UAIObjective* TargetObjective)
{
	if (IsValid(ObjectiveOwner)) ObjectiveOwner->AddObjective(TargetObjective);
}

UAIObjective* UOOAIBlueprintHelperLibrary::CreateSubObjectiveInstance(UAIObjective* ObjectiveOwner, TSubclassOf<UAIObjective> Class)
{
	return IsValid(ObjectiveOwner) ? ObjectiveOwner->TryCreateSubObjective(Class) : nullptr;
}

void UOOAIBlueprintHelperLibrary::AddSubObjective(UAIObjective* ObjectiveOwner, UAIObjective* TargetObjective)
{
	if (!IsValid(ObjectiveOwner) || !TargetObjective) return;
	ObjectiveOwner->AddSubObjective(TargetObjective);
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
