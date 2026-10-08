
#include "AIObjective/AIObjectiveDefinition.h"

bool UAIObjectiveDefinition::ShouldDefinitionBeCancelled(const FGameplayTagContainer& InDefinitionTags) const
{
	return InDefinitionTags.HasAny(CancelDefinitionsWithTags);
}

bool UAIObjectiveDefinition::ShouldDefinitionBeBlocked(const FGameplayTagContainer& InDefinitionTags) const
{
	return InDefinitionTags.HasAny(BlockDefinitionsWithTags);
}

bool UAIObjectiveDefinition::ShouldDefinitionBeQueued(const FGameplayTagContainer& InDefinitionTags) const
{
	return InDefinitionTags.HasAny(EnqueueDefinitionsWithTags);
}

bool UAIObjectiveDefinition::ShouldDefinitionBeCancelled(const UAIObjectiveDefinition* Other) const
{
	return IsValid(Other) && ShouldDefinitionBeCancelled(Other->DefinitionTags);
}

bool UAIObjectiveDefinition::ShouldDefinitionBeBlocked(const UAIObjectiveDefinition* Other) const
{
	return IsValid(Other) && ShouldDefinitionBeBlocked(Other->DefinitionTags);
}

bool UAIObjectiveDefinition::ShouldDefinitionBeQueued(const UAIObjectiveDefinition* Other) const
{
	return IsValid(Other) && ShouldDefinitionBeQueued(Other->DefinitionTags);
}

bool UAIObjectiveDefinition::CanRunAlongside(const UAIObjectiveDefinition* Other) const
{
	if (!IsValid(Other))
	{
		return true;
	}

	// Both directions, since either one holding the other back is enough to stop them running together
	return !ShouldDefinitionBeBlocked(Other)
		&& !ShouldDefinitionBeQueued(Other)
		&& !Other->ShouldDefinitionBeBlocked(this)
		&& !Other->ShouldDefinitionBeQueued(this);
}

bool UAIObjectiveDefinition::HasAnyTags(const FGameplayTagContainer& InTags) const
{
	return DefinitionTags.HasAny(InTags);
}

bool UAIObjectiveDefinition::MatchesQuery(const FGameplayTagQuery& Query) const
{
	return DefinitionTags.MatchesQuery(Query);
}
