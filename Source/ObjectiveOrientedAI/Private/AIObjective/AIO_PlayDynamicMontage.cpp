
#include "AIObjective/AIO_PlayDynamicMontage.h"

#include "AIObjective/AIO_PlayMontage.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"

UAIO_PlayDynamicMontage* UAIO_PlayDynamicMontage::MakeInstance(UAnimSequence* InAnimSequence,
	const FDynamicMontagePlaySettings& InSettings)
{
	UAIO_PlayDynamicMontage* Objective = NewObject<UAIO_PlayDynamicMontage>();

	Objective->AnimSequence = InAnimSequence;
	Objective->SlotNodeName = InSettings.SlotNodeName;
	Objective->BlendInTime = InSettings.BlendInTime;
	Objective->BlendOutTime = InSettings.BlendOutTime;
	Objective->bLoop = InSettings.bLoop;

	// Assigning the base type copies the playback half and leaves the rest, which is already read above
	Objective->MontageSettings = InSettings;

	return Objective;
}

#if WITH_EDITOR
FText UAIO_PlayDynamicMontage::GetDebugDescription_Implementation() const
{
	if (!AnimSequence)
	{
		return FText::FromString(TEXT("no sequence"));
	}

	return FText::FromString(FString::Printf(TEXT("%s on %s%s"),
		*AnimSequence->GetName(), *SlotNodeName.ToString(), bLoop ? TEXT(", looping") : TEXT("")));
}
#endif

TArray<FName> UAIO_PlayDynamicMontage::GetSlotNames() const
{
	const USkeleton* Skeleton = AnimSequence ? AnimSequence->GetSkeleton() : nullptr;

	if (!Skeleton)
	{
		return {};
	}

	TArray<FName> OutSlotNames;

	for (const FAnimSlotGroup& SlotGroup : Skeleton->GetSlotGroups())
	{
		OutSlotNames.Append(SlotGroup.SlotNames);
	}

	return OutSlotNames;
}

bool UAIO_PlayDynamicMontage::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	// The things the montage is built from, asked before anything is built. A play rate of zero leaves the
	// montage sitting on its first frame forever, so the objective would never finish
	if (!IsValid(AnimSequence) || MontageSettings.PlayRate <= 0.f)
	{
		return false;
	}

	// A slot the skeleton has never heard of cannot be played on, and a dynamic montage aimed at one plays
	// into nothing rather than failing, so it is turned down here instead. This only proves the slot was
	// authored somewhere against this skeleton - whether this pawn's anim graph has it is its own business
	const USkeleton* Skeleton = AnimSequence->GetSkeleton();

	return Skeleton && Skeleton->ContainsSlotName(SlotNodeName);
}

UAnimMontage* UAIO_PlayDynamicMontage::CreateDynamicMontage() const
{
	// The play rate and start position arguments of this call are dead - a dynamic montage carries neither,
	// they are applied when it is played - so the sub objective is left to apply them instead
	UAnimMontage* NewMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		AnimSequence, SlotNodeName, BlendInTime, BlendOutTime);

	if (!NewMontage)
	{
		return nullptr;
	}

	if (bLoop)
	{
		// A dynamic montage is built with exactly one section, which by default leads nowhere and so blends
		// out at the end of the sequence. Pointing it back at itself is what makes it play on forever
		for (FCompositeSection& Section : NewMontage->CompositeSections)
		{
			Section.NextSectionName = Section.SectionName;
		}
	}

	return NewMontage;
}

void UAIO_PlayDynamicMontage::OnStarted_Implementation(bool bWasQueued)
{
	if (MontageObjective.IsValid())
	{
		// Coming back from the queue resumed the montage objective alongside this one, so it is playing
		// again already and a second one would play on top of it
		if (bWasQueued)
		{
			return;
		}

		// A restart which was told to keep its sub objectives would stack montages the same way
		ClearSubObjectives();
	}

	// The montage outlives a restart, so only the first pass has to build one
	if (!IsValid(DynamicMontage))
	{
		DynamicMontage = CreateDynamicMontage();
	}

	if (!IsValid(DynamicMontage))
	{
		Invalidate(false);
		return;
	}

	// The built montage carries a single section named Default, so there is nothing a section name could
	// jump to and anything set here would only warn its way through Montage_JumpToSection
	UAIO_PlayMontage* NewMontageObjective = UAIO_PlayMontage::MakeInstance(DynamicMontage,
		FMontagePlaySettings(MontageSettings).SetSectionName(NAME_None));

	// Subscribing through AddSubObjective means the montage reports back even if it stops while activating
	const FObjectiveActivationResult Result = AddSubObjective(NewMontageObjective, false,
		FObjectiveStoppedDelegate::CreateUObject(this, &UAIO_PlayDynamicMontage::OnMontageObjectiveStopped));

	// A montage which did not survive activation has already reported back and invalidated this objective,
	// so only one which never activated at all is left to answer for here
	if (!Result)
	{
		Invalidate(false);
		return;
	}

	MontageObjective = Result.GetObjective();
}

void UAIO_PlayDynamicMontage::OnMontageObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	// Being queued only pauses the montage, and being cancelled means this objective took it away itself,
	// by restarting or tearing down. Only the montage running out is this objective finishing its work
	const bool bRanToCompletion = StopReason.Reason == EAIObjectiveStopReason::Succeeded
		|| StopReason.Reason == EAIObjectiveStopReason::Failed;

	if (!bRanToCompletion || !IsRunning())
	{
		return;
	}

	MontageObjective = nullptr;

	Invalidate(StopReason.IsSucceeded());
}
