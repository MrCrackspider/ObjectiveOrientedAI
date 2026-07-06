
#pragma once

#include "CoreMinimal.h"
#include "ObjectiveContainer.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayDebuggerCategory.h"

struct FGameplayDebuggerCategory_OOAI : public FGameplayDebuggerCategory
{
	FGameplayDebuggerCategory_OOAI();
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
	OBJECTIVEORIENTEDAI_API static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	FDebugInfo DebugInfo;
};

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
