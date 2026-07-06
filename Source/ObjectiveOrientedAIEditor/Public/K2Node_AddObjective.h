#pragma once

#include "K2Node_ConstructObjectFromClass.h"

#include "K2Node_AddObjective.generated.h"

class UClass;
class UEdGraph;
class UEdGraphPin;
class UObject;

UCLASS(Abstract)
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddObjectiveBase : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

public:

	//~ Begin UEdGraphNode Interface.
	virtual bool UseWorldContext() const override { return false; }
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.
	
	//~ Begin UK2Node_ConstructObjectFromClass Interface
	virtual void AllocateDefaultPins() override;
	virtual UClass* GetClassPinBaseClass() const override;
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;
	//~ End UK2Node_ConstructObjectFromClass Interface

protected:
	
	void CreateTargetPin();
	UEdGraphPin* GetTargetPin() const;

	virtual FName GetObjectiveInstanceGetterFunctionName() const { return FName{}; }
	virtual FName GetAddObjectiveInstanceFunctionName() const { return FName{}; }
	virtual FName GetTargetPinFriendlyName() const { return FName{}; }
	virtual UClass* GetTargetClass() const { return nullptr; }
	virtual FText GetObjectiveTitleName() const { return {}; }
};

UCLASS()
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddSubObjective : public UK2Node_AddObjectiveBase
{
	GENERATED_BODY()

public:
	
	UK2Node_AddSubObjective();

protected:

	virtual FName GetObjectiveInstanceGetterFunctionName() const override;
	virtual FName GetAddObjectiveInstanceFunctionName() const override;
	virtual FName GetTargetPinFriendlyName() const override;
	virtual UClass* GetTargetClass() const override;
	virtual FText GetObjectiveTitleName() const override;
};

UCLASS()
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddObjective : public UK2Node_AddObjectiveBase
{
	GENERATED_BODY()

public:
	
	UK2Node_AddObjective(const FObjectInitializer& ObjectInitializer);

protected:

	virtual FName GetObjectiveInstanceGetterFunctionName() const override;
	virtual FName GetAddObjectiveInstanceFunctionName() const override;
	virtual FName GetTargetPinFriendlyName() const override;
	virtual UClass* GetTargetClass() const override;
	virtual FText GetObjectiveTitleName() const override;
};
