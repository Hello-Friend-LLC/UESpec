#pragma once

#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecInventorySocialViewModel.generated.h"

UCLASS(BlueprintType)
class UESPECRUNTIME_API UVM_InventorySocialOverlay : public UUESpecViewModelBase
{
	GENERATED_BODY()

public:
	UVM_InventorySocialOverlay();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquipmentPanelTitle = FText::FromString(TEXT("Loadout"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquippedHelmetName = FText::FromString(TEXT("Ashen Crown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquippedHelmetPower = FText::FromString(TEXT("+412"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TooltipTitle = FText::FromString(TEXT("Ashen Crown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TooltipSummary = FText::FromString(TEXT("Legendary Helm - Power 412"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText ChatSpeaker = FText::FromString(TEXT("[Guild] Mira"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText ChatMessage = FText::FromString(TEXT("Meet at Ember Keep in 5."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowTooltip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowChatLine = true;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetTooltipVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetChatLineVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetEquippedHelmetPowerLabel(const FString& InLabel);
};
