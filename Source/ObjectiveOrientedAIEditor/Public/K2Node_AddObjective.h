#pragma once

#include "K2Node_ConstructObjectFromClass.h"

#include "K2Node_AddObjective.generated.h"

class UClass;
class UEdGraph;
class UEdGraphPin;
class UObject;

/**
 * What every AddObjective expansion builds before it diverges: the intermediate call which creates the
 * objective instance, reduced to the three pins the rest of an expansion hangs off
 */
struct FObjectiveInstanceExpansion
{
	/** The creating call's target pin, carrying the objective component or parent objective */
	UEdGraphPin* TargetPin = nullptr;

	/** The created objective */
	UEdGraphPin* ResultPin = nullptr;

	/** End of the expose on spawn assignment chain, which whatever runs next continues from */
	UEdGraphPin* LastThen = nullptr;

	/** False when the expansion failed, in which case it has already reported why and broken the links */
	explicit operator bool() const { return ResultPin != nullptr; }
};

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

	/**
	 * Spawns the intermediate call which creates the objective instance, moves this node's exec, target,
	 * class and result links onto it, and lays down the expose on spawn assignments. Every expansion
	 * starts this way and differs only in what it feeds the created objective to
	 *
	 * A failure is already reported and this node's links already broken, so a caller which gets an empty
	 * result only has to return
	 */
	FObjectiveInstanceExpansion ExpandObjectiveInstanceCreation(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

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

// ----- Latent variants -----
UCLASS(Abstract)
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddObjectiveAndWaitBase : public UK2Node_AddObjectiveBase
{
	GENERATED_BODY()

public:

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node_ConstructObjectFromClass Interface
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;
	//~ End UK2Node_ConstructObjectFromClass Interface

protected:
	
	virtual UFunction* GetAsyncFactoryFunction() const { return nullptr; }
	virtual FName GetAsyncTargetParamName() const { return FName{}; }
};

UCLASS()
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddObjectiveAndWait : public UK2Node_AddObjectiveAndWaitBase
{
	GENERATED_BODY()

public:

	UK2Node_AddObjectiveAndWait();

protected:

	virtual FName GetObjectiveInstanceGetterFunctionName() const override;
	virtual FName GetTargetPinFriendlyName() const override;
	virtual UClass* GetTargetClass() const override;
	virtual FText GetObjectiveTitleName() const override;
	virtual UFunction* GetAsyncFactoryFunction() const override;
	virtual FName GetAsyncTargetParamName() const override;
};

UCLASS()
class OBJECTIVEORIENTEDAIEDITOR_API UK2Node_AddSubObjectiveAndWait : public UK2Node_AddObjectiveAndWaitBase
{
	GENERATED_BODY()

public:

	UK2Node_AddSubObjectiveAndWait();

protected:

	virtual FName GetObjectiveInstanceGetterFunctionName() const override;
	virtual FName GetTargetPinFriendlyName() const override;
	virtual UClass* GetTargetClass() const override;
	virtual FText GetObjectiveTitleName() const override;
	virtual UFunction* GetAsyncFactoryFunction() const override;
	virtual FName GetAsyncTargetParamName() const override;
};
