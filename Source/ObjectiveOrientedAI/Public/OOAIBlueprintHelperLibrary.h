
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "OOAIBlueprintHelperLibrary.generated.h"

class UObjectiveComponent;
class UAIObjective;

USTRUCT(BlueprintType, meta = (ShowOnlyInnerProperties))
struct FAIObjectiveReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced)
	TObjectPtr<UAIObjective> Objective;
};

USTRUCT(BlueprintType, meta = (ShowOnlyInnerProperties))
struct FAIObjectiveReferenceArray
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced)
	TArray<TObjectPtr<UAIObjective>> Objectives;
};

UENUM(BlueprintType)
enum class EAIObjectiveStatus : uint8
{
	Running,
	Queued,
	Invalid
};

UCLASS()
class OBJECTIVEORIENTEDAI_API UOOAIBlueprintHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintPure, Category = "OOAI", meta = (DefaultToSelf = "Actor"))
	static UObjectiveComponent* GetObjectiveComponent(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (ExpandEnumAsExecs = "ObjectiveStatus"))
	static void SwitchOnStatus(UAIObjective* Target, EAIObjectiveStatus& ObjectiveStatus);
	
	// K2Node_AddObjective utils

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "ObjectiveOwner", WorldContext = "ObjectiveOwner"))
	static UAIObjective* CreateObjectiveInstance(UObjectiveComponent* ObjectiveOwner, TSubclassOf<UAIObjective> Class);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "ObjectiveOwner", WorldContext = "ObjectiveOwner"))
	static UAIObjective* CreateSubObjectiveInstance(UAIObjective* ObjectiveOwner, TSubclassOf<UAIObjective> Class);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "ObjectiveOwner", WorldContext = "ObjectiveOwner"))
	static void AddObjective(UObjectiveComponent* ObjectiveOwner, UAIObjective* TargetObjective);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "ObjectiveOwner", WorldContext = "ObjectiveOwner"))
	static void AddSubObjective(UAIObjective* ObjectiveOwner, UAIObjective* TargetObjective);
	
};