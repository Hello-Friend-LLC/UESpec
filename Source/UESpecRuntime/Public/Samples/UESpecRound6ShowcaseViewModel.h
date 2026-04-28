#pragma once

#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecRound6ShowcaseViewModel.generated.h"

UCLASS(BlueprintType)
class UESPECRUNTIME_API UVM_Round6Showcase : public UUESpecViewModelBase
{
	GENERATED_BODY()

public:
	UVM_Round6Showcase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText PlayerName = FText::FromString(TEXT("Knight-01"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText PlayerLevel = FText::FromString(TEXT("Lv.18"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText PlayerHealthLabel = FText::FromString(TEXT("750 / 1000"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float PlayerHealthRatio = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText PlayerResourceLabel = FText::FromString(TEXT("120 / 300"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float PlayerResourceRatio = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText SkillKeybind = FText::FromString(TEXT("Q"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowSkillAlert = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetName = FText::FromString(TEXT("Warden of Ash"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetLevel = FText::FromString(TEXT("Lv.42 Elite"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetHealthLabel = FText::FromString(TEXT("1860 / 2400"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float TargetHealthRatio = 0.775f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText BuffTimer = FText::FromString(TEXT("12s"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowPrimaryBuff = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText DamageNumberLabel = FText::FromString(TEXT("-248!"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowDamageNumber = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText QuestTitle = FText::FromString(TEXT("Return to Ember Keep"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText QuestProgress = FText::FromString(TEXT("2 / 3 Relics"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquipmentPanelTitle = FText::FromString(TEXT("Loadout"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquippedItemName = FText::FromString(TEXT("Ashen Crown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText EquippedItemPower = FText::FromString(TEXT("+412"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TooltipTitle = FText::FromString(TEXT("Ashen Crown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TooltipSummary = FText::FromString(TEXT("Legendary Helm - Power 412"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowTooltip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText ChatSpeaker = FText::FromString(TEXT("[Guild] Mira"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText ChatMessage = FText::FromString(TEXT("Meet at Ember Keep in 5."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowChatLine = true;
};
