
#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayDebuggerCategory_OOAI.h"

#include "ObjectiveComponent.h"
#include "OOAIBlueprintHelperLibrary.h"

FGameplayDebuggerCategory_OOAI::FGameplayDebuggerCategory_OOAI() { }

void FGameplayDebuggerCategory_OOAI::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (UObjectiveComponent* Comp = UOOAIBlueprintHelperLibrary::GetObjectiveComponent(DebugActor))
	{
		DebugInfo = Comp->GetDebugInfo();
	}
	else
	{
		DebugInfo.Clear();
	}
}

void FGameplayDebuggerCategory_OOAI::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Print(DebugInfo.ToString());
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_OOAI::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_OOAI());
}

#endif