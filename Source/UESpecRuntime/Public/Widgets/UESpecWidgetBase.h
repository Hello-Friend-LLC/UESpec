#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "StateMachine/UESpecUIStateMachine.h"

#include "UESpecWidgetBase.generated.h"

class UUESpecViewModelBase;
class UUESpecSignalBus;

struct FUESpecCapturedWidgetState
{
	bool bHasRenderOpacity = false;
	float RenderOpacity = 1.0f;
	bool bHasVisibility = false;
	ESlateVisibility Visibility = ESlateVisibility::Visible;
	bool bHasColorAndOpacity = false;
	FLinearColor ColorAndOpacity = FLinearColor::White;
};

UENUM(BlueprintType)
enum class EUESpecWidgetKind : uint8
{
	HUD,
	Menu,
	Popup,
	Modal,
	Debug,
	WorldSpace,
	Component,
	Test
};

UENUM(BlueprintType)
enum class EUESpecBindingMode : uint8
{
	MVVM,
	Manual
};

UENUM(BlueprintType)
enum class EUESpecManualBindingDestination : uint8
{
	Text,
	Percent,
	Visibility
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecManualBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	FName WidgetId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	FString SourcePath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	FString ConverterName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	EUESpecManualBindingDestination Destination = EUESpecManualBindingDestination::Text;
};

UCLASS(Abstract, Blueprintable)
class UESPECRUNTIME_API UUESpecWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	EUESpecWidgetKind WidgetKind = EUESpecWidgetKind::Component;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	TSoftClassPtr<UUESpecViewModelBase> ViewModelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	FName ViewModelContext = TEXT("ViewModel");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	EUESpecBindingMode BindingMode = EUESpecBindingMode::MVVM;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	TArray<FUESpecManualBinding> ManualBindings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FUESpecStateMachineDefinition StateMachine;

	UPROPERTY(BlueprintAssignable, Category = "UESpec|StateMachine")
	FUESpecOnUIStateChanged OnUIStateChanged;

	UFUNCTION(BlueprintCallable, Category = "UESpec|StateMachine")
	void SetUIState(FName NewState);

	UFUNCTION(BlueprintPure, Category = "UESpec|StateMachine")
	FName GetUIState() const;

	UFUNCTION(BlueprintPure, Category = "UESpec|ViewModel")
	UUESpecViewModelBase* GetUESpecViewModel() const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|ViewModel")
	void RefreshUESpecBindings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BindSignalBus(UUESpecSignalBus* SignalBus);
	void UnbindSignalBus();

	UFUNCTION()
	void HandleUESpecSignal(FName SignalName);

private:
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "UESpec|ViewModel", meta = (AllowPrivateAccess = true))
	TObjectPtr<UUESpecViewModelBase> ViewModelInstance = nullptr;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "UESpec|StateMachine", meta = (AllowPrivateAccess = true))
	FName CurrentUIState = NAME_None;

	TWeakObjectPtr<UUESpecSignalBus> BoundSignalBus;
	TMap<FName, FUESpecCapturedWidgetState> CapturedWidgetState;
};

UCLASS(Abstract, Blueprintable)
class UESPECRUNTIME_API UUESpecActivatableWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	EUESpecWidgetKind WidgetKind = EUESpecWidgetKind::HUD;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	TSoftClassPtr<UUESpecViewModelBase> ViewModelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	FName ViewModelContext = TEXT("ViewModel");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	EUESpecBindingMode BindingMode = EUESpecBindingMode::MVVM;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|ViewModel")
	TArray<FUESpecManualBinding> ManualBindings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec")
	FString LayerRoute = TEXT("HUDLayer");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FUESpecStateMachineDefinition StateMachine;

	UPROPERTY(BlueprintAssignable, Category = "UESpec|StateMachine")
	FUESpecOnUIStateChanged OnUIStateChanged;

	UFUNCTION(BlueprintPure, Category = "UESpec")
	bool SupportsBackNavigation() const;

	UFUNCTION(BlueprintPure, Category = "UESpec")
	static bool IsActivatableKind(EUESpecWidgetKind InKind);

	UFUNCTION(BlueprintCallable, Category = "UESpec|StateMachine")
	void SetUIState(FName NewState);

	UFUNCTION(BlueprintPure, Category = "UESpec|StateMachine")
	FName GetUIState() const;

	UFUNCTION(BlueprintPure, Category = "UESpec|ViewModel")
	UUESpecViewModelBase* GetUESpecViewModel() const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|ViewModel")
	void RefreshUESpecBindings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BindSignalBus(UUESpecSignalBus* SignalBus);
	void UnbindSignalBus();

	UFUNCTION()
	void HandleUESpecSignal(FName SignalName);

private:
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "UESpec|ViewModel", meta = (AllowPrivateAccess = true))
	TObjectPtr<UUESpecViewModelBase> ViewModelInstance = nullptr;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "UESpec|StateMachine", meta = (AllowPrivateAccess = true))
	FName CurrentUIState = NAME_None;

	TWeakObjectPtr<UUESpecSignalBus> BoundSignalBus;
	TMap<FName, FUESpecCapturedWidgetState> CapturedWidgetState;
};
