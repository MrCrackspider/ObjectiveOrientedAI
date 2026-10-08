
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AIObjectiveDefinition.generated.h"

/**
 * The tag rules an objective plays by, authored as an asset: what the objective is, and what it does to
 * the objectives it meets
 *
 * Every rule reads the same way round. The definition the question is put to is the one whose rules
 * apply, and whatever is passed in is what they apply to, so asking a rule the other way round is simply
 * asking the other definition instead
 */
UCLASS(BlueprintType, DisplayName = "AI Objective Definition")
class OBJECTIVEORIENTEDAI_API UAIObjectiveDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	const FGameplayTagContainer& GetDefinitionTags() const { return DefinitionTags; }
	const FGameplayTagContainer& GetCancelDefinitionsWithTags() const { return CancelDefinitionsWithTags; }
	const FGameplayTagContainer& GetBlockDefinitionsWithTags() const { return BlockDefinitionsWithTags; }
	const FGameplayTagContainer& GetEnqueueDefinitionsWithTags() const { return EnqueueDefinitionsWithTags; }

	/** Whether a definition carrying InDefinitionTags is cancelled outright the moment this one is added */
	bool ShouldDefinitionBeCancelled(const FGameplayTagContainer& InDefinitionTags) const;

	/**
	 * Whether a definition carrying InDefinitionTags is refused while this one runs. Being blocked keeps
	 * an objective out of the container altogether, unlike being queued
	 */
	bool ShouldDefinitionBeBlocked(const FGameplayTagContainer& InDefinitionTags) const;

	/**
	 * Whether a definition carrying InDefinitionTags is held back while this one runs, to pick up again
	 * once this one is out of the way
	 */
	bool ShouldDefinitionBeQueued(const FGameplayTagContainer& InDefinitionTags) const;

	/** The same three rules read against another definition. A null definition is never affected */
	bool ShouldDefinitionBeCancelled(const UAIObjectiveDefinition* Other) const;
	bool ShouldDefinitionBeBlocked(const UAIObjectiveDefinition* Other) const;
	bool ShouldDefinitionBeQueued(const UAIObjectiveDefinition* Other) const;

	/**
	 * Whether two objectives described this way could run side by side, meaning neither one blocks or
	 * queues the other. Cancelling plays no part: a canceled objective is not held back but taken away,
	 * which is not a standoff between the two
	 *
	 * Asked of a definition against itself this answers for two separate objectives which happen to share
	 * it, which is why a definition whose own tags match its own enqueue rule cannot run alongside itself
	 */
	bool CanRunAlongside(const UAIObjectiveDefinition* Other) const;

	/** Whether this definition carries any of InTags */
	bool HasAnyTags(const FGameplayTagContainer& InTags) const;

	/** Whether this definition matches Query */
	bool MatchesQuery(const FGameplayTagQuery& Query) const;

#if WITH_EDITORONLY_DATA
	const FText& GetDescription() const { return Description; }
#endif

protected:

#if WITH_EDITORONLY_DATA

	/**
	 * What this definition is for.
	 */
	UPROPERTY(EditAnywhere, Category = "AI Objective Definition", meta = (MultiLine = true))
	FText Description;

#endif // WITH_EDITORONLY_DATA

	// Tags this definition carries
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Definition")
	FGameplayTagContainer DefinitionTags;

	// All definitions containing any of these tags are cancelled when this one is added
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Definition")
	FGameplayTagContainer CancelDefinitionsWithTags;

	// All definitions containing any of these tags are blocked while this one is running
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Definition")
	FGameplayTagContainer BlockDefinitionsWithTags;

	// All definitions containing any of these tags are enqueued while this one is running
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Objective Definition")
	FGameplayTagContainer EnqueueDefinitionsWithTags;
};
