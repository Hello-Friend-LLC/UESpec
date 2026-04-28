#include "Samples/UESpecCombatStatusViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecCombatStatusViewModel)

namespace
{
	static FText FormatValuePair(const float CurrentValue, const float MaxValue)
	{
		return FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(CurrentValue), FMath::RoundToInt(MaxValue)));
	}
}

UVM_CombatStatus::UVM_CombatStatus()
{
	RefreshPresentation();
}

void UVM_CombatStatus::SetHealthValues(const float InCurrentHealth, const float InMaxHealth)
{
	CurrentHealth = InCurrentHealth;
	MaxHealth = InMaxHealth;
	RefreshPresentation();
}

void UVM_CombatStatus::SetResourceValues(const float InCurrentResource, const float InMaxResource)
{
	CurrentResource = InCurrentResource;
	MaxResource = InMaxResource;
	RefreshPresentation();
}

void UVM_CombatStatus::SetResourceBarVisible(const bool bVisible)
{
	bShowResourceBar = bVisible;
}

void UVM_CombatStatus::RefreshPresentation()
{
	HealthRatio = MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
	ResourceRatio = MaxResource > 0.0f ? FMath::Clamp(CurrentResource / MaxResource, 0.0f, 1.0f) : 0.0f;
	HealthLabel = FormatValuePair(CurrentHealth, MaxHealth);
	ResourceLabel = FormatValuePair(CurrentResource, MaxResource);
}
