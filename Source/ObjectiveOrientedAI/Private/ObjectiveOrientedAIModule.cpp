#include "Modules/ModuleManager.h"

#include "EngineDefines.h"
#include "Templates/SubclassOf.h"
#include "VisualLogger/VisualLogger.h"
#include "GameFramework/WorldSettings.h"

#if WITH_GAMEPLAY_DEBUGGER_CORE
#include "GameplayDebugger.h"
#endif // WITH_GAMEPLAY_DEBUGGER_CORE

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory_OOAI.h"
#endif // WITH_GAMEPLAY_DEBUGGER

class FObjectiveOrientedAIModule : public IModuleInterface
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FObjectiveOrientedAIModule, ObjectiveOrientedAI);

void FObjectiveOrientedAIModule::StartupModule()
{ 
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("OOAI", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_OOAI::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, 1);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER
}

void FObjectiveOrientedAIModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("OOAI");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_GAMEPLAY_DEBUGGER
}