
#pragma once

#include "CoreMinimal.h"
#include "AIObjective/AIObjective.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "AIO_RunEQSQuery.generated.h"

class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;

/**
 * Runs an Environment Query (EQS) against the controlled pawn.
 */
UCLASS(DisplayName = "Run EQS Query")
class OBJECTIVEORIENTEDAI_API UAIO_RunEQSQuery : public UAIObjective
{
	GENERATED_BODY()

	virtual bool CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const override;
	virtual void OnStarted_Implementation(bool bWasQueued) override;
	virtual void OnStopped_Implementation(const FStopReasonType& StopReason) override;

public:

	/**
	 * Creates an objective which runs an environment query. The objective comes back unowned and is
	 * only kept alive by the caller, so add it to a container in the same scope
	 * @param InQueryTemplate Query to run
	 * @param InQuerier       Querier used to resolve query contexts. Defaults to the controlled pawn when unset
	 * @param InRunMode       How the query picks results
	 * @param InQueryParams   Named float params forwarded to the query
	 */
	static UAIO_RunEQSQuery* MakeInstance(UEnvQuery* InQueryTemplate, UObject* InQuerier = nullptr,
		EEnvQueryRunMode::Type InRunMode = EEnvQueryRunMode::SingleResult, const TMap<FName, float>& InQueryParams = {});

#if WITH_EDITOR
	virtual FText GetDebugDescription_Implementation() const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Run EQS Query")
	UEnvQueryInstanceBlueprintWrapper* GetQueryWrapper() const { return QueryWrapper; }

	/** Outputs the resulting actors. Only valid if the query's item type is actor-based. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Run EQS Query")
	bool GetQueryResultsAsActors(TArray<AActor*>& OutActors) const;

	/** Outputs the resulting locations (actor locations if the query generated actors). */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Run EQS Query")
	bool GetQueryResultsAsLocations(TArray<FVector>& OutLocations) const;

	/** Convenience: the highest-scoring result actor, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "Run EQS Query")
	AActor* GetBestResultActor() const;

	/** Convenience: the highest-scoring result location, or ZeroVector. */
	UFUNCTION(BlueprintPure, Category = "Run EQS Query")
	FVector GetBestResultLocation() const;

	/** Score of the item at the given index, or -1 if unavailable. */
	UFUNCTION(BlueprintPure, Category = "Run EQS Query")
	float GetItemScore(int32 ItemIndex) const;

protected:

	// Query to run.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run EQS Query", meta = (ExposeOnSpawn))
	TObjectPtr<UEnvQuery> QueryTemplate;

	// Optional querier used to resolve query contexts. Defaults to the controlled pawn when unset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run EQS Query", meta = (ExposeOnSpawn))
	TObjectPtr<UObject> Querier = nullptr;

	// How the query picks results.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run EQS Query", meta = (ExposeOnSpawn))
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode = EEnvQueryRunMode::SingleResult;

	// Named float params forwarded to the query.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run EQS Query", meta = (ExposeOnSpawn))
	TMap<FName, float> QueryParams;

private:

	UFUNCTION()
	void OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* Wrapper, EEnvQueryStatus::Type Status);

	UPROPERTY(Transient)
	TObjectPtr<UEnvQueryInstanceBlueprintWrapper> QueryWrapper;
};
