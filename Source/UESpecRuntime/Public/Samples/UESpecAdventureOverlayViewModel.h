#pragma once

#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecAdventureOverlayViewModel.generated.h"

UCLASS(BlueprintType)
class UESPECRUNTIME_API UVM_AdventureOverlay : public UUESpecViewModelBase
{
	GENERATED_BODY()

public:
	UVM_AdventureOverlay();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetName = FText::FromString(TEXT("Warden of Ash"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetLevelLabel = FText::FromString(TEXT("Lv.42 Elite"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TargetHealthLabel = FText::FromString(TEXT("1860 / 2400"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float TargetHealthRatio = 0.775f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText PrimaryBuffTimerLabel = FText::FromString(TEXT("12s"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowPrimaryBuff = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText DamageNumberLabel = FText::FromString(TEXT("-248!"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowDamageNumber = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TrackedQuestTitle = FText::FromString(TEXT("Return to Ember Keep"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText TrackedQuestProgress = FText::FromString(TEXT("2 / 3 Relics"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float CurrentTargetHealth = 1860.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float MaxTargetHealth = 2400.0f;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetTargetHealthValues(float InCurrentHealth, float InMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetPrimaryBuffVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetDamageNumberVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetTrackedQuestProgress(const FString& InProgress);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void RefreshPresentation();
};
