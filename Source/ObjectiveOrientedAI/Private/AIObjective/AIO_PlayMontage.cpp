
#include "AIObjective/AIO_PlayMontage.h"

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

void UAIO_PlayMontage::OnInitialized_Implementation()
{
	if (IsValid(OptionalAnimInstance))
	{
		return;
	}

	if (USkeletalMeshComponent* SKComp = GetControlledPawn()->GetComponentByClass<USkeletalMeshComponent>())
	{
		OptionalAnimInstance = SKComp->GetAnimInstance();
	}
}

bool UAIO_PlayMontage::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return IsValid(AnimMontage);
}

void UAIO_PlayMontage::OnStarted_Implementation(bool bWasQueued)
{
	if (!IsValid(OptionalAnimInstance))
	{
		Invalidate(false);
		return;
	}
	if (OptionalAnimInstance->Montage_Play(AnimMontage, PlayRate, EMontagePlayReturnType::Duration, StartingPosition) <= 0.0)
	{
		Invalidate(false);
		return;
	}
	if (SectionName != NAME_None)
	{
		OptionalAnimInstance->Montage_JumpToSection(SectionName);
	}

	if (FOnMontageBlendingOutStarted* Delegate = bInvalidateOnBlendOut ? 
		OptionalAnimInstance->Montage_GetBlendingOutDelegate(AnimMontage) : 
		OptionalAnimInstance->Montage_GetEndedDelegate(AnimMontage))
	{
		Delegate->BindUObject(this, &UAIO_PlayMontage::OnMontageEnded);
	}
}

void UAIO_PlayMontage::OnStopped_Implementation(const FStopReasonType& StopReason)
{
	if (!IsValid(OptionalAnimInstance))
	{
		return;
	}

	if (FOnMontageBlendingOutStarted* Delegate = bInvalidateOnBlendOut ? 
		OptionalAnimInstance->Montage_GetBlendingOutDelegate(AnimMontage) : 
		OptionalAnimInstance->Montage_GetEndedDelegate(AnimMontage))
	{
		Delegate->Unbind();
	}

	OptionalAnimInstance->Montage_Stop(AnimMontage->GetDefaultBlendOutTime());
}

void UAIO_PlayMontage::OnMontageEnded(UAnimMontage* InAnimMontage, bool bInterrupted)
{
	Invalidate(!bInterrupted);
}
