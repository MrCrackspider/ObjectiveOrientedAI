// © Volga Studios. All rights reserved.


#include "OOAIController.h"

#include "ObjectiveComponent.h"

AOOAIController::AOOAIController(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	ObjectiveComponent = CreateDefaultSubobject<UObjectiveComponent>(TEXT("ObjectiveComponent"));
}
