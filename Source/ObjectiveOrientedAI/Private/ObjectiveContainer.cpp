
#include "ObjectiveContainer.h"
#include "GameFramework/Pawn.h"
#include "VisualLogger/VisualLogger.h"
#include "ObjectiveComponent.h"
#include "ObjectiveOrientedAIUserSettings.h"
#include "Algo/Reverse.h"

DEFINE_LOG_CATEGORY(ObjectiveOrientedAI);

namespace ObjectiveQueue
{
	/**
	 * Starting an objective can add or invalidate more objectives, which asks this container to settle
	 * again from inside the pass which is already running. That nesting is normal and usually one or
	 * two levels deep, so only a runaway - objectives which keep undoing each other - is cut off
	 */
	static constexpr int32 MaxReevaluationDepth = 16;
}

bool FStopReasonType::WasQueued() const
{
	return Reason == EAIObjectiveStopReason::Queued;
}

bool FStopReasonType::IsCompleted() const
{
	return	Reason == EAIObjectiveStopReason::Succeeded ||
			Reason == EAIObjectiveStopReason::Failed ||
			Reason == EAIObjectiveStopReason::Cancelled;
}

bool FStopReasonType::IsSucceeded() const
{
	return Reason == EAIObjectiveStopReason::Succeeded;
}

bool FStopReasonType::IsPendingInvalidation() const
{
	return IsCompleted();
}

bool FStopReasonType::IsRestarting() const
{
	return Reason == EAIObjectiveStopReason::Restarting;
}

#if WITH_GAMEPLAY_DEBUGGER_MENU

namespace ObjectiveDebug
{
	// Indented with spaces: the canvas turns a tab into five of them, which is too coarse for a deep tree
	static constexpr int32 SpacesPerIndentLevel = 2;

	/**
	 * Colours for the debugger canvas, in the markup FGameplayDebuggerCanvasContext::Print reads: an
	 * FColorList name in braces, which holds until the next one, across newlines included - so every line
	 * states its own colour. Print silently drops anything in braces it cannot read as a colour, which is
	 * what FDebugInfo::Sanitize is guarding against
	 */
	namespace Color
	{
		static constexpr const TCHAR* Owner = TEXT("{green}");

		/**
		 * The split the engine's own categories draw with, and the reason none of this is on a grey or a
		 * near neutral: the wording which is on every line in white, and whatever differs per actor - a
		 * name, a value, an objective's own account of itself - in yellow
		 */
		static constexpr const TCHAR* Label = TEXT("{white}");
		static constexpr const TCHAR* Value = TEXT("{yellow}");

		static constexpr const TCHAR* Tags = TEXT("{silver}");
		static constexpr const TCHAR* Definition = TEXT("{skyblue}");
		static constexpr const TCHAR* Next = TEXT("{green}");

		// A group's header and the objectives under it, in one colour per state the group stands for
		static constexpr const TCHAR* RunningHeader = TEXT("{lightblue}");
		static constexpr const TCHAR* Running = TEXT("{white}");
		static constexpr const TCHAR* PausedHeader = TEXT("{orange}");
		static constexpr const TCHAR* Paused = TEXT("{orange}");
		static constexpr const TCHAR* QueuedHeader = TEXT("{red}");
		static constexpr const TCHAR* Queued = TEXT("{red}");
	}
}

bool FObjectiveContainer::ShouldGameplayDebug() const
{
	return GetNestingLevel() < UObjectiveOrientedAIUserSettings::Get().GetGameplayDebuggerNestingDepth();
}

FDebugInfo::FDebugInfo(const FObjectiveContainer& InObjectiveContainer)
{
	bQueued = InObjectiveContainer.IsQueued();

	// Past the depth asked for, the objective owning this container is still drawn - it is only what the
	// container holds which is left out, and counted so that a tree cut short here does not read like an
	// objective which simply has nothing under it
	if (!InObjectiveContainer.ShouldGameplayDebug())
	{
		TruncatedCount = InObjectiveContainer.GetCurrentObjectives().Num() + InObjectiveContainer.GetQueuedObjectives().Num();
		return;
	}

	CurrentObjectives.Reserve(InObjectiveContainer.GetCurrentObjectives().Num());
	for (const UAIObjective* Objective : InObjectiveContainer.GetCurrentObjectives())
	{
		CurrentObjectives.Add(Objective->GetDebugInfo());
	}

	QueuedObjectives.Reserve(InObjectiveContainer.GetQueuedObjectives().Num());
	for (const UAIObjective* Objective : InObjectiveContainer.GetQueuedObjectives())
	{
		FDebugInfo& QueuedInfo = QueuedObjectives.Add_GetRef(Objective->GetDebugInfo());

		// Which objective is holding this one back is the one thing the tree cannot show by shape, and the
		// first thing asked on finding something waiting. Only the running objectives are asked: a queued
		// objective can also be held back by a higher priority one which is waiting itself, but that only
		// happens while the whole container is paused, which the group header already says
		for (const UAIObjective* RunningObjective : InObjectiveContainer.GetCurrentObjectives())
		{
			if (RunningObjective->ShouldQueueObjective(Objective))
			{
				QueuedInfo.HeldBackBy = Sanitize(RunningObjective->GetDebugName());
				break;
			}
		}
	}

	// The head of the queue is whatever runs next
	if (!QueuedObjectives.IsEmpty())
	{
		QueuedObjectives[0].bIsNext = true;
	}
}

FDebugInfo& FDebugInfo::SetOwnerName(FString InName)
{
	OwnerName = Sanitize(MoveTemp(InName));
	return *this;
}

FDebugInfo& FDebugInfo::AddDetail(FString InLabel, FString InValue)
{
	Details.Emplace(Sanitize(MoveTemp(InLabel)), Sanitize(MoveTemp(InValue)));
	return *this;
}

FDebugInfo& FDebugInfo::SetDefinitionName(FString InName)
{
	DefinitionName = Sanitize(MoveTemp(InName));
	return *this;
}

FDebugInfo& FDebugInfo::SetTags(FString InTags)
{
	Tags = Sanitize(MoveTemp(InTags));
	return *this;
}

FDebugInfo& FDebugInfo::SetDescription(FString InDescription)
{
	Description = Sanitize(MoveTemp(InDescription));
	return *this;
}

bool FDebugInfo::IsEmpty() const
{
	return OwnerName.IsEmpty() && Details.IsEmpty() && CurrentObjectives.IsEmpty() && QueuedObjectives.IsEmpty();
}

void FDebugInfo::Clear()
{
	*this = FDebugInfo();
}

FString FDebugInfo::ToString() const
{
	FString Result;
	AppendTo(Result, 0, ObjectiveDebug::Color::Owner, TEXT(""));

	// A component holding nothing would otherwise print its own name and stop, which reads like the
	// snapshot failed rather than like there being nothing to show. Nothing collected is not nothing held,
	// so a depth of zero out of a hand edited ini is not allowed to claim it is
	if (CurrentObjectives.IsEmpty() && QueuedObjectives.IsEmpty() && TruncatedCount == 0)
	{
		AppendLine(Result, 1, FString::Printf(TEXT("%sno objectives"), ObjectiveDebug::Color::Label));
	}

	return Result;
}

void FDebugInfo::AppendTo(FString& OutString, int32 IndentLevel, const TCHAR* StateColor, const TCHAR* Prefix) const
{
	AppendLine(OutString, IndentLevel, GetHeadline(StateColor, Prefix));

	for (const TPair<FString, FString>& Detail : Details)
	{
		AppendLine(OutString, IndentLevel + 1, FString::Printf(TEXT("%s%s %s%s"),
			ObjectiveDebug::Color::Label, *Detail.Key, ObjectiveDebug::Color::Value, *Detail.Value));
	}

	// A paused container keeps its running set without letting any of it run, so neither the header nor the
	// objectives under it may be drawn as though they were going
	AppendGroup(OutString, IndentLevel + 1,
		bQueued ? ObjectiveDebug::Color::PausedHeader : ObjectiveDebug::Color::RunningHeader,
		bQueued ? TEXT("Running [paused]") : TEXT("Running"),
		bQueued ? ObjectiveDebug::Color::Paused : ObjectiveDebug::Color::Running,
		CurrentObjectives);

	AppendGroup(OutString, IndentLevel + 1, ObjectiveDebug::Color::QueuedHeader, TEXT("Queued"),
		ObjectiveDebug::Color::Queued, QueuedObjectives);
}

FString FDebugInfo::GetHeadline(const TCHAR* StateColor, const TCHAR* Prefix) const
{
	FString Headline = FString::Printf(TEXT("%s%s%s"), StateColor, Prefix, *OwnerName);

	if (!DefinitionName.IsEmpty())
	{
		Headline += FString::Printf(TEXT(" %s[%s]"), ObjectiveDebug::Color::Definition, *DefinitionName);
	}

	if (!Tags.IsEmpty())
	{
		Headline += FString::Printf(TEXT(" %s<%s>"), ObjectiveDebug::Color::Tags, *Tags);
	}

	// The one part of the line the objective wrote itself, so it says which montage is playing or where a
	// sequence has got to. Treated as a value and not as prose for exactly that reason
	if (!Description.IsEmpty())
	{
		Headline += FString::Printf(TEXT(" %s- %s"), ObjectiveDebug::Color::Value, *Description);
	}

	if (!HeldBackBy.IsEmpty())
	{
		Headline += FString::Printf(TEXT(" %s(held by %s%s%s)"), ObjectiveDebug::Color::Label,
			ObjectiveDebug::Color::Value, *HeldBackBy, ObjectiveDebug::Color::Label);
	}

	if (TruncatedCount > 0)
	{
		Headline += FString::Printf(TEXT(" %s(+%d below depth)"), ObjectiveDebug::Color::Label, TruncatedCount);
	}

	if (bIsNext)
	{
		Headline += FString::Printf(TEXT(" %s<< next"), ObjectiveDebug::Color::Next);
	}

	return Headline;
}

void FDebugInfo::AppendGroup(FString& OutString, int32 IndentLevel, const TCHAR* HeaderColor, const TCHAR* Header,
	const TCHAR* StateColor, const TArray<FDebugInfo>& Objectives)
{
	if (Objectives.IsEmpty())
	{
		return;
	}

	AppendLine(OutString, IndentLevel, FString::Printf(TEXT("%s%s (%d)"), HeaderColor, Header, Objectives.Num()));

	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		// Numbered because the order is the priority order: the last entry is the one which gives way
		Objectives[Index].AppendTo(OutString, IndentLevel + 1, StateColor, *FString::Printf(TEXT("%d. "), Index + 1));
	}
}

FString FDebugInfo::Sanitize(FString InString)
{
	// A brace starts a colour tag, and everything up to the closing one is dropped when what is between
	// them is not a colour, so a name carrying one would quietly lose part of itself. A newline would
	// break the line the name sits on in half, taking its indentation with it
	InString.ReplaceCharInline(TEXT('{'), TEXT('('));
	InString.ReplaceCharInline(TEXT('}'), TEXT(')'));
	InString.ReplaceCharInline(TEXT('\n'), TEXT(' '));
	InString.ReplaceCharInline(TEXT('\r'), TEXT(' '));

	return InString;
}

void FDebugInfo::AppendLine(FString& OutString, int32 IndentLevel, FStringView Line)
{
	if (Line.IsEmpty())
	{
		return;
	}

	if (!OutString.IsEmpty())
	{
		OutString.AppendChar(TEXT('\n'));
	}

	OutString.Append(FString::ChrN(IndentLevel * ObjectiveDebug::SpacesPerIndentLevel, TEXT(' ')));
	OutString.Append(Line);
}

#endif // WITH_GAMEPLAY_DEBUGGER_MENU

UAIObjective* FObjectiveActivationResult::GetObjective() const
{
	return IsValid(Objective) ? Objective.Get() : nullptr;
}

bool FObjectiveActivationResult::WasInvalidatedDuringActivation() const
{
	return bActivated && !IsValid(Objective);
}

int32 FObjectiveContainer::GetNestingLevel() const
{
	int32 NestingLevel = 0;

	for (const FObjectiveContainer* Parent = OwningContainer; Parent; Parent = Parent->OwningContainer)
	{
		NestingLevel++;
	}

	return NestingLevel;
}

#if ENABLE_VISUAL_LOG

namespace ObjectiveLog
{
	/** The objectives named on one line, "Alpha, Beta" */
	static FString JoinNames(const TArray<TObjectPtr<UAIObjective>>& Objectives)
	{
		TArray<FString> Names;
		Names.Reserve(Objectives.Num());

		for (const UAIObjective* Objective : Objectives)
		{
			Names.Add(Objective ? Objective->GetName() : TEXT("<invalid>"));
		}

		return FString::Join(Names, TEXT(", "));
	}

	/**
	 * Hangs an indented "Label: Alpha, Beta" line under whatever has been built so far, or adds nothing at
	 * all when there is nothing to name. Leaving the empty groups out is what keeps an entry down to the
	 * one or two lines which actually say what happened
	 */
	static void AppendSection(FString& OutMessage, const TCHAR* Label, const TArray<TObjectPtr<UAIObjective>>& Objectives)
	{
		if (Objectives.IsEmpty())
		{
			return;
		}

		OutMessage += FString::Printf(TEXT("\n  %s: %s"), Label, *JoinNames(Objectives));
	}
}

bool FObjectiveContainer::ShouldVisualLog() const
{
	return GetNestingLevel() < UObjectiveOrientedAIUserSettings::Get().GetVisualLogNestingDepth();
}

FString FObjectiveContainer::GetStateString() const
{
	FString State;

	// A queued container keeps its running set without letting any of it run, so calling that set running
	// would be the one thing the snapshot must not say
	ObjectiveLog::AppendSection(State, bQueued ? TEXT("Running [paused]") : TEXT("Running"), CurrentObjectives);
	ObjectiveLog::AppendSection(State, TEXT("Queued"), QueuedObjectives);

	return State;
}

#endif // ENABLE_VISUAL_LOG

UAIObjective* FObjectiveContainer::TryCreateAIObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> Class) const
{
	if (!IsValid(Class) || !IsValid(ObjectiveComponent))
	{
		return nullptr;
	}
	const UAIObjective* ClassDefaults = Class.GetDefaultObject();

	// There is no point handing back an objective which this component could never run, and asking before
	// it exists is the whole reason the question is put to the class defaults
	if (!ClassDefaults->IsCompatibleWith(ObjectiveComponent))
	{
		return nullptr;
	}

	if (FindBlockingObjective(ClassDefaults))
	{
		return nullptr;
	}
	return NewObject<UAIObjective>(ObjectiveComponent, Class);
}

bool FObjectiveContainer::CanAddObjective(const UObjectiveComponent* ObjectiveComponent, const UAIObjective* AIObjective) const
{
	if (!IsValid(ObjectiveComponent) || !IsValid(AIObjective))
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

	// Asked here rather than later because here is the last moment at which nothing has been spent yet: the
	// objective being asked is still the class default object or the caller's template, and refusing it
	// costs neither a copy nor an OnInitialized
	if (!AIObjective->IsCompatibleWith(ObjectiveComponent))
	{
		return false;
	}

	const UAIObjective* BlockingObjective = FindBlockingObjective(AIObjective);

#if ENABLE_VISUAL_LOG
	// An objective which is turned away here leaves no other trace at all, so this is the only place the
	// refusal and the objective responsible for it can be put on the record together
	if (BlockingObjective && ShouldVisualLog())
	{
		UE_VLOG(ObjectiveComponent->GetPawn(), ObjectiveOrientedAI, Log, TEXT("Blocked %s, by %s%s"),
			*AIObjective->GetName(), *BlockingObjective->GetName(), *GetStateString());
	}
#endif

	return BlockingObjective == nullptr;
}

void FObjectiveContainer::BindStoppedCallback(UAIObjective* InObjective, const FObjectiveStoppedDelegate& OnStopped)
{
	if (OnStopped.IsBound())
	{
		InObjective->OnObjectiveStopped.Add(OnStopped);
	}
}

FObjectiveActivationResult FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* AIObjective, bool bMakeCopy, FObjectiveStoppedDelegate OnStopped)
{
	if (!CanAddObjective(ObjectiveComponent, AIObjective))
	{
		return {};
	}

	if (AIObjective->HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(ObjectiveOrientedAI, Display, TEXT("Attempt to add Class Default Object (%s) to (%s)"), *AIObjective->GetName(), *ObjectiveComponent->GetName());
		return {};
	}

	// Already initialized means it belongs to a component already, so this is the same objective being
	// added a second time rather than a template being turned into one
	if (AIObjective->IsInitialized())
	{
		if (bMakeCopy)
		{
			UE_LOG(ObjectiveOrientedAI, Display, TEXT("Making copy from an initialized objective (%s) currently not allowed."), *AIObjective->GetName());
			return {};
		}

		return AddInitializedObjective(ObjectiveComponent, AIObjective, OnStopped);
	}

	// Copying leaves the caller an untouched template, which is what lets one asset back several
	// objectives at once
	UAIObjective* ObjectiveToRun = bMakeCopy ? DuplicateObject(AIObjective, ObjectiveComponent) : AIObjective;

	const FNullInitializer NullInitializer;
	return AddNewObjective(ObjectiveComponent, ObjectiveToRun, NullInitializer, OnStopped);
}

FObjectiveActivationResult FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass, FObjectiveStoppedDelegate OnStopped)
{
	const FNullInitializer NullInitializer;
	return AddObjective(ObjectiveComponent, AIObjectiveClass, NullInitializer, MoveTemp(OnStopped));
}

FObjectiveActivationResult FObjectiveContainer::AddObjective(UObjectiveComponent* ObjectiveComponent, TSubclassOf<UAIObjective> AIObjectiveClass, TFunctionRef<void(UAIObjective*)> Initializer, FObjectiveStoppedDelegate OnStopped)
{
	// The class default object stands in for the objective which does not exist yet. It only answers
	// the questions which are the same for every instance of the class, so it is not asked whether it
	// can activate: that is put to the real objective once it has been set up
	if (!CanAddObjective(ObjectiveComponent, AIObjectiveClass.GetDefaultObject()))
	{
		return {};
	}

	UAIObjective* NewObjective = NewObject<UAIObjective>(ObjectiveComponent, AIObjectiveClass);

	return AddNewObjective(ObjectiveComponent, NewObjective, Initializer, OnStopped);
}

FObjectiveActivationResult FObjectiveContainer::AddNewObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* InObjective,
	TFunctionRef<void(UAIObjective*)> Initializer, const FObjectiveStoppedDelegate& OnStopped)
{
	if (!IsValid(InObjective))
	{
		return {};
	}

	// Initialize attaches the objective to its component, hands it to the initializer, and only then
	// raises OnInitialized, so the event sees an objective which is already fully set up. Both the
	// setup and the event are user code and either is free to invalidate it on the way through
	if (!InObjective->Initialize(ObjectiveComponent, Initializer))
	{
		return {};
	}

	if (!IsObjectiveValid(InObjective))
	{
		return {};
	}

	// Subscribed before activation, so the caller still hears about a stop which happens while the
	// objective is starting
	BindStoppedCallback(InObjective, OnStopped);

	// Asked of an objective which is fully initialized and set up, so it answers on the state it will
	// actually run with rather than on whatever it was built with
	if (!InObjective->CanActivate(ObjectiveComponent))
	{
		return {};
	}

	return ActivateObjective(InObjective);
}

FObjectiveActivationResult FObjectiveContainer::AddInitializedObjective(UObjectiveComponent* ObjectiveComponent, UAIObjective* InObjective,
	const FObjectiveStoppedDelegate& OnStopped)
{
	if (!IsObjectiveValid(InObjective))
	{
		return {};
	}

	// Nothing to set up, so this only confirms the objective is not being moved to another component
	if (!InObjective->Initialize(ObjectiveComponent))
	{
		return {};
	}

	if (!InObjective->CanActivate(ObjectiveComponent))
	{
		return {};
	}

	BindStoppedCallback(InObjective, OnStopped);

	// Already waiting for its turn, so adding it again asks for nothing it is not already doing
	if (InObjective->IsQueued())
	{
		return FObjectiveActivationResult::Activated(InObjective);
	}

	// Already running, so adding it again is a request to restart it. That counts as an activation even
	// when the objective does not survive it
	if (InObjective->IsRunning())
	{
		InObjective->Start();
		return FObjectiveActivationResult::Activated(InObjective);
	}

	// Initialized but in no state at all, which is where an objective lands when it was set up for an
	// earlier add that CanActivate turned down
	return ActivateObjective(InObjective);
}

void FObjectiveContainer::InvalidateObjective(UAIObjective* AIObjective, bool bSuccess)
{
	// Order preserving removal: the order of both arrays is what decides which objective gives way
	// when two of them hold each other back
	CurrentObjectives.RemoveSingle(AIObjective);
	QueuedObjectives.RemoveSingle(AIObjective);
	const UObject* LogOwner = AIObjective->GetControlledPawn();

#if ENABLE_VISUAL_LOG
	if (ShouldVisualLog())
	{
		UE_VLOG(LogOwner, ObjectiveOrientedAI, Log, TEXT("Invalidated %s, %s"),
			*AIObjective->GetName(), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	}
#endif

	AIObjective->Destroy(bSuccess ? EAIObjectiveStopReason::Succeeded : EAIObjectiveStopReason::Failed);
	ReevaluateQueue(LogOwner);
}

void FObjectiveContainer::ClearObjectives(const FGameplayTagQuery& Query)
{
	const UObject* LogOwner = CurrentObjectives.IsEmpty() ? nullptr : CurrentObjectives[0]->GetControlledPawn();
	DestroyObjectives(ObtainObjectivesByTagQuery(Query), false);
	ReevaluateQueue(LogOwner);
}

void FObjectiveContainer::ClearObjectives(const FGameplayTagContainer& Tags)
{
	const UObject* LogOwner = CurrentObjectives.IsEmpty() ? nullptr : CurrentObjectives[0]->GetControlledPawn();
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
		if (Objective->MatchesObjectiveTagQuery(Query))
		{
			OutObjectives.Add(Objective);
		}
	}
	for (TObjectPtr<UAIObjective> Objective : QueuedObjectives)
	{
		if (Objective->MatchesObjectiveTagQuery(Query))
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

	// A copy, because pausing or resuming one objective runs user code which may add or invalidate others
	const TArray<TObjectPtr<UAIObjective>> CurObjectives = CurrentObjectives;

	if (bQueued)
	{
		EnqueueObjectives(CurObjectives);
		return;
	}

	StartObjectives(CurObjectives);
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
	return AIObjectives.FilterByPredicate([&InTags](const TObjectPtr<UAIObjective> Item){ return Item->HasAnyObjectiveTags(InTags); });
}

const UAIObjective* FObjectiveContainer::FindBlockingObjective(TNotNull<const UAIObjective*> InObjective) const
{
	for (const UAIObjective* Objective : CurrentObjectives)
	{
		if (Objective->ShouldBlockObjective(InObjective))
		{
			return Objective;
		}
	}
	return nullptr;
}

bool FObjectiveContainer::IsObjectiveValid(const UAIObjective* InObjective)
{
	return IsValid(InObjective) && !InObjective->IsPendingInvalidation();
}

void FObjectiveContainer::AppendValidObjectives(TArray<TObjectPtr<UAIObjective>>& OutObjectives, const TArray<TObjectPtr<UAIObjective>>& SourceObjectives)
{
	for (UAIObjective* Objective : SourceObjectives)
	{
		if (IsObjectiveValid(Objective))
		{
			OutObjectives.Add(Objective);
		}
	}
}

int32 FObjectiveContainer::FindHeldBackCandidate(const TArray<TObjectPtr<UAIObjective>>& Candidates)
{
	// Walking from the back is what makes the lowest priority candidate the one which gives way
	for (int32 Index = Candidates.Num() - 1; Index >= 0; Index--)
	{
		const UAIObjective* Candidate = Candidates[Index];

		for (int32 OtherIndex = 0; OtherIndex < Candidates.Num(); OtherIndex++)
		{
			// An objective never holds itself back, whatever its own two tag sets say
			if (OtherIndex == Index)
			{
				continue;
			}

			const UAIObjective* Other = Candidates[OtherIndex];

			if (Other->ShouldQueueObjective(Candidate))
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

void FObjectiveContainer::ResolveEnqueueRules(TArray<TObjectPtr<UAIObjective>>& OutToRun, TArray<TObjectPtr<UAIObjective>>& OutToQueue) const
{
	// Candidates in priority order. Objectives which are already running come first, so an incumbent
	// keeps its place when two objectives want each other queued, and the queue follows in its own
	// order, the objective closest to resuming at the front
	OutToRun.Reserve(CurrentObjectives.Num() + QueuedObjectives.Num());
	AppendValidObjectives(OutToRun, CurrentObjectives);
	AppendValidObjectives(OutToRun, QueuedObjectives);

	// Every objective which gives way changes what the remaining ones hold back, so the search runs
	// again after each one. The candidate list only ever shrinks, which is what makes this settle.
	// Rules which refer to each other in a circle - A queues B, B queues C, C queues A - have no
	// stable answer at all, and end up with the highest priority objective of the circle running alone
	for (int32 HeldBackIndex = FindHeldBackCandidate(OutToRun); HeldBackIndex != INDEX_NONE; HeldBackIndex = FindHeldBackCandidate(OutToRun))
	{
		OutToQueue.Add(OutToRun[HeldBackIndex]);
		OutToRun.RemoveAt(HeldBackIndex);
	}

	// Objectives gave way from the lowest priority upwards, so the queue came out back to front.
	// Righting it puts the objectives which stepped aside ahead of the ones which were already waiting
	Algo::Reverse(OutToQueue);
}

void FObjectiveContainer::ReevaluateQueue(TWeakObjectPtr<const UObject> LogOwner)
{
	// Everything in here is on its way out, so there is nothing to settle and nothing worth starting
	if (bCleaningDown)
	{
		return;
	}

	if (ReevaluationDepth >= ObjectiveQueue::MaxReevaluationDepth)
	{
		const FString Message = FString::Printf(
			TEXT("Objective queue reevaluation nested %d levels deep on %s and was stopped. Look for objectives which keep starting and stopping each other."),
			ReevaluationDepth, *GetNameSafe(LogOwner.Get()));
		
		ensureMsgf(false, TEXT("%s"), *Message);

		UE_LOG(ObjectiveOrientedAI, Warning, TEXT("%s"), *Message);

		return;
	}

	TGuardValue<int32> DepthGuard(ReevaluationDepth, ReevaluationDepth + 1);

	TArray<TObjectPtr<UAIObjective>> ToRun;
	TArray<TObjectPtr<UAIObjective>> ToQueue;
	ResolveEnqueueRules(ToRun, ToQueue);

	// An objective which is already running is left alone, since starting it again would restart it
	// and throw away the work it is doing. Queueing one which is already queued does nothing at all,
	// so the objectives which step aside need no such check of their own
	TArray<TObjectPtr<UAIObjective>> ToStart;
	TArray<TObjectPtr<UAIObjective>> ToEnqueue;

	for (const TObjectPtr<UAIObjective>& Objective : ToRun)
	{
		// A paused container keeps its running set, it just does not let any of it run
		if (bQueued)
		{
			ToEnqueue.Add(Objective);
		}
		else if (!Objective->IsRunning())
		{
			ToStart.Add(Objective);
		}
	}

	ToEnqueue.Append(ToQueue);

	// The outcome is stored before any of it is applied: an objective which starts can read this
	// container straight back, and it has to find the settled split rather than the one it replaced
	CurrentObjectives = MoveTemp(ToRun);
	QueuedObjectives = MoveTemp(ToQueue);

#if ENABLE_VISUAL_LOG
	if (ShouldVisualLog())
	{
		// Objectives which were waiting already and stay waiting are in ToEnqueue too, and naming them
		// would repeat the snapshot below. Only the ones whose state actually changes are news, which
		// also means a reevaluation that settled on what was already there reports nothing at all
		const TArray<TObjectPtr<UAIObjective>> NewlyEnqueued = ToEnqueue.FilterByPredicate(
			[](const TObjectPtr<UAIObjective>& Objective){ return !Objective->IsQueued(); });

		if (!ToStart.IsEmpty() || !NewlyEnqueued.IsEmpty())
		{
			FString Message = TEXT("Reevaluated queue");
			ObjectiveLog::AppendSection(Message, TEXT("Started"), ToStart);
			ObjectiveLog::AppendSection(Message, TEXT("Enqueued"), NewlyEnqueued);

			// Read off the arrays above, which already hold the outcome, so this is where the objectives
			// end up rather than where they were
			Message += GetStateString();

			// The pawn is only held weakly: a callback earlier in this chain may have destroyed it since
			// the caller looked it up
			UE_VLOG(LogOwner.Get(), ObjectiveOrientedAI, Log, TEXT("%s"), *Message);
		}
	}
#endif

	// Queueing runs first, so an objective which is on its way out has stopped itself, released
	// whatever it was driving and left the container the way it will stay before anything starts
	EnqueueObjectives(ToEnqueue);
	StartObjectives(ToStart);
}

template <typename PredicateType>
void FObjectiveContainer::ObtainObjectives(TArray<TObjectPtr<UAIObjective>>& OutObjectives, TArray<TObjectPtr<UAIObjective>>& SourceObjectives, PredicateType Predicate)
{
	// Both arrays keep their order: it is what decides which objective gives way when two of them
	// hold each other back
	for (int32 Index = 0; Index < SourceObjectives.Num(); )
	{
		UAIObjective* Objective = SourceObjectives[Index];

		// Objectives on their way out never come back, so they are dropped rather than handed over
		if (!IsObjectiveValid(Objective))
		{
			SourceObjectives.RemoveAt(Index);
			continue;
		}

		if (!Predicate(Objective))
		{
			Index++;
			continue;
		}

		OutObjectives.Add(Objective);
		SourceObjectives.RemoveAt(Index);
	}
}

TArray<TObjectPtr<UAIObjective>> FObjectiveContainer::ObtainObjectivesByTag(const FGameplayTagContainer& Tags)
{
	auto MatchesTags = [&Tags](const UAIObjective* Objective)
	{
		return Objective->HasAnyObjectiveTags(Tags);
	};

	TArray<TObjectPtr<UAIObjective>> OutObjectives;
	ObtainObjectives(OutObjectives, CurrentObjectives, MatchesTags);
	ObtainObjectives(OutObjectives, QueuedObjectives, MatchesTags);
	return OutObjectives;
}

TArray<TObjectPtr<UAIObjective>> FObjectiveContainer::ObtainObjectivesByTagQuery(const FGameplayTagQuery& Query)
{
	auto MatchesQuery = [&Query](const UAIObjective* Objective)
	{
		return Objective->MatchesObjectiveTagQuery(Query);
	};

	TArray<TObjectPtr<UAIObjective>> OutObjectives;
	ObtainObjectives(OutObjectives, CurrentObjectives, MatchesQuery);
	ObtainObjectives(OutObjectives, QueuedObjectives, MatchesQuery);
	return OutObjectives;
}

TArray<TObjectPtr<UAIObjective>> FObjectiveContainer::ObtainObjectivesCancelledBy(const UAIObjective* InObjective)
{
	auto IsCancelled = [InObjective](const UAIObjective* Objective)
	{
		return InObjective->ShouldCancelObjective(Objective);
	};

	TArray<TObjectPtr<UAIObjective>> OutObjectives;
	ObtainObjectives(OutObjectives, CurrentObjectives, IsCancelled);
	ObtainObjectives(OutObjectives, QueuedObjectives, IsCancelled);
	return OutObjectives;
}

bool FObjectiveContainer::StartObjective(UAIObjective* InObjective)
{
	if (!InObjective->CanActivate(InObjective->GetOwningComponent()))
	{
		InObjective->Invalidate(false);
		return false;
	}
	InObjective->Start();
	return true;
}

bool FObjectiveContainer::EnqueueObjective(UAIObjective* InObjective)
{
	if (!InObjective->bCanBeQueued)
	{
		InObjective->Invalidate(false);
		return false;
	}
	InObjective->Enqueue();
	return true;
}

bool FObjectiveContainer::ShouldObjectiveRun(UAIObjective* InObjective) const
{
	// A paused container holds on to its running set without letting any of it run
	return IsObjectiveValid(InObjective) && !bQueued && CurrentObjectives.Contains(InObjective);
}

bool FObjectiveContainer::ShouldObjectiveWait(UAIObjective* InObjective) const
{
	return IsObjectiveValid(InObjective)
		&& (QueuedObjectives.Contains(InObjective) || (bQueued && CurrentObjectives.Contains(InObjective)));
}

void FObjectiveContainer::StartObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives) const
{
	for (UAIObjective* Objective : InObjectives)
	{
		// Starting one objective is allowed to invalidate the next, to push it into the queue, or to
		// start it already through a nested reevaluation. Starting it again from this snapshot would
		// restart it on top of the work it just began, or set it running while the container has it
		// filed as waiting, so the container has the last word on every entry
		if (ShouldObjectiveRun(Objective) && !Objective->IsRunning())
		{
			StartObjective(Objective);
		}
	}
}

void FObjectiveContainer::DestroyObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives, bool bActivationFailure)
{
	for (UAIObjective* Objective : InObjectives)
	{
		if (!IsValid(Objective))
		{
			continue;
		}

		Objective->GetObjectiveContainer().OwningContainer = nullptr;
		Objective->Destroy(bActivationFailure ? EAIObjectiveStopReason::Failed : EAIObjectiveStopReason::Cancelled);
	}
}

void FObjectiveContainer::EnqueueObjectives(const TArray<TObjectPtr<UAIObjective>>& InObjectives) const
{
	for (UAIObjective* Objective : InObjectives)
	{
		// Same snapshot problem as starting: stopping one objective can lead to the next one being
		// started by a nested reevaluation, and queueing it now would stop the work it just began
		if (ShouldObjectiveWait(Objective))
		{
			EnqueueObjective(Objective);
		}
	}
}

FObjectiveActivationResult FObjectiveContainer::ActivateObjective(UAIObjective* InAIObjective)
{
	InAIObjective->GetObjectiveContainer().OwningContainer = this;

	// Cancelling happens first, so the cancelled objectives take no part in the rules below
	const TArray<TObjectPtr<UAIObjective>> ObjectivesToCancel = ObtainObjectivesCancelledBy(InAIObjective);

#if ENABLE_VISUAL_LOG
	if (ShouldVisualLog())
	{
		// No snapshot here: the newcomer has not joined the container yet, and the reevaluation which
		// follows reports the settled state once it has
		FString Message = FString::Printf(TEXT("Adding %s"), *InAIObjective->GetName());
		ObjectiveLog::AppendSection(Message, TEXT("Cancelled"), ObjectivesToCancel);

		UE_VLOG(InAIObjective->GetControlledPawn(), ObjectiveOrientedAI, Log, TEXT("%s"), *Message);
	}
#endif

	DestroyObjectives(ObjectivesToCancel, false);

	// The newcomer plays by the same rules as everything else, so it simply joins the container and
	// lets the reevaluation decide whether it runs now or waits. Joining the running set, at the back,
	// is what lets it push aside an objective which is only waiting while still yielding to an
	// incumbent when the two of them want each other queued
	CurrentObjectives.AddUnique(InAIObjective);

	ReevaluateQueue(InAIObjective->GetControlledPawn());

	// A reevaluation which refused to run - the container is cleaning down, or nesting ran away - leaves
	// the newcomer sitting in the running set without ever having been given a state. Nothing is going
	// to come back for it, so it is taken out again rather than left behind as a stale entry
	if (InAIObjective->GetState() == EAIObjectiveState::None)
	{
		CurrentObjectives.RemoveSingle(InAIObjective);
		InAIObjective->Destroy(EAIObjectiveStopReason::Failed);
	}

	// The objective may already be invalid by now: it is allowed to invalidate itself from OnStarted,
	// and that still counts as a successful activation. Only never reaching a state at all is a failure,
	// which is what happens when it could not activate or was not allowed to wait
	const bool bActivated = InAIObjective->IsRunning() || InAIObjective->IsQueued() || InAIObjective->WasEverStarted();

	return bActivated ? FObjectiveActivationResult::Activated(InAIObjective) : FObjectiveActivationResult{};
}
