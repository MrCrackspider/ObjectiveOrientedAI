
#pragma once

#include "CoreMinimal.h"

#include "OOAIBlueprintHelperLibrary.generated.h"

class UObjectiveComponent;
class UAIObjective;

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
	
	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target", WorldContext = "Target"))
	static UAIObjective* CreateObjectiveInstance(UObjectiveComponent* Target, TSubclassOf<UAIObjective> Class);
	
	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target", WorldContext = "Target"))
	static UAIObjective* CreateSubObjectiveInstance(UAIObjective* Target, TSubclassOf<UAIObjective> Class);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target", WorldContext = "Target"))
	static void AddObjective(UObjectiveComponent* Target, UAIObjective* TargetObjective);

	UFUNCTION(BlueprintCallable, Category = "OOAI", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target", WorldContext = "Target"))
	static void AddSubObjective(UAIObjective* Target, UAIObjective* TargetObjective);
	
};