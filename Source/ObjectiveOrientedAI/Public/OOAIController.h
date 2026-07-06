// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ObjectiveComponentOwnerInterface.h"
#include "OOAIController.generated.h"

class UObjectiveComponent;

UCLASS()
class OBJECTIVEORIENTEDAI_API AOOAIController : public AAIController, public IObjectiveComponentOwnerInterface
{
	GENERATED_BODY()

public:
	AOOAIController(const FObjectInitializer& ObjectInitializer);
	
	//~ Begin IObjectiveComponentOwnerInterface
	virtual UObjectiveComponent* GetObjectiveComponent() const override { return ObjectiveComponent; }
	//~ End IObjectiveComponentOwnerInterface
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UObjectiveComponent> ObjectiveComponent;
};
