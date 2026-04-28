#include "Samples/UESpecAdventureOverlayViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecAdventureOverlayViewModel)

namespace
{
	static FText FormatHealthPair(const float CurrentValue, const float MaxValue)
	{
		return FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(CurrentValue), FMath::RoundToInt(MaxValue)));
	}
}

UVM_AdventureOverlay::UVM_AdventureOverlay()
{
	RefreshPresentation();
}

void UVM_AdventureOverlay::SetTargetHealthValues(const float InCurrentHealth, const float InMaxHealth)
{
	CurrentTargetHealth = InCurrentHealth;
	MaxTargetHealth = InMaxHealth;
	RefreshPresentation();
}

void UVM_AdventureOverlay::SetPrimaryBuffVisible(const bool bVisible)
{
	bShowPrimaryBuff = bVisible;
}

void UVM_AdventureOverlay::SetDamageNumberVisible(const bool bVisible)
{
	bShowDamageNumber = bVisible;
}

void UVM_AdventureOverlay::SetTrackedQuestProgress(const FString& InProgress)
{
	TrackedQuestProgress = FText::FromString(InProgress);
}

void UVM_AdventureOverlay::RefreshPresentation()
{
	TargetHealthRatio = MaxTargetHealth > 0.0f ? FMath::Clamp(CurrentTargetHealth / MaxTargetHealth, 0.0f, 1.0f) : 0.0f;
	TargetHealthLabel = FormatHealthPair(CurrentTargetHealth, MaxTargetHealth);
}
