
#include "AIObjective/AIO_RunEQSQuery.h"

#include "AIController.h"
#include "ObjectiveComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"

UAIO_RunEQSQuery* UAIO_RunEQSQuery::MakeInstance(UEnvQuery* InQueryTemplate, UObject* InQuerier,
	EEnvQueryRunMode::Type InRunMode, const TMap<FName, float>& InQueryParams)
{
	UAIO_RunEQSQuery* Objective = NewObject<UAIO_RunEQSQuery>();

	Objective->QueryTemplate = InQueryTemplate;
	Objective->Querier = InQuerier;
	Objective->RunMode = InRunMode;
	Objective->QueryParams = InQueryParams;

	return Objective;
}

#if WITH_EDITOR
FText UAIO_RunEQSQuery::GetDebugDescription_Implementation() const
{
	return FText::FromString(QueryTemplate ? QueryTemplate->GetName() : TEXT("No query"));
}
#endif

bool UAIO_RunEQSQuery::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return IsValid(QueryTemplate) && (IsValid(Querier) || IsValid(GetControlledPawn())) && UEnvQueryManager::GetCurrent(ObjectiveComponent->GetWorld()) != nullptr;
}

void UAIO_RunEQSQuery::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued)
	{
		return;
	}

	UEnvQueryManager* EQSManager = UEnvQueryManager::GetCurrent(GetWorld());
	if (!EQSManager)
	{
		Invalidate(false);
		return;
	}

	UObject* QueryOwner = Querier;
	if (!QueryOwner)
	{
		QueryOwner = GetControlledPawn();
	}
	if (!QueryOwner)
	{
		QueryOwner = GetOwningController();
	}

	QueryWrapper = NewObject<UEnvQueryInstanceBlueprintWrapper>(EQSManager);
	QueryWrapper->SetInstigator(this);

	FEnvQueryRequest Request(QueryTemplate, QueryOwner);
	for (const TPair<FName, float>& Param : QueryParams)
	{
		Request.SetFloatParam(Param.Key, Param.Value);
	}

	QueryWrapper->GetOnQueryFinishedEvent().AddDynamic(this, &UAIO_RunEQSQuery::OnQueryFinished);
	QueryWrapper->RunQuery(RunMode, Request);
}

void UAIO_RunEQSQuery::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (StopReason.IsPendingInvalidation() && QueryWrapper)
	{
		QueryWrapper->GetOnQueryFinishedEvent().RemoveDynamic(this, &UAIO_RunEQSQuery::OnQueryFinished);
	}
}

void UAIO_RunEQSQuery::OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* Wrapper, EEnvQueryStatus::Type Status)
{
	Invalidate(Status == EEnvQueryStatus::Success);
}

bool UAIO_RunEQSQuery::GetQueryResultsAsActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	return QueryWrapper && QueryWrapper->GetQueryResultsAsActors(OutActors);
}

bool UAIO_RunEQSQuery::GetQueryResultsAsLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	return QueryWrapper && QueryWrapper->GetQueryResultsAsLocations(OutLocations);
}

AActor* UAIO_RunEQSQuery::GetBestResultActor() const
{
	TArray<AActor*> Actors;
	if (QueryWrapper && QueryWrapper->GetQueryResultsAsActors(Actors) && Actors.Num() > 0)
	{
		return Actors[0];
	}
	return nullptr;
}

FVector UAIO_RunEQSQuery::GetBestResultLocation() const
{
	TArray<FVector> Locations;
	if (QueryWrapper && QueryWrapper->GetQueryResultsAsLocations(Locations) && Locations.Num() > 0)
	{
		return Locations[0];
	}
	return FVector::ZeroVector;
}

float UAIO_RunEQSQuery::GetItemScore(int32 ItemIndex) const
{
	return QueryWrapper ? QueryWrapper->GetItemScore(ItemIndex) : -1.0f;
}
