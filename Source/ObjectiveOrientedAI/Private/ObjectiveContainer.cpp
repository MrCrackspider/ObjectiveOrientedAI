
#include "ObjectiveContainer.h"
#include "VisualLogger/VisualLogger.h"
#include "ObjectiveComponent.h"

DEFINE_LOG_CATEGORY(ObjectiveOrientedAI);

void FDebugInfo::Clear()
{
	OwnerName.Reset();
	CurrentObjectivesInfo.Empty();
	StackedObjectivesInfo.Empty();
}

FDebugInfo& FDebugInfo::SetOwnerName(FString&& InName)
{
	OwnerName = InName;
	return *this;
}

FDebugInfo::FDebugInfo(const FDebugInfo& Other)
{
	OwnerName = Other.OwnerName;
	CurrentObjectivesLine = Other.CurrentObjectivesLine;
	CurrentObjectivesInfo = Other.CurrentObjectivesInfo;
	StackedObjectivesInfo = Other.StackedObjectivesInfo;
}

FDebugInfo& FDebugInfo::operator=(FDebugInfo& Other)
{
	OwnerName = Other.OwnerName;
	CurrentObjectivesLine = Other.CurrentObjectivesLine;
	CurrentObjectivesInfo = Other.CurrentObjectivesInfo;
	StackedObjectivesInfo = Other.StackedObjectivesInfo;
	return *this;
}

FDebugInfo::FDebugInfo(const FObjectiveContainer& InObjectiveContainer)
{
	if (InObjectiveContainer.IsQueued())
	{
		CurrentObjectivesLine += " [Pending]";
	}
	
	CurrentObjectivesInfo.Reserve(InObjectiveContainer.GetCurrentObjectives().Num());
	
	for (const UAIObjective* Objective : InObjectiveContainer.GetCurrentObjectives())
	{
		CurrentObjectivesInfo.Add(Objective->GetDebugInfo());
	}

	StackedObjectivesInfo.Reserve(InObjectiveContainer.GetQueuedObjectives().Num());
	
	for (const UAIObjective* Objective : InObjectiveContainer.GetQueuedObjectives())
	{
		StackedObjectivesInfo.Add(Objective->GetDebugInfo());
		if (Objective == InObjectiveContainer.GetQueuedObjectives()[0])
		{
			StackedObjectivesInfo[0].SetOwnerName(Objective->GetName() + " [Next]");
		}
	}
}

FDebugInfo& FDebugInfo::Tabulate()
{
	OwnerName = "\t" + OwnerName;
	
	CurrentObjectivesLine = "\t" + CurrentObjectivesLine;
	
	for (FDebugInfo& DebugInfo : CurrentObjectivesInfo)
	{
		DebugInfo.Tabulate();
	}
	
	StackedObjectivesLine = "\t" + StackedObjectivesLine;
	
	for (FDebugInfo& DebugInfo : StackedObjectivesInfo)
	{
		DebugInfo.Tabulate();
	}

	return *this;
}

FString FDebugInfo::ToString()
{
	FString OutString = OwnerName;
	if (!CurrentObjectivesInfo.IsEmpty())
	{
		OutString += "\n";
		OutString += CurrentObjectivesLine;
		for (FDebugInfo& Info : CurrentObjectivesInfo)
		{
			OutString.Append("\n" + Info.Tabulate().ToString());
		}
	}
	
	if (!StackedObjectivesInfo.IsEmpty())
	{
		OutString += "\n";
		OutString += StackedObjectivesLine;
		for (FDebugInfo& Info : StackedObjectivesInfo)
		{
			OutString.Append("\n" + Info.Tabulate().ToString());
		}
	}

	return OutString;
}

FDebugInfo& FDebugInfo::operator=(FDebugInfo&& Other)
{
	OwnerName = MoveTemp(Other.OwnerName);
	CurrentObjectivesLine = MoveTemp(Other.CurrentObjectivesLine);
	CurrentObjectivesInfo = MoveTemp(Other.CurrentObjectivesInfo);
	StackedObjectivesInfo = MoveTemp(Other.StackedObjectivesInfo);
	return *this;
}

UAIObjective* FObjectiveContainer::TryCreateAIObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> Class) const
{
	if (!IsValid(Class) || !IsValid(ObjectiveComponent))
	{
		return nullptr;
	}
	if (ShouldObjectiveBeBlocked(Class.GetDefaultObject()))
	{
		return nullptr;
	}
	return NewObject<UAIObjective>(ObjectiveComponent, Class);
}

bool FObjectiveContainer::CanAddObjective(const UObjectiveComponent* ObjectiveComponent, const UAIObjective* AIObjective, bool* OutBlocked) const
{
	if (!IsValid(AIObjective))
	{
		return false;
	}

	if (bCleaningDown)
	{
		return false;
	}
	
	if (!ObjectiveComponent->GetPawn())
	{
		return false;
	}

	if (ShouldObjectiveBeBlocked(AIObjective))
	{
		if (OutBlocked)
		{
			*OutBlocked = true;
		}
		return false;
	}
	
	return true;
}

UAIObjective* FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* AIObjective, bool bMakeCopy)
{
	if (!CanAddObjective(ObjectiveComponent, AIObjective))
	{
		return nullptr;
	}

	if (AIObjective->HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(ObjectiveOrientedAI, Display, TEXT("Attempt to add Class Default Object (%s) to (%s)"), *AIObjective->GetName(), *ObjectiveComponent->GetName());
		return nullptr;
	}
	
	if (!AIObjective->CanActivate(ObjectiveComponent))
	{
		return nullptr;
	}
	
	if (!AIObjective->IsInitialized())
	{
		UAIObjective* ObjectiveToRun = AIObjective;
		if (bMakeCopy)
		{
			ObjectiveToRun = DuplicateObject(AIObjective, ObjectiveComponent);
		}
		ObjectiveToRun->Initialize(ObjectiveComponent);
		return ActivateObjective(ObjectiveToRun);
	}
	else
	{
		if (bMakeCopy)
		{
			UE_LOG(ObjectiveOrientedAI, Display, TEXT("Making copy from an initialized objective (%s) currently not allowed."), *AIObjective->GetName());
			return nullptr;
		}
	}
	
	if (AIObjective->IsPendingInvalidation())
	{
		return nullptr;
	}
	
	if (!AIObjective->Initialize(ObjectiveComponent))
	{
		return nullptr;
	}

	// if objective is queued do nothing
	if (AIObjective->IsQueued())
	{
		return AIObjective;
	}

	// if objective is running then try to restart it
	if (AIObjective->IsRunning())
	{
		AIObjective->Start();
		if (!IsValid(AIObjective))
		{
			return nullptr;
		}
		return AIObjective;
	}
	
	return ActivateObjective(AIObjective);
}

UAIObjective* FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass)
{
	if (!CanAddObjective(ObjectiveComponent, AIObjectiveClass.GetDefaultObject()))
    {
    	return nullptr;
    }
    
    if (!AIObjectiveClass.GetDefaultObject()->CanActivate(ObjectiveComponent))
    {
    	return nullptr;
    }
    
    UAIObjective* NewObjective = NewObject<UAIObjective>(ObjectiveComponent, AIObjectiveClass);
    NewObjective->Initialize(ObjectiveComponent);
    return ActivateObjective(NewObjective);
}

UAIObjective* FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer)
{
	if (!CanAddObjective(ObjectiveComponent, AIObjectiveClass.GetDefaultObject()))
	{
		return nullptr;
	}
	UAIObjective* NewObjective = NewObject<UAIObjective>(ObjectiveComponent, AIObjectiveClass);
	NewObjective->Initialize(ObjectiveComponent);
	Initializer(NewObjective);
	if (!IsValid(NewObjective))
	{
		return nullptr;
	}
	return ActivateObjective(NewObjective);
}

void FObjectiveContainer::InvalidateObjective(UAIObjective* AIObjective, bool bSuccess)
{
	CurrentObjectives.RemoveSingleSwap(AIObjective);
	QueuedObjectives.RemoveSingle(AIObjective);
	const UObject* LogOwner = AIObjective->GetControlledPawn();
	UE_VLOG(LogOwner, ObjectiveOrientedAI, Log, TEXT("Invalidating objective %s"), *AIObjective->GetName());
	AIObjective->Destroy(bSuccess ? EAIObjectiveStopReason::Succeeded : EAIObjectiveStopReason::Failed);
	ReevaluateQueue(LogOwner);
}

void FObjectiveContainer::ClearObjectives(const FGameplayTagQuery& Query)
{
	const UObject* LogOwner = CurrentObjectives.IsEmpty() ? CurrentObjectives[0]->GetControlledPawn() : nullptr;
	DestroyObjectives(ObtainObjectivesByTagQuery(Query), false);
	ReevaluateQueue(LogOwner);
}

void FObjectiveContainer::ClearObjectives(const FGameplayTagContainer& Tags)
{
	const UObject* LogOwner = CurrentObjectives.IsEmpty() ? CurrentObjectives[0]->GetControlledPawn() : nullptr;
	DestroyObjectives(ObtainObjectivesByTag(Tags));
	ReevaluateQueue(LogOwner);
}

TArray<UAIObjective*> FObjectiveContainer::GetObjectivesByTag(const FGameplayTagContainer& InTags) const
{
	TArray<UAIObjective*> OutObjectives = GetCurrentObjectivesByTag(InTags);
	OutObjectives.Append(GetQueuedObjectivesByTag(InTags));
	return OutObjectives;
}

TArray<UAIObjective*> FObjectiveContainer::GetObjectivesByTagQuery(const FGameplayTagQuery& Query) const
{
	TArray<UAIObjective*> OutObjectives;
	for (TObjectPtr<UAIObjective> Objective : CurrentObjectives)
	{
		if (Objective->ObjectiveTags.MatchesQuery(Query))
		{
			OutObjectives.Add(Objective);
		}
	}
	for (TObjectPtr<UAIObjective> Objective : QueuedObjectives)
	{
		if (Objective->ObjectiveTags.MatchesQuery(Query))
		{
			OutObjectives.Add(Objective);
		}
	}
	return OutObjectives;
}

void FObjectiveContainer::ClearObjectives()
{
	if (bCleaningDown)
	{
		return;
	}
	TGuardValue<bool> Guard(bCleaningDown, true);
	TArray<TObjectPtr<UAIObjective>> Objectives = MoveTemp(CurrentObjectives);
	Objectives.Append(MoveTemp(QueuedObjectives));
	DestroyObjectives(Objectives, false);
}

void FObjectiveContainer::SetQueued(bool bNewQueued)
{
	if (bQueued == bNewQueued)
	{
		return;
	}
	bQueued = bNewQueued;

	TArray<TObjectPtr<UAIObjective>> CurObjectives = GetCurrentObjectives();
	
	for (TObjectPtr<UAIObjective> Objective : CurObjectives)
	{
		if (bQueued)
		{
			Objective->Enqueue();
		}
		else
		{
			Objective->Start();
		}
	}
}

TArray<UAIObjective*> FObjectiveContainer::GetCurrentObjectivesByTag(const FGameplayTagContainer& InTags) const
{
	return GetObjectivesByTag(CurrentObjectives, InTags);
}

TArray<UAIObjective*> FObjectiveContainer::GetQueuedObjectivesByTag(const FGameplayTagContainer& InTags) const
{
	return GetObjectivesByTag(QueuedObjectives, InTags);
}

TArray<UAIObjective*> FObjectiveContainer::GetObjectivesByTag(const TArray<UAIObjective*>& AIObjectives, const FGameplayTagContainer& InTags)
{
	return AIObjectives.FilterByPredicate([&InTags](const TObjectPtr<UAIObjective> Item){ return Item->ObjectiveTags.HasAny(InTags); });
}

bool FObjectiveContainer::ShouldObjectiveBeBlocked(TNotNull<const UAIObjective*> InObjective) const
{
	const FGameplayTagContainer& ObjectiveTags = InObjective->ObjectiveTags;
	for (const UAIObjective* Objective : CurrentObjectives)
	{
		if (ObjectiveTags.HasAny(Objective->BlockObjectivesWithTags))
		{
			return true;
		}
	}
	return false;
}

bool FObjectiveContainer::ShouldObjectiveBeQueued(TNotNull<const UAIObjective*> InObjective) const
{
	const FGameplayTagContainer& ObjectiveTags = InObjective->ObjectiveTags;
	for (const UAIObjective* Objective : CurrentObjectives)
	{
		if (ObjectiveTags.HasAny(Objective->EnqueueObjectivesWithTags))
		{
			return true;
		}
	}
	return false;
}

void FObjectiveContainer::ReevaluateQueue(const UObject* LogOwner)
{
	TArray<UAIObjective*> ObjectivesToActivate = ObtainActivatableObjectives();
	TArray<UAIObjective*> PendingInvalidationObjectives;
	
	for (TArray<UAIObjective*>::TIterator It(ObjectivesToActivate); It; ++It)
	{
		UAIObjective* Value = *It;
		if (!Value->CanActivate(Value->GetOwningComponent()))
		{
			PendingInvalidationObjectives.Add(Value);
			It.RemoveCurrentSwap();
		}
	}
	
	FGameplayTagContainer ActivatableObjectiveEnqueuingTags;
	for (TObjectPtr<UAIObjective> Objective : ObjectivesToActivate)
	{
		ActivatableObjectiveEnqueuingTags.AppendTags(Objective->EnqueueObjectivesWithTags);
	}
	
	TArray<UAIObjective*> ObjectivesToQueue = ObtainObjectivesByTag(ActivatableObjectiveEnqueuingTags, CurrentObjectives);

	for (TArray<UAIObjective*>::TIterator It(ObjectivesToQueue); It; ++It)
	{
		UAIObjective* Value = *It;
		if (!Value->bCanBeQueued)
		{
			PendingInvalidationObjectives.Add(Value);
			It.RemoveCurrent();
		}
	}
	
	CurrentObjectives.Append(ObjectivesToActivate);
	
	// Append ObjectivesToEnqueue to the front of QueuedObjectives
	{
		TArray<TObjectPtr<UAIObjective>> QueuedObjectivesTemp = MoveTemp(QueuedObjectives);
		QueuedObjectives = ObjectivesToQueue;
		QueuedObjectives.Append(MoveTemp(QueuedObjectivesTemp));
	}
	
#if ENABLE_VISUAL_LOG
	const FString VisLogMessage = FString::Printf(TEXT(
		"Invalidated Objectives:\n%s\n"
		"Objectives To Queue:\n%s\n"
		"Objectives To Activate:\n%s"),
		*ObjectivesToString(PendingInvalidationObjectives),
		*ObjectivesToString(ObjectivesToQueue),
		*ObjectivesToString(ObjectivesToActivate));

	UE_VLOG(LogOwner, ObjectiveOrientedAI, Log, TEXT("%s"), *VisLogMessage);
#endif
	
	DestroyObjectives(PendingInvalidationObjectives, true);
	EnqueueObjectives(ObjectivesToQueue);
	StartObjectives(ObjectivesToActivate);
}

TArray<UAIObjective*> FObjectiveContainer::ObtainObjectivesByTag(const FGameplayTagContainer& Tags, TArray<TObjectPtr<UAIObjective>>& SourceObjectives, bool bPreserveOrder)
{
	TArray<UAIObjective*> OutObjectives;
	
	auto RemoveAndAdd = [&OutObjectives, &Tags](UAIObjective* Objective)
	{
		if (IsValid(Objective) && !Objective->IsPendingInvalidation() && Objective->ObjectiveTags.HasAny(Tags))
		{
			OutObjectives.Add(Objective);
			return true;
		}
		return false;
	};
	
	if (bPreserveOrder)
	{
		SourceObjectives.RemoveAll(RemoveAndAdd);
	}
	else
	{
		SourceObjectives.RemoveAllSwap(RemoveAndAdd);
	}
	
	return OutObjectives;
}

TArray<UAIObjective*> FObjectiveContainer::ObtainObjectivesByTag(const FGameplayTagContainer& Tags)
{
	TArray<UAIObjective*> OutObjectives = ObtainObjectivesByTag(Tags, CurrentObjectives, false);
	OutObjectives += ObtainObjectivesByTag(Tags, QueuedObjectives, true);
	return OutObjectives;
}

TArray<UAIObjective*> FObjectiveContainer::ObtainObjectivesByTagQuery(const FGameplayTagQuery& Query)
{
	TArray<UAIObjective*> ObjectivesToRemove = GetObjectivesByTagQuery(Query);
	CurrentObjectives.RemoveAllSwap([&ObjectivesToRemove](const TObjectPtr<UAIObjective> Item)
	{
		return ObjectivesToRemove.Contains(Item);
	});
	QueuedObjectives.RemoveAll([&ObjectivesToRemove](const TObjectPtr<UAIObjective> Item)
	{
		return ObjectivesToRemove.Contains(Item);
	});
	return ObjectivesToRemove;
}

TArray<UAIObjective*> FObjectiveContainer::ObtainActivatableObjectives()
{
	FGameplayTagContainer AccumulatedEnqueuingTags;

	// Gather all current tags
	for (UAIObjective* Objective : CurrentObjectives)
	{
		AccumulatedEnqueuingTags.AppendTags(Objective->EnqueueObjectivesWithTags);
	}

	TArray<UAIObjective*> PendingObjectives;

	// Move from the last added objective to gather all objectives which can be activated and update enqueuing tags accordingly
	for (TArray<TObjectPtr<UAIObjective>>::TIterator It(QueuedObjectives); It; ++It)
	{
		UAIObjective* Value = *It;
		if (!Value->ObjectiveTags.HasAny(AccumulatedEnqueuingTags))
		{
			AccumulatedEnqueuingTags.AppendTags(Value->EnqueueObjectivesWithTags);
			PendingObjectives.Add(Value);
			It.RemoveCurrent();
		}
	}

	return PendingObjectives;
}

void FObjectiveContainer::StartObjectives(const TArray<UAIObjective*>& InObjectives)
{
	for (UAIObjective* Objective : InObjectives)
	{
		if (Objective->CanActivate(Objective->GetOwningComponent()))
		{
			Objective->Start();
		}
		else
		{
			Objective->Invalidate(false);
		}
	}
}

void FObjectiveContainer::DestroyObjectives(const TArray<UAIObjective*>& InObjectives, bool bActivationFailure)
{
	for (UAIObjective* Objective : InObjectives)
	{
		Objective->GetObjectiveContainer().OwningContainer = nullptr;
		Objective->Destroy(bActivationFailure ? EAIObjectiveStopReason::Failed : EAIObjectiveStopReason::Cancelled);
	}
}

void FObjectiveContainer::EnqueueObjectives(const TArray<UAIObjective*>& InObjectives)
{
	for (UAIObjective* Objective : InObjectives)
	{
		if (Objective->bCanBeQueued)
		{
			Objective->Enqueue();
		}
		else
		{
			Objective->Invalidate(false);
		}
	}
}

FString FObjectiveContainer::ObjectivesToString(const TArray<UAIObjective*>& Objectives)
{
	FString Result;

	for (const UAIObjective* Objective : Objectives)
	{
		if (!Objective)
		{
			Result += TEXT(" - <Invalid>\n");
			continue;
		}

		Result += FString::Printf(TEXT(" - %s\n"), *Objective->GetName());
	}

	return Result;
}

UAIObjective* FObjectiveContainer::ActivateObjective(UAIObjective* InAIObjective)
{
	InAIObjective->GetObjectiveContainer().OwningContainer = this;
	TArray<UAIObjective*> ObjectivesToCancel = ObtainObjectivesByTag(InAIObjective->CancelObjectivesWithTags);
	
	bool bShouldBeQueued = ShouldObjectiveBeQueued(InAIObjective);
	
	TArray<UAIObjective*> ObjectivesToEnqueue = ObtainObjectivesByTag(InAIObjective->EnqueueObjectivesWithTags, CurrentObjectives, false);
	TArray<UAIObjective*> ObjectivesToActivate = ObtainActivatableObjectives();

	for (TArray<UAIObjective*>::TIterator It(ObjectivesToActivate); It; ++It)
	{
		UAIObjective* Value = *It;
		if (!Value->CanActivate(Value->GetOwningComponent()))
		{
			ObjectivesToCancel.Add(Value);
			It.RemoveCurrentSwap();
			continue;
		}
		if (Value->ObjectiveTags.HasAny(InAIObjective->EnqueueObjectivesWithTags))
		{
			ObjectivesToEnqueue.Add(Value);
			It.RemoveCurrentSwap();
		}
	}
	
	CurrentObjectives.Append(ObjectivesToActivate);
	
	// Append ObjectivesToEnqueue to the front of QueuedObjectives
	{
		TArray<TObjectPtr<UAIObjective>> QueuedObjectivesTemp = MoveTemp(QueuedObjectives);
		QueuedObjectives = ObjectivesToEnqueue;
		QueuedObjectives.Append(MoveTemp(QueuedObjectivesTemp));
	}

	if (bShouldBeQueued)
	{
		QueuedObjectives.Add(InAIObjective);
	}
	else
	{
		CurrentObjectives.Add(InAIObjective);
	}

#if ENABLE_VISUAL_LOG
	const FString VisLogMessage = FString::Printf(TEXT(
		"Adding Objective %s\n"
		"Invalidated Objectives:\n%s\n"
		"Objectives To Queue:\n%s\n"
		"Objectives To Activate:\n%s"),
		*InAIObjective->GetName(),
		*ObjectivesToString(ObjectivesToCancel),
		*ObjectivesToString(ObjectivesToEnqueue),
		*ObjectivesToString(ObjectivesToActivate));

	UE_VLOG(InAIObjective->GetControlledPawn(), ObjectiveOrientedAI, Log, TEXT("%s"), *VisLogMessage);
#endif
	
	if (bQueued || bShouldBeQueued)
	{
		ObjectivesToEnqueue.Add(InAIObjective);
	}
	else
	{
		ObjectivesToActivate.Add(InAIObjective);
	}
	
	DestroyObjectives(ObjectivesToCancel, false);
	EnqueueObjectives(ObjectivesToEnqueue);
	StartObjectives(ObjectivesToActivate);
	
	return IsValid(InAIObjective) ? InAIObjective : nullptr;
}
