#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/UESpecArpgWidgets.h"
#include "Widgets/UESpecWidgetBase.h"

#include "UESpecSampleWidgets.generated.h"

class UUESpecSignalBus;
class UVM_AdventureOverlay;
class UVM_CombatStatus;
class UVM_InventorySocialOverlay;
class UVM_Round6Showcase;
class UVM_SkillBar;
class UWidget;

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecTestHostWidget : public UUserWidget
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecStateMachineTestWidget : public UUESpecWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void InitializeTestTree();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunTestLifecycle();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunTestTick(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void AttachSignalBus(UUESpecSignalBus* SignalBus);

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UWidget* FindTestWidgetByName(FName WidgetName) const;

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UVM_SkillBar* GetSkillBarViewModel() const;

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	bool IsSlotQHovered() const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RefreshStateFromViewModel();

private:
	UFUNCTION()
	void HandleSlotQClicked();

	UFUNCTION()
	void HandleSlotQHovered();

	UFUNCTION()
	void HandleSlotQUnhovered();

	UPROPERTY(Transient)
	bool bSlotQHovered = false;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecCombatStatusTestWidget : public UUESpecWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void InitializeCombatStatusTree();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunCombatStatusLifecycle();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RefreshCombatStatusFromViewModel();

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UVM_CombatStatus* GetCombatStatusViewModel() const;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecAdventureOverlayTestWidget : public UUESpecWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void InitializeAdventureOverlayTree();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunAdventureOverlayLifecycle();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RefreshAdventureOverlayFromViewModel();

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UVM_AdventureOverlay* GetAdventureOverlayViewModel() const;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecInventorySocialOverlayTestWidget : public UUESpecWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void InitializeInventorySocialTree();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunInventorySocialLifecycle();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RefreshInventorySocialFromViewModel();

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UVM_InventorySocialOverlay* GetInventorySocialViewModel() const;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecRound6ShowcaseTestWidget : public UUESpecWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void InitializeRound6ShowcaseTree();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RunRound6ShowcaseLifecycle();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Tests")
	void RefreshRound6ShowcaseFromViewModel();

	UFUNCTION(BlueprintPure, Category = "UESpec|Tests")
	UVM_Round6Showcase* GetRound6ShowcaseViewModel() const;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecLayerTestWidget : public UUESpecActivatableWidgetBase
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecSignalProbe : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESpec|Tests")
	FName LastSignal = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESpec|Tests")
	int32 BroadcastCount = 0;

	UFUNCTION()
	void HandleSignal(FName SignalName);
};
