
#include "AIObjective/AIO_PlayMontage.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	/**
	 * Delegate this objective listens to, or null when the montage has no active instance to listen to.
	 */
	FOnMontageEnded* FindMontageDelegate(UAnimInstance* AnimInstance, UAnimMontage* Montage, bool bBlendingOut)
	{
		if (!IsValid(AnimInstance) || !IsValid(Montage))
		{
			return nullptr;
		}

		return bBlendingOut
			? AnimInstance->Montage_GetBlendingOutDelegate(Montage)
			: AnimInstance->Montage_GetEndedDelegate(Montage);
	}
}

UAIO_PlayMontage* UAIO_PlayMontage::MakeInstance(UAnimMontage* InAnimMontage, const FMontagePlaySettings& InSettings)
{
	UAIO_PlayMontage* Objective = NewObject<UAIO_PlayMontage>();

	Objective->AnimMontage = InAnimMontage;
	Objective->MontageSettings = InSettings;

	return Objective;
}

#if WITH_EDITOR
FText UAIO_PlayMontage::GetDebugDescription_Implementation() const
{
	if (!AnimMontage)
	{
		return FText::FromString(TEXT("No montage"));
	}

	if (MontageSettings.SectionName.IsNone())
	{
		return FText::FromString(AnimMontage->GetName());
	}

	return FText::FromString(FString::Printf(TEXT("%s : %s"), *AnimMontage->GetName(),
		*MontageSettings.SectionName.ToString()));
}
#endif

TArray<FName> UAIO_PlayMontage::GetMontageSections() const
{
	if (!AnimMontage)
	{
		return {};
	}

	TArray<FName> OutSections;

	OutSections.Reserve(AnimMontage->CompositeSections.Num());

	for (const FCompositeSection& Section : AnimMontage->CompositeSections)
	{
		OutSections.Add(Section.SectionName);
	}

	return OutSections;
}

bool UAIO_PlayMontage::CacheAnimInstance()
{
	if (IsValid(CachedAnimInstance))
	{
		return true;
	}

	// Initialize is public, so the pawn is only guaranteed on the paths which go through the container
	const APawn* Pawn = GetControlledPawn();

	if (!Pawn)
	{
		return false;
	}

	if (const USkeletalMeshComponent* SKComp = Pawn->GetComponentByClass<USkeletalMeshComponent>())
	{
		CachedAnimInstance = SKComp->GetAnimInstance();
	}

	return IsValid(CachedAnimInstance);
}

void UAIO_PlayMontage::OnInitialized_Implementation()
{
	CacheAnimInstance();
}

bool UAIO_PlayMontage::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	// Montage_Play reports a play rate of zero as an endless duration rather than a failure, and the
	// montage then sits on its first frame forever, so the objective would never finish
	return IsValid(AnimMontage) && MontageSettings.PlayRate > 0.f;
}

void UAIO_PlayMontage::OnStarted_Implementation(bool bWasQueued)
{
	// The pawn may have gained its mesh, or swapped anim instance, since this objective was initialized
	if (!CacheAnimInstance())
	{
		Invalidate(false);
		return;
	}

	const bool bResuming = MontageSettings.bResumeFromStopPosition && StoppedPosition >= 0.f;

	if (CachedAnimInstance->Montage_Play(AnimMontage, MontageSettings.PlayRate, EMontagePlayReturnType::Duration,
		GetPlayPosition()) <= 0.0)
	{
		Invalidate(false);
		return;
	}

	// Jumping to the section would throw away the position just resumed to, and a montage picking up
	// where it stopped is already inside whichever section it stopped in
	if (!MontageSettings.SectionName.IsNone() && !bResuming)
	{
		// Passing the montage matters, a null one jumps every montage on the anim instance
		CachedAnimInstance->Montage_JumpToSection(MontageSettings.SectionName, AnimMontage);
	}

	FOnMontageEnded* Delegate = FindMontageDelegate(CachedAnimInstance, AnimMontage, MontageSettings.bInvalidateOnBlendOut);

	if (!Delegate)
	{
		// The montage started but has no instance left to report back through, most often because
		// something stopped it again on the way up. Nothing would ever finish this objective
		Invalidate(false);
		return;
	}

	Delegate->BindUObject(this, &UAIO_PlayMontage::OnMontageEnded);
}

void UAIO_PlayMontage::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	FOnMontageEnded* Delegate = FindMontageDelegate(CachedAnimInstance, AnimMontage, MontageSettings.bInvalidateOnBlendOut);

	// A montage which is not listening to this objective is either already ending by itself or has
	// been taken over by another objective playing the same asset. Either way it is not ours to stop
	if (!Delegate || !Delegate->IsBoundToObject(this))
	{
		return;
	}

	Delegate->Unbind();

	// Read before the stop, while the montage still has an active instance to read a position from
	if (MontageSettings.bResumeFromStopPosition)
	{
		StoppedPosition = CachedAnimInstance->Montage_GetPosition(AnimMontage);
	}

	CachedAnimInstance->Montage_Stop(AnimMontage->GetDefaultBlendOutTime(), AnimMontage);
}

float UAIO_PlayMontage::GetPlayPosition() const
{
	if (!MontageSettings.bResumeFromStopPosition || StoppedPosition < 0.f)
	{
		return MontageSettings.StartingPosition;
	}

	return FMath::Clamp(StoppedPosition, 0.f, AnimMontage->GetPlayLength());
}

void UAIO_PlayMontage::OnMontageEnded(UAnimMontage* InAnimMontage, bool bInterrupted)
{
	if (InAnimMontage != AnimMontage || !IsRunning())
	{
		return;
	}

	Invalidate(!bInterrupted);
}
