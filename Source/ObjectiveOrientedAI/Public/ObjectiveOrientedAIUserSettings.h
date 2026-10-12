
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ObjectiveOrientedAIUserSettings.generated.h"

/**
 * Per developer settings for the Objective Oriented AI plugin
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Objective Oriented AI"))
class OBJECTIVEORIENTEDAI_API UObjectiveOrientedAIUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UObjectiveOrientedAIUserSettings& Get() { return *GetDefault<UObjectiveOrientedAIUserSettings>(); }

	//~ Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	int32 GetVisualLogNestingDepth() const { return VisualLogNestingDepth; }

	int32 GetGameplayDebuggerNestingDepth() const { return GameplayDebuggerNestingDepth; }

protected:

	/**
	 * How many levels of the objective tree report their queue activity to the visual log. One is the
	 * objective component's own objectives, each further level adds the sub objectives of the one above,
	 * and zero silences them altogether
	 */
	UPROPERTY(config, EditAnywhere, Category = "Visual Logging", meta = (ClampMin = 0, UIMin = 0, UIMax = 8))
	int32 VisualLogNestingDepth = 1;

	/**
	 * How many levels of the objective tree the gameplay debugger draws, counted the same way: one is the
	 * objective component's own objectives, each further level adds the sub objectives of the one above.
	 * An objective whose sub objectives were left out says how many they were
	 *
	 * Lowest setting is one rather than zero, because drawing the objectives is the whole of what the
	 * category does: it is switched off by switching the category off. The default goes deeper than the
	 * visual log's, since a sequence or a composite does its actual work one level down and the debugger
	 * is read on demand rather than written on every queue change
	 */
	UPROPERTY(config, EditAnywhere, Category = "Gameplay Debugger", meta = (ClampMin = 1, UIMin = 1, UIMax = 8))
	int32 GameplayDebuggerNestingDepth = 3;
};
