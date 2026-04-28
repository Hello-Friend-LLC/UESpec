#pragma once

#include "CoreMinimal.h"

#include "UESpecTestRunner.generated.h"

class FJsonObject;
class FJsonValue;
class UObject;
class SWidget;
class UUserWidget;
class UUESpecViewModelBase;
class UWidget;

UENUM(BlueprintType)
enum class EUESpecTestStatus : uint8
{
	Pass,
	Fail,
	Skip
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecTestFailureContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString SpecPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString TestStep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString FailureMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString ExpectedValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString ActualValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> MissingIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString WidgetTreeSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString StateMachineSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> NearestDeclared;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString CompilerErrorCode;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecTestResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	EUESpecTestStatus Status = EUESpecTestStatus::Pass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString FailureMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FUESpecTestFailureContext FailureContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	float DurationSec = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> ExecutedPhases;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	int32 ExecutedSteps = 0;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecTestRunReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	bool bOk = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecTestResult> Results;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> LogLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString JUnitReportPath;
};

class UESPECRUNTIME_API FUESpecTestRunner
{
public:
	static FUESpecTestRunReport Execute(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& TestSpecObject, const FString& JUnitReportPath = FString(), const FString& SpecPath = FString());

private:
	static bool ExecutePhase(UUserWidget& RootWidget, const FString& SpecPath, const FString& CaseName, const FString& PhaseName, const TArray<TSharedPtr<FJsonValue>>& Steps, FUESpecTestResult& OutResult, FString& OutFailureMessage, FUESpecTestFailureContext& OutFailureContext, FUESpecTestRunReport& OutReport, double StepTimeoutSeconds, double PhaseTimeoutSeconds, double TotalTimeoutSeconds, double CaseStartSeconds);
	static bool ExecuteStep(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& StepObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteClick(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteHover(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteFocus(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteEmitSignal(UUserWidget& RootWidget, const FString& SignalName, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteNavigateKey(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& NavigateKeyObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteSetVMField(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& SetFieldObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteAssertVisible(UUserWidget& RootWidget, const FString& WidgetId, bool bExpected, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteWaitForVisible(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& WaitForVisibleObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteAssertText(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertTextObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteAssertStyle(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertStyleObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteAssertState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertStateObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteAssertFocus(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertFocusObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteScreenshot(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& ScreenshotObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ExecuteWaitForState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& WaitForStateObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext = nullptr);
	static bool ApplyParameterViewModelState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& ParameterObject, FString& OutFailureMessage);
	static bool ApplyJsonValueToProperty(UObject* OwnerObject, FProperty& Property, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutFailureMessage);
	static bool SetObjectPathValue(UObject& RootObject, const FString& FieldPath, const TSharedPtr<FJsonValue>& Value, FString& OutFailureMessage);
	static UUESpecViewModelBase* GetRootViewModel(UUserWidget& RootWidget);
	static UWidget* FindWidgetBySlate(UUserWidget& RootWidget, const TSharedPtr<SWidget>& SlateWidget);
	static TSharedPtr<SWidget> ResolveSlateWidget(UWidget& Widget);
	static bool FocusWidget(UWidget& Widget, FString& OutFailureMessage);
	static bool HoverWidget(UWidget& Widget, FString& OutFailureMessage);
	static bool ResolveStyleToken(const FString& InValue, FString& OutResolvedValue);
	static bool TryGetWidgetText(const UWidget& Widget, FString& OutText);
	static bool TryGetWidgetStyleValue(const UWidget& Widget, const FString& PropertyName, FString& OutValue);
	static FString BuildScreenshotPath(const FString& ScreenshotName);
	static FString NormalizeStepDescriptionForCase(const FString& StepDescription, const FString& CaseName, bool& bOutShouldExecute);
	static UWidget* FindWidgetById(UUserWidget& RootWidget, const FString& WidgetId);
	static FString BuildDefaultJUnitPath();
	static FString BuildXmlEscape(const FString& InValue);
	static FString StatusToJUnitString(EUESpecTestStatus Status);
	static void AppendLogLine(FUESpecTestRunReport& Report, const FString& Line);
	static void WriteJUnitReport(const FUESpecTestRunReport& Report);
};
