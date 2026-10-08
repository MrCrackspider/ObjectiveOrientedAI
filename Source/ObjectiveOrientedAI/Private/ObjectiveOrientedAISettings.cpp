
#include "ObjectiveOrientedAISettings.h"

#if WITH_EDITORONLY_DATA

namespace ObjectiveTagRootsFilter
{
	/** The roots as the comma separated filter string a gameplay tag picker expects */
	static FString Make(const FGameplayTagContainer& Roots)
	{
		TArray<FString> RootNames;
		RootNames.Reserve(Roots.Num());

		// Iterating the container yields the tags which were actually picked, not the parents it derives
		// from them, which is exactly the set of roots wanted here
		for (const FGameplayTag& Root : Roots)
		{
			RootNames.Add(Root.ToString());
		}

		return FString::Join(RootNames, TEXT(","));
	}
}

FString UObjectiveOrientedAISettings::GetDefinitionTagRootsFilter() const
{
	return ObjectiveTagRootsFilter::Make(DefinitionTagRoots);
}

FString UObjectiveOrientedAISettings::GetObjectiveTagRootsFilter() const
{
	return ObjectiveTagRootsFilter::Make(ObjectiveTagRoots);
}

#endif // WITH_EDITORONLY_DATA
