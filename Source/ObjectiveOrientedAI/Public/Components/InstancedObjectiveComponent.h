
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InstancedObjectiveComponent.generated.h"

class UAIObjective;

UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class OBJECTIVEORIENTEDAI_API UInstancedObjectiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	UInstancedObjectiveComponent();

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Instanced, Category = "AI Objective", BlueprintReadWrite)
	TObjectPtr<UAIObjective> Objective;
};
