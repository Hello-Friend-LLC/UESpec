#pragma once

#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecSampleSkillBarViewModel.generated.h"

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSampleSkillState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	bool isReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	bool isOnCooldown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	bool isLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	float cooldownRemaining = 0.0f;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UVM_SkillBar : public UUESpecViewModelBase
{
	GENERATED_BODY()

public:
	UVM_SkillBar();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec|Samples")
	TArray<FUESpecSampleSkillState> Skills;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESpec|Samples")
	int32 LastUsedSkillIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESpec|Samples")
	bool bSkillReady = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESpec|Samples")
	bool bSkillLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	FText SkillLabel = FText::FromString(TEXT("Slash"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	float CooldownRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "UESpec|Samples")
	bool bShowCooldown = true;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void UseSkill(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetSkillReady(bool bInSkillReady);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SetSkillLocked(bool bInSkillLocked);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Samples")
	void SyncPrimarySkillState();

	UFUNCTION(BlueprintPure, Category = "UESpec|Samples")
	bool IsSkillReady(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "UESpec|Samples")
	bool IsSkillLocked(int32 Index) const;

private:
	void EnsureSkillIndex(int32 Index);
	FUESpecSampleSkillState* GetSkillState(int32 Index);
	const FUESpecSampleSkillState* GetSkillState(int32 Index) const;
};
