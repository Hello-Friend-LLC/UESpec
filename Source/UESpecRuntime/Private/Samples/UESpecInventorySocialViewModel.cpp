#include "Samples/UESpecInventorySocialViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecInventorySocialViewModel)

UVM_InventorySocialOverlay::UVM_InventorySocialOverlay() = default;

void UVM_InventorySocialOverlay::SetTooltipVisible(const bool bVisible)
{
	bShowTooltip = bVisible;
}

void UVM_InventorySocialOverlay::SetChatLineVisible(const bool bVisible)
{
	bShowChatLine = bVisible;
}

void UVM_InventorySocialOverlay::SetEquippedHelmetPowerLabel(const FString& InLabel)
{
	EquippedHelmetPower = FText::FromString(InLabel);
}
