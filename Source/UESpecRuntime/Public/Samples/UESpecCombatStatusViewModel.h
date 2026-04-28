#pragma once

#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecCombatStatusViewModel.generated.h"

UCLASS(BlueprintType)
class UESPECRUNTIME_API UVM_CombatStatus : public UUESpecViewModelBase
{
	GENERATED_BODY()

public:
	UVM_CombatStatus();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText DisplayName = FText::FromString(TEXT("Knight-01"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText LevelLabel = FText::FromString(TEXT("Lv.18"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText HealthLabel = FText::FromString(TEXT("750 / 1000"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText ResourceLabel = FText::FromString(TEXT("120 / 300"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float HealthRatio = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float ResourceRatio = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowResourceBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float CurrentHealth = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float MaxHealth = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float CurrentResource = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float MaxResource = 300.0f;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetHealthValues(float InCurrentHealth, float InMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetResourceValues(float InCurrentResource, float InMaxResource);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetResourceBarVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void RefreshPresentation();
};
