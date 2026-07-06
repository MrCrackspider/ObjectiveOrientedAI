
#include "AIObjective/AIObjectiveSequence.h"

UAIObjective* UAIObjectiveSequence::GetCurrentObjective() const
{
	if (Sequence.IsValidIndex(CurrentObjectiveIndex))
	{
		return Sequence[CurrentObjectiveIndex];
	}
	return nullptr;
}

bool UAIObjectiveSequence::CanActivate_Implementation(const UObjectiveComponent* ObjectiveComponent) const
{
	return HasAnyValidObjective();
}

void UAIObjectiveSequence::OnStarted_Implementation(bool bWasQueued)
{
	if (bWasQueued)
	{
		// All pending subobjectives will start automatically
		return;
	}
	if (StartIndex > 0 && StartIndex < Sequence.Num())
	{
		CurrentObjectiveIndex = Inverse ? Sequence.Num() - 1 - StartIndex : StartIndex;
	}
	else
	{
		CurrentObjectiveIndex = Inverse ? Sequence.Num() - 1 : 0;
	}
	StartSequence(CurrentObjectiveIndex);
}

void UAIObjectiveSequence::StartSequence(int32 InStartIndex)
{
	if (IsPendingInvalidation())
	{
		return;
	}
	
	CurrentObjectiveIndex = InStartIndex;
	
	if (!Sequence.IsValidIndex(CurrentObjectiveIndex))
	{
		Invalidate(false);
		return;
	}
	
	UAIObjective* Objective = GetCurrentObjective();
	
	if (!Objective)
	{
		StartSequence(InStartIndex + (Inverse ? -1 : 1));
		return;
	}
	
	if (UAIObjective* ObjectiveCopy = AddSubObjective(Objective, true))
	{
		ObjectiveCopy->OnObjectiveStoppedEvent.AddDynamic(this, &UAIObjectiveSequence::OnObjectiveStopped);
	}
	else
	{
		StartSequence(InStartIndex + (Inverse ? -1 : 1));
	}
}

int32 UAIObjectiveSequence::GetCurrentIndexClamped() const
{
	if (Sequence.IsEmpty())
	{
		return INDEX_NONE;
	}
	return FMath::Clamp(CurrentObjectiveIndex, 0, Sequence.Num() - 1);
}

bool UAIObjectiveSequence::HasAnyValidObjective() const
{
	for (UAIObjective* Obj : Sequence)
	{
		if (Obj)
		{
			return true;
		}
	}
	return false;
}

bool UAIObjectiveSequence::IsEndPoint() const
{
	return (Inverse && CurrentObjectiveIndex == 0) || (!Inverse && CurrentObjectiveIndex == Sequence.Num() - 1);
}

bool UAIObjectiveSequence::IsLoop() const
{
	return ExecutionMode == EObjectiveSequenceMode::Loop;
}

/*int32 UAIObjectiveSequence::FindNextValidIndex() const
{
	int32 CurIndex = GetCurrentIndexClamped();
	if (CurIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	
	
	
	if (Inverse)
	{
		for (int32 i = CurIndex - 1; i >= 0; i--)
		{
			if (Sequence.IsValidIndex(i) && Sequence[i])
			{
				return i;
			}
		}
	}
	else
	{
		for (int32 i = CurIndex + 1; i < Sequence.Num(); i++)
		{
			if (Sequence.IsValidIndex(i) && Sequence[i])
			{
				return i;
			}
		}
	}
}*/

void UAIObjectiveSequence::OnObjectiveStopped(UAIObjective* AIObjective, const FStopReasonType& StopReason)
{
	if (!StopReason.IsPendingInvalidation())
	{
		return;
	}
	
	const bool bIsEndPoint = IsEndPoint();
	
	if (bIsEndPoint && IsLoop() == false)
	{
		Inverse = !Inverse;
	}
	
	CurrentObjectiveIndex = CurrentObjectiveIndex + (Inverse ? -1 : 1);
	
	if (bIsEndPoint)
	{
		if (IsLoop())
		{
			CurrentObjectiveIndex = (CurrentObjectiveIndex + Sequence.Num()) % Sequence.Num(); 
		}
		
		if (NumLoops > 0)
		{
			CurrentLoopNum++;
			if (CurrentLoopNum == NumLoops)
			{
				Invalidate(true);
			}
		}
	}
	StartSequence(CurrentObjectiveIndex);
}
