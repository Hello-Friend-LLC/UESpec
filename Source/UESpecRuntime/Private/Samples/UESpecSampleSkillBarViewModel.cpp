#include "Samples/UESpecSampleSkillBarViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecSampleSkillBarViewModel)

UVM_SkillBar::UVM_SkillBar()
{
	Skills.SetNum(1);
	SyncPrimarySkillState();
}

void UVM_SkillBar::EnsureSkillIndex(const int32 Index)
{
	if (Index >= 0 && Skills.Num() <= Index)
	{
		Skills.SetNum(Index + 1);
	}
}

FUESpecSampleSkillState* UVM_SkillBar::GetSkillState(const int32 Index)
{
	return Index >= 0 && Skills.IsValidIndex(Index) ? &Skills[Index] : nullptr;
}

const FUESpecSampleSkillState* UVM_SkillBar::GetSkillState(const int32 Index) const
{
	return Index >= 0 && Skills.IsValidIndex(Index) ? &Skills[Index] : nullptr;
}

void UVM_SkillBar::UseSkill(const int32 Index)
{
	EnsureSkillIndex(Index);

	if (FUESpecSampleSkillState* const SkillState = GetSkillState(Index))
	{
		SkillState->isReady = false;
		SkillState->isOnCooldown = true;
		if (SkillState->cooldownRemaining <= 0.0f)
		{
			SkillState->cooldownRemaining = 5.0f;
		}
	}

	LastUsedSkillIndex = Index;
	SyncPrimarySkillState();
}

void UVM_SkillBar::SetSkillReady(const bool bInSkillReady)
{
	EnsureSkillIndex(0);
	if (FUESpecSampleSkillState* const SkillState = GetSkillState(0))
	{
		SkillState->isReady = bInSkillReady;
		if (bInSkillReady)
		{
			SkillState->isOnCooldown = false;
			SkillState->cooldownRemaining = 0.0f;
		}
	}

	SyncPrimarySkillState();
}

void UVM_SkillBar::SetSkillLocked(const bool bInSkillLocked)
{
	EnsureSkillIndex(0);
	if (FUESpecSampleSkillState* const SkillState = GetSkillState(0))
	{
		SkillState->isLocked = bInSkillLocked;
	}

	SyncPrimarySkillState();
}

void UVM_SkillBar::SyncPrimarySkillState()
{
	const FUESpecSampleSkillState* const SkillState = GetSkillState(0);
	if (!SkillState)
	{
		bSkillReady = true;
		bSkillLocked = false;
		bShowCooldown = false;
		CooldownRatio = 0.0f;
		return;
	}

	bSkillReady = SkillState->isReady;
	bSkillLocked = SkillState->isLocked;
	bShowCooldown = SkillState->isOnCooldown;
	CooldownRatio = SkillState->cooldownRemaining > 0.0f ? FMath::Clamp(SkillState->cooldownRemaining / 10.0f, 0.0f, 1.0f) : 0.0f;
}

bool UVM_SkillBar::IsSkillReady(const int32 Index) const
{
	if (const FUESpecSampleSkillState* const SkillState = GetSkillState(Index))
	{
		return SkillState->isReady && !SkillState->isLocked;
	}

	return false;
}

bool UVM_SkillBar::IsSkillLocked(const int32 Index) const
{
	if (const FUESpecSampleSkillState* const SkillState = GetSkillState(Index))
	{
		return SkillState->isLocked;
	}

	return true;
}
