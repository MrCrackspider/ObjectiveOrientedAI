#include "K2Node_AddObjective.h"

#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ObjectiveComponent.h"
#include "OOAIBlueprintHelperLibrary.h"
#include "Async/AsyncAction_AddObjectiveAndWaitForResult.h"

struct FK2Node_AddObjectiveHelper
{
	static const FName ObjectiveComponentPinFriendlyName;
	static const FName ParentObjectivePinFriendlyName;
	
	static const FName ObjectiveClassPinName;
	static const FName ObjectiveOwnerPinName;
	static const FName TargetObjectivePinName;

	// Async factory param / dedicated exec pin names
	static const FName AsyncObjectivePinName;
	static const FName CompletedPinName;
	static const FName FailedPinName;
};

const FName FK2Node_AddObjectiveHelper::ObjectiveComponentPinFriendlyName(TEXT("ObjectiveComponent"));
const FName FK2Node_AddObjectiveHelper::ParentObjectivePinFriendlyName(TEXT("ParentObjective"));

const FName FK2Node_AddObjectiveHelper::ObjectiveClassPinName(TEXT("Class"));
const FName FK2Node_AddObjectiveHelper::ObjectiveOwnerPinName(TEXT("ObjectiveOwner"));
const FName FK2Node_AddObjectiveHelper::TargetObjectivePinName(TEXT("TargetObjective"));

const FName FK2Node_AddObjectiveHelper::AsyncObjectivePinName(TEXT("Objective"));
const FName FK2Node_AddObjectiveHelper::CompletedPinName(TEXT("Completed"));
const FName FK2Node_AddObjectiveHelper::FailedPinName(TEXT("Failed"));

#define LOCTEXT_NAMESPACE "K2Node_AddObjective"

// ----- Base -----

FObjectiveInstanceExpansion UK2Node_AddObjectiveBase::ExpandObjectiveInstanceCreation(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// Get objective intermediate node
	UK2Node_CallFunction* CallGetObjectiveNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallGetObjectiveNode->FunctionReference.SetExternalMember(GetObjectiveInstanceGetterFunctionName(), UOOAIBlueprintHelperLibrary::StaticClass());
	CallGetObjectiveNode->AllocateDefaultPins();

	// Exec link
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallGetObjectiveNode->GetExecPin());

	// Target link
	UEdGraphPin* CallGetObjectiveTargetPin = CallGetObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::ObjectiveOwnerPinName);
	CompilerContext.MovePinLinksToIntermediate(*GetTargetPin(), *CallGetObjectiveTargetPin);

	// AIObjective Class link
	UEdGraphPin* CallGetObjectiveClassPin = CallGetObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::ObjectiveClassPinName);
	UEdGraphPin* ThisNodeClassPin = GetClassPin();
	UClass* SpawnClass = ThisNodeClassPin ? Cast<UClass>(ThisNodeClassPin->DefaultObject) : nullptr;

	// Read before the class pin's links are moved below. For a linked pin GetClassToSpawn resolves the
	// class through LinkedTo, so once those links belong to the intermediate node it has nothing left to
	// resolve and answers null - and a null class makes GenerateAssignmentNodes find no properties and
	// silently emit no expose on spawn assignments at all
	const UClass* ClassToSpawn = GetClassToSpawn();

	if (!ThisNodeClassPin || (ThisNodeClassPin->LinkedTo.IsEmpty() && !SpawnClass))
	{
		CompilerContext.MessageLog.Error(
			*FText::Format(LOCTEXT("AddObjectiveMissingClass_Error", "{0} node @@ must have a @@ specified."), GetObjectiveTitleName()).ToString(),
			CallGetObjectiveNode, CallGetObjectiveClassPin);
		BreakAllNodeLinks();
		return {};
	}

	if (!ThisNodeClassPin->LinkedTo.IsEmpty())
	{
		CompilerContext.MovePinLinksToIntermediate(*ThisNodeClassPin, *CallGetObjectiveClassPin);
	}
	else
	{
		CallGetObjectiveClassPin->DefaultObject = SpawnClass;
	}

	// Return value link
	UEdGraphPin* ThisNodeResultPin = GetResultPin();
	UEdGraphPin* CallGetObjectiveResultPin = CallGetObjectiveNode->GetReturnValuePin();
	CallGetObjectiveResultPin->PinType = ThisNodeResultPin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*ThisNodeResultPin, *CallGetObjectiveResultPin);

	FObjectiveInstanceExpansion Expansion;
	Expansion.TargetPin = CallGetObjectiveTargetPin;
	Expansion.ResultPin = CallGetObjectiveResultPin;

	// GenerateAssignmentNodes skips any input pin whose name also exists on the call it assigns from,
	// reading the collision as "this pin belongs to the node, not to the spawned class". An expose on
	// spawn property colliding with one of the creation call's parameters would be left at its default
	// with nothing said about it, so it is reported here instead of failing silently at runtime
	if (ClassToSpawn)
	{
		for (const UEdGraphPin* Pin : Pins)
		{
			if (Pin->Direction != EGPD_Input || !CallGetObjectiveNode->FindPin(Pin->PinName) ||
				!FindFProperty<FProperty>(ClassToSpawn, Pin->PinName))
			{
				continue;
			}

			CompilerContext.MessageLog.Error(
				*FText::Format(LOCTEXT("AddObjectiveReservedPropertyName_Error", "@@ cannot expose a property named '{0}' on spawn: the name is reserved by the node's own expansion. Rename the property."),
					FText::FromName(Pin->PinName)).ToString(),
				this);
			BreakAllNodeLinks();
			return {};
		}
	}

	// Expose on spawn assignments, which run between creating the objective and doing anything with it
	Expansion.LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(
		CompilerContext, SourceGraph, CallGetObjectiveNode, this, CallGetObjectiveResultPin, ClassToSpawn);

	return Expansion;
}

void UK2Node_AddObjectiveBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FObjectiveInstanceExpansion Creation = ExpandObjectiveInstanceCreation(CompilerContext, SourceGraph);
	if (!Creation)
	{
		return;
	}

	// Add objective intermediate node
	UK2Node_CallFunction* CallAddObjectiveNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallAddObjectiveNode->FunctionReference.SetExternalMember(GetAddObjectiveInstanceFunctionName(), UOOAIBlueprintHelperLibrary::StaticClass());
	CallAddObjectiveNode->AllocateDefaultPins();

	// Target link copy, so the add call goes to whatever the creating call was given
	CompilerContext.CopyPinLinksToIntermediate(*Creation.TargetPin, *CallAddObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::ObjectiveOwnerPinName));

	// Link get result with add TargetObjective input
	Creation.ResultPin->MakeLinkTo(CallAddObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::TargetObjectivePinName));

	// Then pin link
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallAddObjectiveNode->GetThenPin());

	// Exec: continue from the assignment chain into the add call
	Creation.LastThen->MakeLinkTo(CallAddObjectiveNode->GetExecPin());

	BreakAllNodeLinks();
}

FText UK2Node_AddObjectiveBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphPin* ClassPin = GetClassPin();
	const UClass* ObjectiveClass = ClassPin ? Cast<UClass>(ClassPin->DefaultObject) : nullptr;
	
	if (!ClassPin || !ClassPin->LinkedTo.IsEmpty() || !ObjectiveClass)
	{
		return GetObjectiveTitleName();
	}

	if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ObjectiveName"), FText::FromString(FBlueprintEditorUtils::GetClassNameWithoutSuffix(ObjectiveClass)));
		Args.Add(TEXT("ObjectiveTitleName"), GetObjectiveTitleName());
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("NamedObjectiveTitle", "{ObjectiveTitleName} {ObjectiveName}"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_AddObjectiveBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	CreateTargetPin();
}

UClass* UK2Node_AddObjectiveBase::GetClassPinBaseClass() const
{
	return UAIObjective::StaticClass();
}

bool UK2Node_AddObjectiveBase::IsSpawnVarPin(UEdGraphPin* Pin) const
{
	UEdGraphPin* ParentPin = Pin->ParentPin;
	while (ParentPin)
	{
		if (ParentPin->PinName == GetTargetPinFriendlyName())
		{
			return false;
		}
		ParentPin = ParentPin->ParentPin;
	}

	return Super::IsSpawnVarPin(Pin) && Pin->PinName != GetTargetPinFriendlyName();
}

void UK2Node_AddObjectiveBase::CreateTargetPin()
{
	UEdGraphPin* Pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, GetTargetClass(), UEdGraphSchema_K2::PN_Self);
	Pin->PinFriendlyName = FText::FromName(GetTargetPinFriendlyName());
	Pins.Remove(Pin);
	Pins.Insert(Pin, 1);
}

UEdGraphPin* UK2Node_AddObjectiveBase::GetTargetPin() const
{
	UEdGraphPin* Pin = FindPinChecked(UEdGraphSchema_K2::PN_Self);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

// ----- AddSubObjective -----

UK2Node_AddSubObjective::UK2Node_AddSubObjective()
{
	NodeTooltip = LOCTEXT("AddSubObjectiveTooltip", "AddSubObjective");
}

FName UK2Node_AddSubObjective::GetObjectiveInstanceGetterFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, CreateSubObjectiveInstance);
}

FName UK2Node_AddSubObjective::GetAddObjectiveInstanceFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, AddSubObjective);
}

FName UK2Node_AddSubObjective::GetTargetPinFriendlyName() const
{
	return FK2Node_AddObjectiveHelper::ParentObjectivePinFriendlyName;
}

UClass* UK2Node_AddSubObjective::GetTargetClass() const
{
	return UAIObjective::StaticClass();
}

FText UK2Node_AddSubObjective::GetObjectiveTitleName() const
{
	return LOCTEXT("AddSubObjectiveTitle", "Add SubObjective");
}

// ----- AddObjective -----

UK2Node_AddObjective::UK2Node_AddObjective(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("AddObjectiveTooltip", "AddObjective");
}

FText UK2Node_AddObjectiveBase::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Objective Component");
}

FName UK2Node_AddObjective::GetObjectiveInstanceGetterFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, CreateObjectiveInstance);
}

FName UK2Node_AddObjective::GetAddObjectiveInstanceFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, AddObjective);
}

FName UK2Node_AddObjective::GetTargetPinFriendlyName() const
{
	return FK2Node_AddObjectiveHelper::ObjectiveComponentPinFriendlyName;
}

UClass* UK2Node_AddObjective::GetTargetClass() const
{
	return UObjectiveComponent::StaticClass();
}

FText UK2Node_AddObjective::GetObjectiveTitleName() const
{
	return LOCTEXT("AddObjectiveTitle", "Add Objective");
}

// ----- AddObjectiveAndWait base -----

void UK2Node_AddObjectiveAndWaitBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FK2Node_AddObjectiveHelper::CompletedPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FK2Node_AddObjectiveHelper::FailedPinName);
}

bool UK2Node_AddObjectiveAndWaitBase::IsSpawnVarPin(UEdGraphPin* Pin) const
{
	if (Pin->PinName == FK2Node_AddObjectiveHelper::CompletedPinName ||
		Pin->PinName == FK2Node_AddObjectiveHelper::FailedPinName)
	{
		return false;
	}

	return Super::IsSpawnVarPin(Pin);
}

void UK2Node_AddObjectiveAndWaitBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// Deliberately skipping UK2Node_AddObjectiveBase, whose expansion ends in a plain add call. This one
	// shares the objective creation below but hands the result to an async node instead
	UK2Node::ExpandNode(CompilerContext, SourceGraph);

	UFunction* AsyncFactoryFunction = GetAsyncFactoryFunction();
	if (!AsyncFactoryFunction)
	{
		CompilerContext.MessageLog.Error(
			*FText::Format(LOCTEXT("AddObjectiveMissingFactory_Error", "{0} node @@ has no async factory function."), GetObjectiveTitleName()).ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	const FObjectiveInstanceExpansion Creation = ExpandObjectiveInstanceCreation(CompilerContext, SourceGraph);
	if (!Creation)
	{
		return;
	}

	// Async wait intermediate node
	UK2Node_AsyncAction* AsyncNode = CompilerContext.SpawnIntermediateNode<UK2Node_AsyncAction>(this, SourceGraph);
	AsyncNode->InitializeProxyFromFunction(AsyncFactoryFunction);
	AsyncNode->AllocateDefaultPins();

	// Exec: continue from the assignment chain into the async node
	Creation.LastThen->MakeLinkTo(AsyncNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));

	// Target link (component / parent objective) - copy the same links the getter received
	CompilerContext.CopyPinLinksToIntermediate(*Creation.TargetPin, *AsyncNode->FindPinChecked(GetAsyncTargetParamName()));

	// Objective link: feed created instance into the async factory
	Creation.ResultPin->MakeLinkTo(AsyncNode->FindPinChecked(FK2Node_AddObjectiveHelper::AsyncObjectivePinName));

	// Output exec pins
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *AsyncNode->FindPinChecked(UEdGraphSchema_K2::PN_Then));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(FK2Node_AddObjectiveHelper::CompletedPinName), *AsyncNode->FindPinChecked(FK2Node_AddObjectiveHelper::CompletedPinName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(FK2Node_AddObjectiveHelper::FailedPinName), *AsyncNode->FindPinChecked(FK2Node_AddObjectiveHelper::FailedPinName));

	BreakAllNodeLinks();
}

// ----- AddObjectiveAndWait -----

UK2Node_AddObjectiveAndWait::UK2Node_AddObjectiveAndWait()
{
	NodeTooltip = LOCTEXT("AddObjectiveAndWaitTooltip", "Adds an objective and waits for it to complete or fail");
}

FName UK2Node_AddObjectiveAndWait::GetObjectiveInstanceGetterFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, CreateObjectiveInstance);
}

FName UK2Node_AddObjectiveAndWait::GetTargetPinFriendlyName() const
{
	return FK2Node_AddObjectiveHelper::ObjectiveComponentPinFriendlyName;
}

UClass* UK2Node_AddObjectiveAndWait::GetTargetClass() const
{
	return UObjectiveComponent::StaticClass();
}

FText UK2Node_AddObjectiveAndWait::GetObjectiveTitleName() const
{
	return LOCTEXT("AddObjectiveAndWait", "Add Objective and Wait");
}

UFunction* UK2Node_AddObjectiveAndWait::GetAsyncFactoryFunction() const
{
	return UAsyncAction_AddObjectiveAndWaitForResult::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAsyncAction_AddObjectiveAndWaitForResult, AddObjectiveAndWaitForResult));
}

FName UK2Node_AddObjectiveAndWait::GetAsyncTargetParamName() const
{
	return FK2Node_AddObjectiveHelper::ObjectiveComponentPinFriendlyName;
}

// ----- AddSubObjectiveAndWait -----

UK2Node_AddSubObjectiveAndWait::UK2Node_AddSubObjectiveAndWait()
{
	NodeTooltip = LOCTEXT("AddSubObjectiveAndWaitTooltip", "Adds a sub-objective and waits for it to complete or fail");
}

FName UK2Node_AddSubObjectiveAndWait::GetObjectiveInstanceGetterFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UOOAIBlueprintHelperLibrary, CreateSubObjectiveInstance);
}

FName UK2Node_AddSubObjectiveAndWait::GetTargetPinFriendlyName() const
{
	return FK2Node_AddObjectiveHelper::ParentObjectivePinFriendlyName;
}

UClass* UK2Node_AddSubObjectiveAndWait::GetTargetClass() const
{
	return UAIObjective::StaticClass();
}

FText UK2Node_AddSubObjectiveAndWait::GetObjectiveTitleName() const
{
	return LOCTEXT("AddSubObjectiveAndWait", "Add SubObjective and Wait");
}

UFunction* UK2Node_AddSubObjectiveAndWait::GetAsyncFactoryFunction() const
{
	return UAsyncAction_AddObjectiveAndWaitForResult::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAsyncAction_AddObjectiveAndWaitForResult, AddSubObjectiveAndWaitForResult));
}

FName UK2Node_AddSubObjectiveAndWait::GetAsyncTargetParamName() const
{
	return FK2Node_AddObjectiveHelper::ParentObjectivePinFriendlyName;
}

#undef LOCTEXT_NAMESPACE
