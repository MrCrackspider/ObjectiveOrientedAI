#include "K2Node_AddObjective.h"

#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "ObjectiveComponent.h"
#include "OOAIBlueprintHelperLibrary.h"

struct FK2Node_AddObjectiveHelper
{
	static const FName ObjectiveComponentPinFriendlyName;
	static const FName ParentObjectivePinFriendlyName;
	
	static const FName ObjectiveClassPinName;
	static const FName TargetPinName;
	static const FName TargetObjectivePinName;
};

const FName FK2Node_AddObjectiveHelper::ObjectiveComponentPinFriendlyName(TEXT("ObjectiveComponent"));
const FName FK2Node_AddObjectiveHelper::ParentObjectivePinFriendlyName(TEXT("ParentObjective"));

const FName FK2Node_AddObjectiveHelper::ObjectiveClassPinName(TEXT("Class"));
const FName FK2Node_AddObjectiveHelper::TargetPinName(TEXT("Target"));
const FName FK2Node_AddObjectiveHelper::TargetObjectivePinName(TEXT("TargetObjective"));

#define LOCTEXT_NAMESPACE "K2Node_AddObjective"

// ----- Base -----

void UK2Node_AddObjectiveBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	// Node input / output pins
	UEdGraphPin* ThisNodeExecPin = GetExecPin();
	UEdGraphPin* ThisNodeThenPin = GetThenPin();
	UEdGraphPin* ThisNodeTargetPin = GetTargetPin();
	UEdGraphPin* ThisNodeClassPin = GetClassPin();
	UEdGraphPin* ThisNodeResultPin = GetResultPin();
	
	// Get objective intermediate node
	UK2Node_CallFunction* CallGetObjectiveNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallGetObjectiveNode->FunctionReference.SetExternalMember(GetObjectiveInstanceGetterFunctionName(), UOOAIBlueprintHelperLibrary::StaticClass());
	CallGetObjectiveNode->AllocateDefaultPins();

	UClass* ClassToSpawn = GetClassToSpawn();
	
	// Exec link
	UEdGraphPin* CallGetObjectiveExecPin = CallGetObjectiveNode->GetExecPin();
	CompilerContext.MovePinLinksToIntermediate(*ThisNodeExecPin, *CallGetObjectiveExecPin);

	// Target link
	UEdGraphPin* CallGetObjectiveTargetPin = CallGetObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::TargetPinName);
	CompilerContext.MovePinLinksToIntermediate(*ThisNodeTargetPin, *CallGetObjectiveTargetPin);
	
	// AIObjective Class link
	UEdGraphPin* CallGetObjectiveClassPin = CallGetObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::ObjectiveClassPinName);
	
	UClass* SpawnClass = ThisNodeClassPin ? Cast<UClass>(ThisNodeClassPin->DefaultObject) : nullptr;
	if ( !ThisNodeClassPin || (ThisNodeClassPin->LinkedTo.IsEmpty() && !SpawnClass))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("AddObjectiveNodeMissingClass_Error", "AddObjective node @@ must have a @@ specified.").ToString(), CallGetObjectiveNode, CallGetObjectiveClassPin);
		BreakAllNodeLinks();
		return;
	}
	
	if(ThisNodeClassPin->LinkedTo.Num() > 0)
	{
		CompilerContext.MovePinLinksToIntermediate(*ThisNodeClassPin, *CallGetObjectiveClassPin);
	}
	else
	{
		CallGetObjectiveClassPin->DefaultObject = SpawnClass;
	}

	// Return value link
	UEdGraphPin* CallGetObjectiveResultPin = CallGetObjectiveNode->GetReturnValuePin();
	CallGetObjectiveResultPin->PinType = ThisNodeResultPin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*ThisNodeResultPin, *CallGetObjectiveResultPin);
	
	// Add objective intermediate node
	UK2Node_CallFunction* CallAddObjectiveNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallAddObjectiveNode->FunctionReference.SetExternalMember(GetAddObjectiveInstanceFunctionName(), UOOAIBlueprintHelperLibrary::StaticClass());
	CallAddObjectiveNode->AllocateDefaultPins();
	
	// Target link copy
	UEdGraphPin* CallAddObjectiveTargetPin = CallAddObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::TargetPinName);
	CompilerContext.CopyPinLinksToIntermediate(*CallGetObjectiveTargetPin, *CallAddObjectiveTargetPin);
	
	// Link get result with add TargetObjective input
	UEdGraphPin* CallAddObjectiveTargetObjectivePin = CallAddObjectiveNode->FindPin(FK2Node_AddObjectiveHelper::TargetObjectivePinName);
	CallGetObjectiveResultPin->MakeLinkTo(CallAddObjectiveTargetObjectivePin);
	
	// Then pin link
	UEdGraphPin* CallAddObjectiveThenPin = CallAddObjectiveNode->GetThenPin();
	CompilerContext.MovePinLinksToIntermediate(*ThisNodeThenPin, *CallAddObjectiveThenPin);
	
	UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(CompilerContext, SourceGraph, CallGetObjectiveNode, this, CallGetObjectiveResultPin, ClassToSpawn);
	UEdGraphPin* CallAddObjectiveExecPin = CallAddObjectiveNode->GetExecPin();
	LastThen->MakeLinkTo(CallAddObjectiveExecPin);
	
	BreakAllNodeLinks();
}

FText UK2Node_AddObjectiveBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraphPin* BlueprintPin = GetClassPin();
	if (BlueprintPin == NULL)
	{
		return GetObjectiveTitleName();
	}
	else if (BlueprintPin->LinkedTo.Num() > 0)
	{
		// Blueprint will be determined dynamically, so we don't have the name in this case
		return LOCTEXT("AddObjective", "AddObjective");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ObjectiveName"), FText::FromString(GetNameSafe(BlueprintPin->DefaultObject)));
		Args.Add(TEXT("ObjectiveTitleName"), GetObjectiveTitleName());
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("AddObjective", "{ObjectiveTitleName} {ObjectiveName}"), Args), this);
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
	NodeTooltip = LOCTEXT("NodeTooltip", "AddSubObjective");
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
	return LOCTEXT("AddObjective", "Add SubObjective");
}

// ----- AddObjective -----

UK2Node_AddObjective::UK2Node_AddObjective(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "AddObjective");
}

FText UK2Node_AddObjectiveBase::GetMenuCategory() const
{
	return LOCTEXT("AddObjective", "Objective Component");
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
	return LOCTEXT("AddObjective", "Add Objective");
}

#undef LOCTEXT_NAMESPACE
