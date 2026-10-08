
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ObjectiveOrientedAISettings.generated.h"

/**
 * Project wide settings for the Objective Oriented AI plugin
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Objective Oriented AI"))
class OBJECTIVEORIENTEDAI_API UObjectiveOrientedAISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UObjectiveOrientedAISettings& Get() { return *GetDefault<UObjectiveOrientedAISettings>(); }

	//~ Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	//~ End UDeveloperSettings

#if WITH_EDITORONLY_DATA

	const FGameplayTagContainer& GetDefinitionTagRoots() const { return DefinitionTagRoots; }
	const FGameplayTagContainer& GetObjectiveTagRoots() const { return ObjectiveTagRoots; }

	/**
	 * The roots as the comma separated filter string a gameplay tag picker expects. Empty when no roots
	 * are set, which the picker reads as no filter at all
	 */
	FString GetDefinitionTagRootsFilter() const;
	FString GetObjectiveTagRootsFilter() const;

#endif // WITH_EDITORONLY_DATA

protected:

#if WITH_EDITORONLY_DATA

	/**
	 * Tag pickers on an AI Objective Definition only offer tags under these roots. Leaving this empty
	 * places no limit on them
	 *
	 * This filters what the editor offers, it does not validate what a definition already holds: tags
	 * picked before a root was removed stay where they are
	 */
	UPROPERTY(config, EditAnywhere, Category = "AI Objective Definition")
	FGameplayTagContainer DefinitionTagRoots;

	/**
	 * The tag picker for an objective's own tags only offers tags under these roots. Leaving this empty
	 * places no limit on it
	 */
	UPROPERTY(config, EditAnywhere, Category = "AI Objective")
	FGameplayTagContainer ObjectiveTagRoots;

#endif // WITH_EDITORONLY_DATA
};
