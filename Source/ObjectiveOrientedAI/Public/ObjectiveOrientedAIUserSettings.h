
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

protected:

	/**
	 * How many levels of the objective tree report their queue activity to the visual log. One is the
	 * objective component's own objectives, each further level adds the sub objectives of the one above,
	 * and zero silences them altogether
	 */
	UPROPERTY(config, EditAnywhere, Category = "Visual Logging", meta = (ClampMin = 0, UIMin = 0, UIMax = 8))
	int32 VisualLogNestingDepth = 1;
};
