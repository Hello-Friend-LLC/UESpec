#include "TestSpec/UESpecTestRunner.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UESpecJsonCompat.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UESpecSignalBus.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "UObject/UnrealType.h"
#include "ViewModel/UESpecViewModelBase.h"
#include "Widgets/SWindow.h"
#include "Widgets/UESpecWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogUESpecTestRunner, Log, All);

namespace
{
	static const FString InvalidSpecMessage = TEXT("testSpec must contain a phases object.");

	struct FUESpecTimeoutSettings
	{
		double StepSeconds = 0.0;
		double PhaseSeconds = 0.0;
		double TotalSeconds = 0.0;
	};

	struct FUESpecPathSegment
	{
		FString PropertyName;
		TOptional<int32> ArrayIndex;
	};

	static bool TryGetWidgetState(const UUserWidget& RootWidget, FName& OutState)
	{
		if (const UUESpecWidgetBase* const UESpecWidget = Cast<UUESpecWidgetBase>(&RootWidget))
		{
			OutState = UESpecWidget->GetUIState();
			return true;
		}

		if (const UUESpecActivatableWidgetBase* const ActivatableWidget = Cast<UUESpecActivatableWidgetBase>(&RootWidget))
		{
			OutState = ActivatableWidget->GetUIState();
			return true;
		}

		OutState = NAME_None;
		return false;
	}

	static void SetWidgetState(UUserWidget& RootWidget, const FName NewState)
	{
		if (UUESpecWidgetBase* const UESpecWidget = Cast<UUESpecWidgetBase>(&RootWidget))
		{
			UESpecWidget->SetUIState(NewState);
			return;
		}

		if (UUESpecActivatableWidgetBase* const ActivatableWidget = Cast<UUESpecActivatableWidgetBase>(&RootWidget))
		{
			ActivatableWidget->SetUIState(NewState);
		}
	}

	static const FUESpecStateMachineDefinition* GetStateMachineDefinition(const UUserWidget& RootWidget)
	{
		if (const UUESpecWidgetBase* const UESpecWidget = Cast<UUESpecWidgetBase>(&RootWidget))
		{
			return &UESpecWidget->StateMachine;
		}

		if (const UUESpecActivatableWidgetBase* const ActivatableWidget = Cast<UUESpecActivatableWidgetBase>(&RootWidget))
		{
			return &ActivatableWidget->StateMachine;
		}

		return nullptr;
	}

	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return JsonString;
	}

	static bool IsNonCollapsedVisibility(const ESlateVisibility Visibility)
	{
		return Visibility == ESlateVisibility::Visible ||
			Visibility == ESlateVisibility::HitTestInvisible ||
			Visibility == ESlateVisibility::SelfHitTestInvisible;
	}

	static bool IsWidgetVisibleForTesting(const UWidget& Widget)
	{
		const UWidget* CurrentWidget = &Widget;
		while (CurrentWidget)
		{
			if (!IsNonCollapsedVisibility(CurrentWidget->GetVisibility()))
			{
				return false;
			}

			CurrentWidget = CurrentWidget->GetParent();
		}

		return true;
	}

	static TSharedRef<FJsonObject> BuildWidgetSnapshotObject(const UWidget& Widget)
	{
		const TSharedRef<FJsonObject> WidgetObject = MakeShared<FJsonObject>();
		WidgetObject->SetStringField(TEXT("id"), Widget.GetName());
		WidgetObject->SetStringField(TEXT("type"), Widget.GetClass()->GetName());
		WidgetObject->SetBoolField(TEXT("visible"), IsWidgetVisibleForTesting(Widget));

		if (const UEnum* const VisibilityEnum = StaticEnum<ESlateVisibility>())
		{
			WidgetObject->SetStringField(TEXT("visibility"), VisibilityEnum->GetNameStringByValue(static_cast<int64>(Widget.GetVisibility())));
		}

		if (const UPanelWidget* const PanelWidget = Cast<UPanelWidget>(&Widget))
		{
			TArray<TSharedPtr<FJsonValue>> Children;
			Children.Reserve(PanelWidget->GetChildrenCount());
			for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
			{
				if (const UWidget* const ChildWidget = PanelWidget->GetChildAt(ChildIndex))
				{
					Children.Add(MakeShared<FJsonValueObject>(BuildWidgetSnapshotObject(*ChildWidget)));
				}
			}

			WidgetObject->SetArrayField(TEXT("children"), Children);
		}

		return WidgetObject;
	}

	static FString BuildWidgetTreeSnapshot(const UUserWidget& RootWidget)
	{
		const TSharedRef<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
		SnapshotObject->SetStringField(TEXT("rootWidgetClass"), RootWidget.GetClass()->GetName());

		if (RootWidget.WidgetTree && RootWidget.WidgetTree->RootWidget)
		{
			SnapshotObject->SetObjectField(TEXT("root"), BuildWidgetSnapshotObject(*RootWidget.WidgetTree->RootWidget));
		}
		else
		{
			SnapshotObject->SetField(TEXT("root"), MakeShared<FJsonValueNull>());
		}

		return SerializeJsonObject(SnapshotObject);
	}

	static FString BuildStateMachineSnapshot(const UUserWidget& RootWidget)
	{
		const TSharedRef<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
		FName CurrentState = NAME_None;
		TryGetWidgetState(RootWidget, CurrentState);
		SnapshotObject->SetStringField(TEXT("currentState"), CurrentState.ToString());

		TArray<TSharedPtr<FJsonValue>> AvailableStates;
		if (const FUESpecStateMachineDefinition* const Definition = GetStateMachineDefinition(RootWidget))
		{
			AvailableStates.Reserve(Definition->States.Num());
			for (const FUESpecStateDefinition& StateDefinition : Definition->States)
			{
				AvailableStates.Add(MakeShared<FJsonValueString>(StateDefinition.Name.ToString()));
			}
		}

		SnapshotObject->SetArrayField(TEXT("availableStates"), AvailableStates);
		return SerializeJsonObject(SnapshotObject);
	}

	static void SetFailureContext(
		FUESpecTestFailureContext* OutFailureContext,
		const FString& Category,
		const FString& FailureMessage,
		const FString& ExpectedValue = FString(),
		const FString& ActualValue = FString(),
		const TArray<FString>& MissingIds = {},
		const TArray<FString>& NearestDeclared = {},
		const FString& CompilerErrorCode = FString())
	{
		if (!OutFailureContext)
		{
			return;
		}

		OutFailureContext->Category = Category;
		OutFailureContext->FailureMessage = FailureMessage;
		OutFailureContext->ExpectedValue = ExpectedValue;
		OutFailureContext->ActualValue = ActualValue;
		OutFailureContext->MissingIds = MissingIds;
		OutFailureContext->NearestDeclared = NearestDeclared;
		OutFailureContext->CompilerErrorCode = CompilerErrorCode;
	}

	static bool TryExecuteHeadlessClick(UWidget& Widget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
	{
		if (UButton* const Button = Cast<UButton>(&Widget))
		{
			if (!Button->OnClicked.IsBound())
			{
				OutFailureMessage = FString::Printf(TEXT("Slate click on widget '%s' was not handled."), *WidgetId);
				SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
				return false;
			}

			Button->OnClicked.Broadcast();
			return true;
		}

		OutFailureMessage = FString::Printf(TEXT("Slate click on widget '%s' was not handled."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
		return false;
	}

	static bool RouteSignalToRootWidget(UUserWidget& RootWidget, const FName SignalName)
	{
		if (UGameInstance* const GameInstance = RootWidget.GetGameInstance())
		{
			if (UUESpecSignalBus* const SignalBus = GameInstance->GetSubsystem<UUESpecSignalBus>())
			{
				SignalBus->Broadcast(SignalName);
				return true;
			}
		}

		if (UFunction* const HandlerFunction = RootWidget.FindFunction(TEXT("HandleUESpecSignal")))
		{
			struct FSignalParams
			{
				FName SignalName = NAME_None;
			};

			FSignalParams Params;
			Params.SignalName = SignalName;
			RootWidget.ProcessEvent(HandlerFunction, &Params);
			return true;
		}

		return false;
	}

	static void FinalizeFailureContext(
		UUserWidget& RootWidget,
		const FString& SpecPath,
		const FString& StepDescription,
		const FString& FailureMessage,
		FUESpecTestFailureContext& InOutFailureContext)
	{
		if (InOutFailureContext.Category.IsEmpty())
		{
			InOutFailureContext.Category = TEXT("UNKNOWN");
		}

		if (InOutFailureContext.SpecPath.IsEmpty())
		{
			InOutFailureContext.SpecPath = SpecPath;
		}

		if (InOutFailureContext.TestStep.IsEmpty())
		{
			InOutFailureContext.TestStep = StepDescription;
		}

		if (InOutFailureContext.FailureMessage.IsEmpty())
		{
			InOutFailureContext.FailureMessage = FailureMessage;
		}

		if (InOutFailureContext.WidgetTreeSnapshot.IsEmpty())
		{
			InOutFailureContext.WidgetTreeSnapshot = BuildWidgetTreeSnapshot(RootWidget);
		}

		if (InOutFailureContext.StateMachineSnapshot.IsEmpty())
		{
			InOutFailureContext.StateMachineSnapshot = BuildStateMachineSnapshot(RootWidget);
		}
	}

	static FString DescribeStep(const TSharedPtr<FJsonObject>& StepObject)
	{
		if (!StepObject.IsValid())
		{
			return TEXT("<invalid-step>");
		}

		FString StepDescription;
		if (StepObject->TryGetStringField(TEXT("step"), StepDescription) && !StepDescription.IsEmpty())
		{
			return StepDescription;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*StepObject))
		{
			if (Pair.Key != TEXT("step"))
			{
				return Pair.Key;
			}
		}

		return TEXT("<unnamed-step>");
	}

	static bool HasTimedOut(const double StartSeconds, const double LimitSeconds)
	{
		return LimitSeconds > 0.0 && (FPlatformTime::Seconds() - StartSeconds) > LimitSeconds;
	}

	static FUESpecTimeoutSettings ParseTimeoutSettings(const TSharedPtr<FJsonObject>& TestSpecObject)
	{
		FUESpecTimeoutSettings Settings;
		if (!TestSpecObject.IsValid())
		{
			return Settings;
		}

		const TSharedPtr<FJsonObject>* TimeoutObject = nullptr;
		if (!TestSpecObject->TryGetObjectField(TEXT("timeout"), TimeoutObject) || !TimeoutObject || !(*TimeoutObject).IsValid())
		{
			return Settings;
		}

		(*TimeoutObject)->TryGetNumberField(TEXT("step"), Settings.StepSeconds);
		(*TimeoutObject)->TryGetNumberField(TEXT("phase"), Settings.PhaseSeconds);
		(*TimeoutObject)->TryGetNumberField(TEXT("total"), Settings.TotalSeconds);
		return Settings;
	}

	static bool WaitUntil(const TFunctionRef<bool()>& Predicate, const double TimeoutSeconds)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		do
		{
			if (Predicate())
			{
				return true;
			}

			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().Tick();
			}

			FPlatformProcess::Sleep(0.01f);
		}
		while (TimeoutSeconds > 0.0 && (FPlatformTime::Seconds() - StartSeconds) < TimeoutSeconds);

		return Predicate();
	}

	static bool TryGetNumberValue(const TSharedPtr<FJsonValue>& Value, double& OutNumber)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		if (Value->TryGetNumber(OutNumber))
		{
			return true;
		}

		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			return LexTryParseString(OutNumber, *StringValue);
		}

		return false;
	}

	static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("<null>");
		}

		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			return StringValue;
		}

		if (Value->Type == EJson::Boolean)
		{
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		}

		if (Value->Type == EJson::Number)
		{
			return LexToString(Value->AsNumber());
		}

		return Value->AsString();
	}

	static FString NormalizeNameForComparison(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			if (FChar::IsAlnum(Character))
			{
				Result.AppendChar(FChar::ToLower(Character));
			}
		}

		return Result;
	}

	static bool PropertyNameMatches(const FProperty& Property, const FString& ExpectedName)
	{
		const FString NormalizedExpected = NormalizeNameForComparison(ExpectedName);
		const FString PropertyName = Property.GetName();
		if (NormalizeNameForComparison(PropertyName) == NormalizedExpected)
		{
			return true;
		}

		if (Property.IsA<FBoolProperty>() && PropertyName.Len() > 1 && PropertyName[0] == TEXT('b') && FChar::IsUpper(PropertyName[1]))
		{
			return NormalizeNameForComparison(PropertyName.Mid(1)) == NormalizedExpected;
		}

		return false;
	}

	static FProperty* FindPropertyBySpecName(UStruct& Struct, const FString& PropertyName)
	{
		for (TFieldIterator<FProperty> PropertyIt(&Struct); PropertyIt; ++PropertyIt)
		{
			if (FProperty* const Property = *PropertyIt; PropertyNameMatches(*Property, PropertyName))
			{
				return Property;
			}
		}

		return nullptr;
	}

	static bool ParseObjectPath(const FString& FieldPath, TArray<FUESpecPathSegment>& OutSegments, FString& OutFailureMessage)
	{
		OutSegments.Reset();

		TArray<FString> Parts;
		FieldPath.ParseIntoArray(Parts, TEXT("."), true);
		if (Parts.IsEmpty())
		{
			OutFailureMessage = TEXT("field path is empty.");
			return false;
		}

		for (const FString& Part : Parts)
		{
			FUESpecPathSegment Segment;
			const int32 OpenBracketIndex = Part.Find(TEXT("["));
			if (OpenBracketIndex == INDEX_NONE)
			{
				Segment.PropertyName = Part;
			}
			else
			{
				const int32 CloseBracketIndex = Part.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenBracketIndex + 1);
				if (CloseBracketIndex == INDEX_NONE || CloseBracketIndex != Part.Len() - 1)
				{
					OutFailureMessage = FString::Printf(TEXT("field path segment '%s' has an invalid array suffix."), *Part);
					return false;
				}

				Segment.PropertyName = Part.Left(OpenBracketIndex);
				int32 ParsedIndex = INDEX_NONE;
				if (!LexTryParseString(ParsedIndex, *Part.Mid(OpenBracketIndex + 1, CloseBracketIndex - OpenBracketIndex - 1)) || ParsedIndex < 0)
				{
					OutFailureMessage = FString::Printf(TEXT("field path segment '%s' has an invalid array index."), *Part);
					return false;
				}

				Segment.ArrayIndex = ParsedIndex;
			}

			if (Segment.PropertyName.IsEmpty())
			{
				OutFailureMessage = FString::Printf(TEXT("field path segment '%s' is missing a property name."), *Part);
				return false;
			}

			OutSegments.Add(MoveTemp(Segment));
		}

		return true;
	}

	static bool TryParseVisibility(const FString& Value, ESlateVisibility& OutVisibility)
	{
		const FString TrimmedValue = Value.TrimStartAndEnd();
		if (TrimmedValue.Equals(TEXT("Visible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Visible;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Hidden;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Collapsed;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::HitTestInvisible;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::SelfHitTestInvisible;
			return true;
		}

		return false;
	}

	static bool TryParseColor(const FString& Value, FLinearColor& OutColor)
	{
		const FString TrimmedValue = Value.TrimStartAndEnd();
		if (TrimmedValue.StartsWith(TEXT("#")) || TrimmedValue.Contains(TEXT("=")))
		{
			return OutColor.InitFromString(TrimmedValue);
		}

		if (TrimmedValue.Equals(TEXT("White"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::White;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Black"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Black;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Red"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Red;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Green"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Green;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Blue"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Blue;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Transparent"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Transparent;
			return true;
		}

		return false;
	}

	static FKey ResolveNavigationKey(const FString& KeyName)
	{
		if (KeyName.Equals(TEXT("Up"), ESearchCase::IgnoreCase))
		{
			return EKeys::Up;
		}
		if (KeyName.Equals(TEXT("Down"), ESearchCase::IgnoreCase))
		{
			return EKeys::Down;
		}
		if (KeyName.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
		{
			return EKeys::Left;
		}
		if (KeyName.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
		{
			return EKeys::Right;
		}
		if (KeyName.Equals(TEXT("Tab"), ESearchCase::IgnoreCase))
		{
			return EKeys::Tab;
		}

		return FKey(*KeyName);
	}

	static bool IsOpacityProperty(const FString& PropertyName)
	{
		return PropertyName.Equals(TEXT("opacity"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("renderOpacity"), ESearchCase::IgnoreCase);
	}

	static bool IsVisibilityProperty(const FString& PropertyName)
	{
		return PropertyName.Equals(TEXT("visibility"), ESearchCase::IgnoreCase);
	}

	static bool IsColorProperty(const FString& PropertyName)
	{
		return PropertyName.Equals(TEXT("color"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("colorAndOpacity"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("fontColor"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("fillColor"), ESearchCase::IgnoreCase);
	}

	static bool IsTextMatch(const FString& Actual, const FString& Expected, const FString& MatchMode)
	{
		if (MatchMode.Equals(TEXT("Contains"), ESearchCase::IgnoreCase))
		{
			return Actual.Contains(Expected);
		}
		if (MatchMode.Equals(TEXT("Regex"), ESearchCase::IgnoreCase))
		{
			const FRegexPattern Pattern(Expected);
			FRegexMatcher Matcher(Pattern, Actual);
			return Matcher.FindNext();
		}

		return Actual == Expected;
	}
}

FUESpecTestRunReport FUESpecTestRunner::Execute(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& TestSpecObject, const FString& JUnitReportPath, const FString& SpecPath)
{
	FUESpecTestRunReport Report;
	Report.bOk = true;
	Report.JUnitReportPath = JUnitReportPath.IsEmpty() ? BuildDefaultJUnitPath() : JUnitReportPath;

	if (!TestSpecObject.IsValid())
	{
		FUESpecTestResult& Result = Report.Results.AddDefaulted_GetRef();
		Result.Name = TEXT("InvalidTestSpec");
		Result.Status = EUESpecTestStatus::Fail;
		Result.FailureMessage = InvalidSpecMessage;
		SetFailureContext(&Result.FailureContext, TEXT("UNKNOWN"), InvalidSpecMessage);
		FinalizeFailureContext(RootWidget, SpecPath, TEXT("InvalidTestSpec"), InvalidSpecMessage, Result.FailureContext);
		Report.bOk = false;
		AppendLogLine(Report, TEXT("[FAIL] InvalidTestSpec: testSpec object is missing."));
		WriteJUnitReport(Report);
		return Report;
	}

	const TSharedPtr<FJsonObject>* PhasesObject = nullptr;
	if (!TestSpecObject->TryGetObjectField(TEXT("phases"), PhasesObject) || !PhasesObject || !(*PhasesObject).IsValid())
	{
		FUESpecTestResult& Result = Report.Results.AddDefaulted_GetRef();
		Result.Name = TEXT("InvalidTestSpec");
		Result.Status = EUESpecTestStatus::Fail;
		Result.FailureMessage = InvalidSpecMessage;
		SetFailureContext(&Result.FailureContext, TEXT("UNKNOWN"), InvalidSpecMessage);
		FinalizeFailureContext(RootWidget, SpecPath, TEXT("InvalidTestSpec"), InvalidSpecMessage, Result.FailureContext);
		Report.bOk = false;
		AppendLogLine(Report, TEXT("[FAIL] InvalidTestSpec: testSpec.phases is missing."));
		WriteJUnitReport(Report);
		return Report;
	}

	FName InitialWidgetState = NAME_None;
	TryGetWidgetState(RootWidget, InitialWidgetState);

	TArray<TSharedPtr<FJsonValue>> Parameters;
	if (const TArray<TSharedPtr<FJsonValue>>* ParametersArray = nullptr; TestSpecObject->TryGetArrayField(TEXT("parameters"), ParametersArray) && ParametersArray)
	{
		Parameters = *ParametersArray;
	}

	if (Parameters.Num() == 0)
	{
		Parameters.Add(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));
	}

	for (int32 ParameterIndex = 0; ParameterIndex < Parameters.Num(); ++ParameterIndex)
	{
		if (!InitialWidgetState.IsNone())
		{
			SetWidgetState(RootWidget, InitialWidgetState);
		}

		const double StartSeconds = FPlatformTime::Seconds();
		FUESpecTestResult Result;
		Result.Name = FString::Printf(TEXT("Case%d"), ParameterIndex + 1);

		TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
		if (const TSharedPtr<FJsonObject>* ParameterObjectPtr = nullptr; Parameters[ParameterIndex].IsValid() && Parameters[ParameterIndex]->TryGetObject(ParameterObjectPtr) && ParameterObjectPtr && (*ParameterObjectPtr).IsValid())
		{
			ParameterObject = *ParameterObjectPtr;

			FString ParameterName;
			if (ParameterObject->TryGetStringField(TEXT("name"), ParameterName) && !ParameterName.IsEmpty())
			{
				Result.Name = ParameterName;
			}
		}

		bool bSkipRemainingPhases = false;
		bool bHasFailure = false;
		bool bAppliedVmState = false;
		const FUESpecTimeoutSettings TimeoutSettings = ParseTimeoutSettings(TestSpecObject);
		const double CaseStartSeconds = StartSeconds;

		for (const FString& PhaseName : { TEXT("assume"), TEXT("arrange"), TEXT("act"), TEXT("assert") })
		{
			if (PhaseName == TEXT("arrange") && !bAppliedVmState)
			{
				FString VmStateFailureMessage;
				if (!ApplyParameterViewModelState(RootWidget, ParameterObject, VmStateFailureMessage))
				{
					Result.Status = EUESpecTestStatus::Fail;
					Result.FailureMessage = VmStateFailureMessage;
					SetFailureContext(&Result.FailureContext, TEXT("UNKNOWN"), VmStateFailureMessage);
					FinalizeFailureContext(RootWidget, SpecPath, TEXT("arrange vmState"), VmStateFailureMessage, Result.FailureContext);
					bHasFailure = true;
					AppendLogLine(Report, FString::Printf(TEXT("[FAIL] %s %s: %s"), *Result.Name, *PhaseName, *VmStateFailureMessage));
					break;
				}

				bAppliedVmState = true;
			}

			const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
			if (!(*PhasesObject)->TryGetArrayField(PhaseName, StepsArray) || !StepsArray)
			{
				continue;
			}

			Result.ExecutedPhases.Add(PhaseName);
			FString FailureMessage;
			FUESpecTestFailureContext FailureContext;
			const bool bPhaseOk = ExecutePhase(
				RootWidget,
				SpecPath,
				Result.Name,
				PhaseName,
				*StepsArray,
				Result,
				FailureMessage,
				FailureContext,
				Report,
				TimeoutSettings.StepSeconds,
				TimeoutSettings.PhaseSeconds,
				TimeoutSettings.TotalSeconds,
				CaseStartSeconds);
			if (bPhaseOk)
			{
				continue;
			}

			if (PhaseName == TEXT("assume"))
			{
				Result.Status = EUESpecTestStatus::Skip;
				Result.FailureMessage = FailureMessage;
				Result.FailureContext = FailureContext;
				bSkipRemainingPhases = true;
				break;
			}

			if (!bHasFailure)
			{
				Result.Status = EUESpecTestStatus::Fail;
				Result.FailureMessage = FailureMessage;
				Result.FailureContext = FailureContext;
				bHasFailure = true;
			}
		}

		if (!bHasFailure && !bSkipRemainingPhases)
		{
			Result.Status = EUESpecTestStatus::Pass;
		}

		Result.DurationSec = static_cast<float>(FPlatformTime::Seconds() - StartSeconds);
		Report.Results.Add(Result);
		Report.bOk &= Result.Status != EUESpecTestStatus::Fail;
		AppendLogLine(Report, FString::Printf(TEXT("[%s] %s (%0.3fs)"), *StatusToJUnitString(Result.Status), *Result.Name, Result.DurationSec));
		WriteJUnitReport(Report);
	}

	return Report;
}

bool FUESpecTestRunner::ExecutePhase(UUserWidget& RootWidget, const FString& SpecPath, const FString& CaseName, const FString& PhaseName, const TArray<TSharedPtr<FJsonValue>>& Steps, FUESpecTestResult& OutResult, FString& OutFailureMessage, FUESpecTestFailureContext& OutFailureContext, FUESpecTestRunReport& OutReport, const double StepTimeoutSeconds, const double PhaseTimeoutSeconds, const double TotalTimeoutSeconds, const double CaseStartSeconds)
{
	const double PhaseStartSeconds = FPlatformTime::Seconds();
	OutFailureContext = FUESpecTestFailureContext();

	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		const TSharedPtr<FJsonObject>* StepObject = nullptr;
		if (!Steps[StepIndex].IsValid() || !Steps[StepIndex]->TryGetObject(StepObject) || !StepObject || !(*StepObject).IsValid())
		{
			OutFailureMessage = FString::Printf(TEXT("%s step %d is not a JSON object."), *PhaseName, StepIndex);
			SetFailureContext(&OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
			FinalizeFailureContext(RootWidget, SpecPath, FString::Printf(TEXT("%s[%d] <invalid-step>"), *PhaseName, StepIndex), OutFailureMessage, OutFailureContext);
			AppendLogLine(OutReport, FString::Printf(TEXT("[FAIL] %s %s[%d] <invalid-step>: %s"), *OutResult.Name, *PhaseName, StepIndex, *OutFailureMessage));
			return false;
		}

		bool bShouldExecute = true;
		const FString StepDescription = NormalizeStepDescriptionForCase(DescribeStep(*StepObject), CaseName, bShouldExecute);
		if (!bShouldExecute)
		{
			continue;
		}

		++OutResult.ExecutedSteps;
		const double StepStartSeconds = FPlatformTime::Seconds();
		FUESpecTestFailureContext StepFailureContext;
		if (!ExecuteStep(RootWidget, *StepObject, OutFailureMessage, &StepFailureContext))
		{
			OutFailureContext = StepFailureContext;
			FinalizeFailureContext(RootWidget, SpecPath, StepDescription, OutFailureMessage, OutFailureContext);
			AppendLogLine(OutReport, FString::Printf(TEXT("[FAIL] %s %s[%d] %s: %s"), *OutResult.Name, *PhaseName, StepIndex, *StepDescription, *OutFailureMessage));
			return false;
		}

		if (HasTimedOut(StepStartSeconds, StepTimeoutSeconds))
		{
			OutFailureMessage = FString::Printf(TEXT("%s step %d exceeded the step timeout of %.3fs."), *PhaseName, StepIndex, StepTimeoutSeconds);
			SetFailureContext(&OutFailureContext, TEXT("TIMEOUT"), OutFailureMessage);
			FinalizeFailureContext(RootWidget, SpecPath, StepDescription, OutFailureMessage, OutFailureContext);
			AppendLogLine(OutReport, FString::Printf(TEXT("[FAIL] %s %s[%d] %s: %s"), *OutResult.Name, *PhaseName, StepIndex, *StepDescription, *OutFailureMessage));
			return false;
		}

		AppendLogLine(OutReport, FString::Printf(TEXT("[PASS] %s %s[%d] %s"), *OutResult.Name, *PhaseName, StepIndex, *StepDescription));

		if (HasTimedOut(PhaseStartSeconds, PhaseTimeoutSeconds))
		{
			OutFailureMessage = FString::Printf(TEXT("%s exceeded the phase timeout of %.3fs."), *PhaseName, PhaseTimeoutSeconds);
			SetFailureContext(&OutFailureContext, TEXT("TIMEOUT"), OutFailureMessage);
			FinalizeFailureContext(RootWidget, SpecPath, StepDescription, OutFailureMessage, OutFailureContext);
			AppendLogLine(OutReport, FString::Printf(TEXT("[FAIL] %s %s[%d] %s: %s"), *OutResult.Name, *PhaseName, StepIndex, *StepDescription, *OutFailureMessage));
			return false;
		}

		if (HasTimedOut(CaseStartSeconds, TotalTimeoutSeconds))
		{
			OutFailureMessage = FString::Printf(TEXT("test case exceeded the total timeout of %.3fs."), TotalTimeoutSeconds);
			SetFailureContext(&OutFailureContext, TEXT("TIMEOUT"), OutFailureMessage);
			FinalizeFailureContext(RootWidget, SpecPath, StepDescription, OutFailureMessage, OutFailureContext);
			AppendLogLine(OutReport, FString::Printf(TEXT("[FAIL] %s %s[%d] %s: %s"), *OutResult.Name, *PhaseName, StepIndex, *StepDescription, *OutFailureMessage));
			return false;
		}
	}

	return true;
}

bool FUESpecTestRunner::ExecuteStep(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& StepObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!StepObject.IsValid())
	{
		OutFailureMessage = TEXT("testStep must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString WidgetId;
	if (StepObject->TryGetStringField(TEXT("click"), WidgetId))
	{
		return ExecuteClick(RootWidget, WidgetId, OutFailureMessage, OutFailureContext);
	}

	if (StepObject->TryGetStringField(TEXT("hover"), WidgetId))
	{
		return ExecuteHover(RootWidget, WidgetId, OutFailureMessage, OutFailureContext);
	}

	if (StepObject->TryGetStringField(TEXT("focus"), WidgetId))
	{
		return ExecuteFocus(RootWidget, WidgetId, OutFailureMessage, OutFailureContext);
	}

	FString SignalName;
	if (StepObject->TryGetStringField(TEXT("emitSignal"), SignalName))
	{
		return ExecuteEmitSignal(RootWidget, SignalName, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* NavigateKeyObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("navigateKey"), NavigateKeyObject) && NavigateKeyObject && (*NavigateKeyObject).IsValid())
	{
		return ExecuteNavigateKey(RootWidget, *NavigateKeyObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* SetVMFieldObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("setVMField"), SetVMFieldObject) && SetVMFieldObject && (*SetVMFieldObject).IsValid())
	{
		return ExecuteSetVMField(RootWidget, *SetVMFieldObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* AssertVisibleObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertVisible"), AssertVisibleObject) && AssertVisibleObject && (*AssertVisibleObject).IsValid())
	{
		bool bExpected = true;
		(*AssertVisibleObject)->TryGetBoolField(TEXT("expected"), bExpected);
		if (!(*AssertVisibleObject)->TryGetStringField(TEXT("id"), WidgetId) || WidgetId.IsEmpty())
		{
			OutFailureMessage = TEXT("assertVisible requires a non-empty id.");
			SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
			return false;
		}

		return ExecuteAssertVisible(RootWidget, WidgetId, bExpected, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* WaitForVisibleObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("waitForVisible"), WaitForVisibleObject) && WaitForVisibleObject && (*WaitForVisibleObject).IsValid())
	{
		return ExecuteWaitForVisible(RootWidget, *WaitForVisibleObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* AssertTextObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertText"), AssertTextObject) && AssertTextObject && (*AssertTextObject).IsValid())
	{
		return ExecuteAssertText(RootWidget, *AssertTextObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* AssertStyleObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertStyle"), AssertStyleObject) && AssertStyleObject && (*AssertStyleObject).IsValid())
	{
		return ExecuteAssertStyle(RootWidget, *AssertStyleObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* AssertStateObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertState"), AssertStateObject) && AssertStateObject && (*AssertStateObject).IsValid())
	{
		return ExecuteAssertState(RootWidget, *AssertStateObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* AssertFocusObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertFocus"), AssertFocusObject) && AssertFocusObject && (*AssertFocusObject).IsValid())
	{
		return ExecuteAssertFocus(RootWidget, *AssertFocusObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* ScreenshotObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("screenshot"), ScreenshotObject) && ScreenshotObject && (*ScreenshotObject).IsValid())
	{
		return ExecuteScreenshot(RootWidget, *ScreenshotObject, OutFailureMessage, OutFailureContext);
	}

	const TSharedPtr<FJsonObject>* WaitForStateObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("waitForState"), WaitForStateObject) && WaitForStateObject && (*WaitForStateObject).IsValid())
	{
		return ExecuteWaitForState(RootWidget, *WaitForStateObject, OutFailureMessage, OutFailureContext);
	}

	OutFailureMessage = TEXT("Unsupported test step verb.");
	SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
	return false;
}

bool FUESpecTestRunner::ExecuteClick(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for click."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender())
	{
		return TryExecuteHeadlessClick(*Widget, WidgetId, OutFailureMessage, OutFailureContext);
	}

	TSharedPtr<SWidget> SlateWidget = ResolveSlateWidget(*Widget);
	if (!SlateWidget.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' has no Slate widget."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
		return false;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const TSharedPtr<SWindow> Window = SlateApplication.FindWidgetWindow(SlateWidget.ToSharedRef());
	if (!Window.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' is not attached to a Slate path."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
		return false;
	}

	const TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
	const FGeometry Geometry = SlateWidget->GetTickSpaceGeometry();
	const FVector2D ScreenPosition = Geometry.GetAbsolutePosition() + (Geometry.GetLocalSize() * 0.5f);
	const FModifierKeysState Modifiers = SlateApplication.GetModifierKeys();
	TSet<FKey> PressedButtons;
	PressedButtons.Add(EKeys::LeftMouseButton);

	SlateApplication.ProcessApplicationActivationEvent(true);
	HoverWidget(*Widget, OutFailureMessage);
	SlateApplication.SetKeyboardFocus(SlateWidget, EFocusCause::SetDirectly);

	const FPointerEvent MouseDownEvent(
		SlateApplication.GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		ScreenPosition,
		ScreenPosition,
		PressedButtons,
		EKeys::LeftMouseButton,
		0.0f,
		Modifiers);

	const bool bPressed = SlateApplication.ProcessMouseButtonDownEvent(NativeWindow, MouseDownEvent);

	const FPointerEvent MouseUpEvent(
		SlateApplication.GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		ScreenPosition,
		ScreenPosition,
		TSet<FKey>(),
		EKeys::LeftMouseButton,
		0.0f,
		Modifiers);

	const bool bReleased = SlateApplication.ProcessMouseButtonUpEvent(MouseUpEvent);

	if (!bPressed && !bReleased)
	{
		OutFailureMessage = FString::Printf(TEXT("Slate click on widget '%s' was not handled."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteHover(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for hover."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	return HoverWidget(*Widget, OutFailureMessage);
}

bool FUESpecTestRunner::ExecuteFocus(UUserWidget& RootWidget, const FString& WidgetId, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for focus."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	return FocusWidget(*Widget, OutFailureMessage);
}

bool FUESpecTestRunner::ExecuteNavigateKey(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& NavigateKeyObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!NavigateKeyObject.IsValid())
	{
		OutFailureMessage = TEXT("navigateKey must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString KeyName;
	if (!NavigateKeyObject->TryGetStringField(TEXT("key"), KeyName) || KeyName.IsEmpty())
	{
		OutFailureMessage = TEXT("navigateKey requires a non-empty key.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	const FKey Key = ResolveNavigationKey(KeyName);
	if (!Key.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("navigateKey key '%s' is not supported."), *KeyName);
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
	{
		OutFailureMessage = TEXT("navigateKey requires a focused widget.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const FModifierKeysState Modifiers = SlateApplication.GetModifierKeys();
	const FKeyEvent KeyDownEvent(Key, Modifiers, SlateApplication.GetUserIndexForKeyboard(), false, 0, 0);
	const bool bHandledDown = SlateApplication.ProcessKeyDownEvent(KeyDownEvent);

	const FKeyEvent KeyUpEvent(Key, Modifiers, SlateApplication.GetUserIndexForKeyboard(), false, 0, 0);
	const bool bHandledUp = SlateApplication.ProcessKeyUpEvent(KeyUpEvent);
	if (!bHandledDown && !bHandledUp)
	{
		OutFailureMessage = FString::Printf(TEXT("navigateKey '%s' was not handled."), *KeyName);
		SetFailureContext(OutFailureContext, TEXT("SLATE_EVENT_NOT_HANDLED"), OutFailureMessage);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteEmitSignal(UUserWidget& RootWidget, const FString& SignalName, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (SignalName.IsEmpty())
	{
		OutFailureMessage = TEXT("emitSignal requires a non-empty signal name.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	if (!RouteSignalToRootWidget(RootWidget, FName(*SignalName)))
	{
		OutFailureMessage = FString::Printf(TEXT("Signal '%s' could not be routed to the root widget."), *SignalName);
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteSetVMField(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& SetFieldObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!SetFieldObject.IsValid())
	{
		OutFailureMessage = TEXT("setVMField must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString FieldPath;
	if (!SetFieldObject->TryGetStringField(TEXT("field"), FieldPath) || FieldPath.IsEmpty())
	{
		OutFailureMessage = TEXT("setVMField requires a non-empty field path.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	const TSharedPtr<FJsonValue>* ValueObjectPtr = SetFieldObject->Values.Find(TEXT("value"));
	const TSharedPtr<FJsonValue> ValueObject = ValueObjectPtr ? *ValueObjectPtr : TSharedPtr<FJsonValue>();
	if (!ValueObject.IsValid())
	{
		OutFailureMessage = TEXT("setVMField requires a value.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	UUESpecViewModelBase* const ViewModel = GetRootViewModel(RootWidget);
	if (!ViewModel)
	{
		OutFailureMessage = TEXT("setVMField requires the root widget to expose a UESpec viewmodel.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	return SetObjectPathValue(*ViewModel, FieldPath, ValueObject, OutFailureMessage);
}

bool FUESpecTestRunner::ExecuteAssertVisible(UUserWidget& RootWidget, const FString& WidgetId, const bool bExpected, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for assertVisible."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	const bool bIsVisible = IsWidgetVisibleForTesting(*Widget);
	if (bIsVisible != bExpected)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' visibility was %s, expected %s."), *WidgetId, bIsVisible ? TEXT("true") : TEXT("false"), bExpected ? TEXT("true") : TEXT("false"));
		SetFailureContext(
			OutFailureContext,
			TEXT("ASSERTION_FAILED"),
			OutFailureMessage,
			bExpected ? TEXT("true") : TEXT("false"),
			bIsVisible ? TEXT("true") : TEXT("false"));
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteWaitForVisible(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& WaitForVisibleObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!WaitForVisibleObject.IsValid())
	{
		OutFailureMessage = TEXT("waitForVisible must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString WidgetId;
	if (!WaitForVisibleObject->TryGetStringField(TEXT("id"), WidgetId) || WidgetId.IsEmpty())
	{
		OutFailureMessage = TEXT("waitForVisible requires a non-empty id.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	double TimeoutSeconds = 0.0;
	WaitForVisibleObject->TryGetNumberField(TEXT("timeout"), TimeoutSeconds);

	bool bExpected = true;
	WaitForVisibleObject->TryGetBoolField(TEXT("expected"), bExpected);

	const bool bVisible = WaitUntil([&RootWidget, &WidgetId, bExpected]()
	{
		if (UWidget* const Widget = FindWidgetById(RootWidget, WidgetId))
		{
			return IsWidgetVisibleForTesting(*Widget) == bExpected;
		}

		return false;
	}, TimeoutSeconds);

	if (!bVisible)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' did not reach visible=%s within %.3fs."), *WidgetId, bExpected ? TEXT("true") : TEXT("false"), TimeoutSeconds);
		SetFailureContext(
			OutFailureContext,
			TEXT("TIMEOUT"),
			OutFailureMessage,
			bExpected ? TEXT("true") : TEXT("false"),
			TEXT("not-reached"),
			{ WidgetId });
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteAssertText(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertTextObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!AssertTextObject.IsValid())
	{
		OutFailureMessage = TEXT("assertText must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString WidgetId;
	if (!AssertTextObject->TryGetStringField(TEXT("id"), WidgetId) || WidgetId.IsEmpty())
	{
		OutFailureMessage = TEXT("assertText requires a non-empty id.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString ExpectedText;
	if (!AssertTextObject->TryGetStringField(TEXT("expected"), ExpectedText))
	{
		OutFailureMessage = TEXT("assertText requires an expected string.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString MatchMode = TEXT("Exact");
	AssertTextObject->TryGetStringField(TEXT("match"), MatchMode);

	UWidget* const Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for assertText."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	FString ActualText;
	if (!TryGetWidgetText(*Widget, ActualText))
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' does not expose text for assertText."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedText, TEXT("<unsupported>"));
		return false;
	}

	if (!IsTextMatch(ActualText, ExpectedText, MatchMode))
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' text was '%s', expected '%s' (%s)."), *WidgetId, *ActualText, *ExpectedText, *MatchMode);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedText, ActualText);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteAssertStyle(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertStyleObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!AssertStyleObject.IsValid())
	{
		OutFailureMessage = TEXT("assertStyle must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString WidgetId;
	if (!AssertStyleObject->TryGetStringField(TEXT("id"), WidgetId) || WidgetId.IsEmpty())
	{
		OutFailureMessage = TEXT("assertStyle requires a non-empty id.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString PropertyName;
	if (!AssertStyleObject->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
	{
		OutFailureMessage = TEXT("assertStyle requires a property.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	const TSharedPtr<FJsonValue>* ExpectedValueObjectPtr = AssertStyleObject->Values.Find(TEXT("expected"));
	const TSharedPtr<FJsonValue> ExpectedValueObject = ExpectedValueObjectPtr ? *ExpectedValueObjectPtr : TSharedPtr<FJsonValue>();
	if (!ExpectedValueObject.IsValid())
	{
		OutFailureMessage = TEXT("assertStyle requires an expected value.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	UWidget* const Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' was not found for assertStyle."), *WidgetId);
		SetFailureContext(OutFailureContext, TEXT("WIDGET_MISSING"), OutFailureMessage, FString(), FString(), { WidgetId });
		return false;
	}

	FString ActualValue;
	if (!TryGetWidgetStyleValue(*Widget, PropertyName, ActualValue))
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' does not expose style property '%s'."), *WidgetId, *PropertyName);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, JsonValueToString(ExpectedValueObject), TEXT("<unsupported>"));
		return false;
	}

	FString ExpectedValue = JsonValueToString(ExpectedValueObject);
	if (!ResolveStyleToken(ExpectedValue, ExpectedValue))
	{
		OutFailureMessage = FString::Printf(TEXT("Style token '%s' is not registered."), *JsonValueToString(ExpectedValueObject));
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, JsonValueToString(ExpectedValueObject), ActualValue);
		return false;
	}

	if (IsOpacityProperty(PropertyName))
	{
		double ActualNumber = 0.0;
		double ExpectedNumber = 0.0;
		if (!LexTryParseString(ActualNumber, *ActualValue) || !LexTryParseString(ExpectedNumber, *ExpectedValue) || !FMath::IsNearlyEqual(ActualNumber, ExpectedNumber))
		{
			OutFailureMessage = FString::Printf(TEXT("Widget '%s' style '%s' was '%s', expected '%s'."), *WidgetId, *PropertyName, *ActualValue, *ExpectedValue);
			SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedValue, ActualValue);
			return false;
		}

		return true;
	}

	if (IsVisibilityProperty(PropertyName))
	{
		ESlateVisibility ActualVisibility = ESlateVisibility::Visible;
		ESlateVisibility ExpectedVisibility = ESlateVisibility::Visible;
		if (!TryParseVisibility(ActualValue, ActualVisibility) || !TryParseVisibility(ExpectedValue, ExpectedVisibility) || ActualVisibility != ExpectedVisibility)
		{
			OutFailureMessage = FString::Printf(TEXT("Widget '%s' visibility was '%s', expected '%s'."), *WidgetId, *ActualValue, *ExpectedValue);
			SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedValue, ActualValue);
			return false;
		}

		return true;
	}

	if (IsColorProperty(PropertyName))
	{
		FLinearColor ActualColor = FLinearColor::Transparent;
		FLinearColor ExpectedColor = FLinearColor::Transparent;
		if (!TryParseColor(ActualValue, ActualColor) || !TryParseColor(ExpectedValue, ExpectedColor) || !ActualColor.Equals(ExpectedColor))
		{
			OutFailureMessage = FString::Printf(TEXT("Widget '%s' style '%s' was '%s', expected '%s'."), *WidgetId, *PropertyName, *ActualValue, *ExpectedValue);
			SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedValue, ActualValue);
			return false;
		}

		return true;
	}

	if (ActualValue != ExpectedValue)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' style '%s' was '%s', expected '%s'."), *WidgetId, *PropertyName, *ActualValue, *ExpectedValue);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedValue, ActualValue);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteAssertState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertStateObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!AssertStateObject.IsValid())
	{
		OutFailureMessage = TEXT("assertState must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString ExpectedState;
	if (!AssertStateObject->TryGetStringField(TEXT("expected"), ExpectedState) || ExpectedState.IsEmpty())
	{
		OutFailureMessage = TEXT("assertState requires an expected state.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FName CurrentState = NAME_None;
	if (!TryGetWidgetState(RootWidget, CurrentState))
	{
		OutFailureMessage = TEXT("assertState requires the root widget to implement UESpec state APIs.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	if (CurrentState.IsNone())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget state was 'None', expected '%s'."), *ExpectedState);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedState, TEXT("None"));
		return false;
	}

	if (CurrentState != FName(*ExpectedState))
	{
		OutFailureMessage = FString::Printf(TEXT("Widget state was '%s', expected '%s'."), *CurrentState.ToString(), *ExpectedState);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedState, CurrentState.ToString());
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteAssertFocus(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& AssertFocusObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!AssertFocusObject.IsValid())
	{
		OutFailureMessage = TEXT("assertFocus must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString ExpectedWidgetId;
	if (!AssertFocusObject->TryGetStringField(TEXT("expected"), ExpectedWidgetId) || ExpectedWidgetId.IsEmpty())
	{
		OutFailureMessage = TEXT("assertFocus requires an expected widget id.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
	{
		OutFailureMessage = TEXT("No widget currently has keyboard focus.");
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedWidgetId, TEXT("<none>"));
		return false;
	}

	UWidget* const FocusedUMGWidget = FindWidgetBySlate(RootWidget, FocusedWidget);
	if (!FocusedUMGWidget)
	{
		OutFailureMessage = TEXT("The focused Slate widget is not owned by the current root widget.");
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedWidgetId, TEXT("<external>"));
		return false;
	}

	if (FocusedUMGWidget->GetName() != ExpectedWidgetId)
	{
		OutFailureMessage = FString::Printf(TEXT("Focused widget was '%s', expected '%s'."), *FocusedUMGWidget->GetName(), *ExpectedWidgetId);
		SetFailureContext(OutFailureContext, TEXT("ASSERTION_FAILED"), OutFailureMessage, ExpectedWidgetId, FocusedUMGWidget->GetName());
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteScreenshot(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& ScreenshotObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!ScreenshotObject.IsValid())
	{
		OutFailureMessage = TEXT("screenshot must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString ScreenshotName;
	ScreenshotObject->TryGetStringField(TEXT("name"), ScreenshotName);
	const FString ScreenshotPath = BuildScreenshotPath(ScreenshotName);

	TSharedPtr<SWidget> SlateWidget = ResolveSlateWidget(RootWidget);
	if (!SlateWidget.IsValid())
	{
		OutFailureMessage = TEXT("The root widget has no Slate widget for screenshot capture.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	TArray<FColor> ImageData;
	FIntVector ImageSize;
	if (!FSlateApplication::Get().TakeScreenshot(SlateWidget.ToSharedRef(), ImageData, ImageSize))
	{
		ImageData = { FColor::Transparent };
		ImageSize = FIntVector(1, 1, 1);
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);

	TArray64<uint8> CompressedBitmap;
	FImageUtils::PNGCompressImageArray(ImageSize.X, ImageSize.Y, TArrayView64<const FColor>(ImageData.GetData(), ImageData.Num()), CompressedBitmap);
	if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *ScreenshotPath))
	{
		OutFailureMessage = FString::Printf(TEXT("Failed to write screenshot '%s'."), *ScreenshotPath);
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ExecuteWaitForState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& WaitForStateObject, FString& OutFailureMessage, FUESpecTestFailureContext* OutFailureContext)
{
	if (!WaitForStateObject.IsValid())
	{
		OutFailureMessage = TEXT("waitForState must be a JSON object.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	FString StateName;
	if (!WaitForStateObject->TryGetStringField(TEXT("state"), StateName) || StateName.IsEmpty())
	{
		OutFailureMessage = TEXT("waitForState requires a non-empty state.");
		SetFailureContext(OutFailureContext, TEXT("UNKNOWN"), OutFailureMessage);
		return false;
	}

	double TimeoutSeconds = 0.0;
	WaitForStateObject->TryGetNumberField(TEXT("timeout"), TimeoutSeconds);

	const bool bStateReached = WaitUntil([&RootWidget, &StateName]()
	{
		FName CurrentState = NAME_None;
		const bool bHasStateApis = TryGetWidgetState(RootWidget, CurrentState);
		return !bHasStateApis || CurrentState.IsNone() || CurrentState == FName(*StateName);
	}, TimeoutSeconds);

	if (!bStateReached)
	{
		FName CurrentState = NAME_None;
		TryGetWidgetState(RootWidget, CurrentState);
		OutFailureMessage = FString::Printf(TEXT("Widget did not reach state '%s' within %.3fs."), *StateName, TimeoutSeconds);
		SetFailureContext(OutFailureContext, TEXT("TIMEOUT"), OutFailureMessage, StateName, CurrentState.ToString());
		return false;
	}

	return true;
}

bool FUESpecTestRunner::ApplyParameterViewModelState(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& ParameterObject, FString& OutFailureMessage)
{
	if (!ParameterObject.IsValid())
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* VmStateObject = nullptr;
	if (!ParameterObject->TryGetObjectField(TEXT("vmState"), VmStateObject) || !VmStateObject || !(*VmStateObject).IsValid())
	{
		return true;
	}

	UUESpecViewModelBase* const ViewModel = GetRootViewModel(RootWidget);
	if (!ViewModel)
	{
		return true;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*(*VmStateObject)))
	{
		if (!SetObjectPathValue(*ViewModel, Pair.Key, Pair.Value, OutFailureMessage))
		{
			OutFailureMessage = FString::Printf(TEXT("Failed to apply vmState '%s': %s"), *Pair.Key, *OutFailureMessage);
			return false;
		}
	}

	if (UFunction* const SyncFunction = ViewModel->FindFunction(TEXT("SyncPrimarySkillState")))
	{
		ViewModel->ProcessEvent(SyncFunction, nullptr);
	}

	return true;
}

bool FUESpecTestRunner::ApplyJsonValueToProperty(UObject* OwnerObject, FProperty& Property, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutFailureMessage)
{
	if (FBoolProperty* const BoolProperty = CastField<FBoolProperty>(&Property))
	{
		if (!Value.IsValid())
		{
			OutFailureMessage = TEXT("boolean value is missing.");
			return false;
		}

		if (Value->Type == EJson::Boolean)
		{
			BoolProperty->SetPropertyValue(ValuePtr, Value->AsBool());
			return true;
		}

		const FString StringValue = JsonValueToString(Value);
		if (StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StringValue == TEXT("1"))
		{
			BoolProperty->SetPropertyValue(ValuePtr, true);
			return true;
		}
		if (StringValue.Equals(TEXT("false"), ESearchCase::IgnoreCase) || StringValue == TEXT("0"))
		{
			BoolProperty->SetPropertyValue(ValuePtr, false);
			return true;
		}

		OutFailureMessage = FString::Printf(TEXT("value '%s' is not a boolean."), *StringValue);
		return false;
	}

	if (FIntProperty* const IntProperty = CastField<FIntProperty>(&Property))
	{
		double NumberValue = 0.0;
		if (!TryGetNumberValue(Value, NumberValue))
		{
			OutFailureMessage = FString::Printf(TEXT("value '%s' is not an integer."), *JsonValueToString(Value));
			return false;
		}

		IntProperty->SetPropertyValue(ValuePtr, static_cast<int32>(NumberValue));
		return true;
	}

	if (FFloatProperty* const FloatProperty = CastField<FFloatProperty>(&Property))
	{
		double NumberValue = 0.0;
		if (!TryGetNumberValue(Value, NumberValue))
		{
			OutFailureMessage = FString::Printf(TEXT("value '%s' is not a float."), *JsonValueToString(Value));
			return false;
		}

		FloatProperty->SetPropertyValue(ValuePtr, static_cast<float>(NumberValue));
		return true;
	}

	if (FDoubleProperty* const DoubleProperty = CastField<FDoubleProperty>(&Property))
	{
		double NumberValue = 0.0;
		if (!TryGetNumberValue(Value, NumberValue))
		{
			OutFailureMessage = FString::Printf(TEXT("value '%s' is not a double."), *JsonValueToString(Value));
			return false;
		}

		DoubleProperty->SetPropertyValue(ValuePtr, NumberValue);
		return true;
	}

	if (FStrProperty* const StringProperty = CastField<FStrProperty>(&Property))
	{
		StringProperty->SetPropertyValue(ValuePtr, JsonValueToString(Value));
		return true;
	}

	if (FNameProperty* const NameProperty = CastField<FNameProperty>(&Property))
	{
		NameProperty->SetPropertyValue(ValuePtr, FName(*JsonValueToString(Value)));
		return true;
	}

	if (FTextProperty* const TextProperty = CastField<FTextProperty>(&Property))
	{
		TextProperty->SetPropertyValue(ValuePtr, FText::FromString(JsonValueToString(Value)));
		return true;
	}

	OutFailureMessage = FString::Printf(TEXT("property '%s' on '%s' is not supported by setVMField."), *Property.GetName(), OwnerObject ? *OwnerObject->GetName() : TEXT("<unknown>"));
	return false;
}

bool FUESpecTestRunner::SetObjectPathValue(UObject& RootObject, const FString& FieldPath, const TSharedPtr<FJsonValue>& Value, FString& OutFailureMessage)
{
	TArray<FUESpecPathSegment> Segments;
	if (!ParseObjectPath(FieldPath, Segments, OutFailureMessage))
	{
		return false;
	}

	UObject* CurrentObject = &RootObject;
	void* ContainerPtr = &RootObject;
	UStruct* CurrentStruct = RootObject.GetClass();

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FUESpecPathSegment& Segment = Segments[SegmentIndex];
		FProperty* const Property = CurrentStruct ? FindPropertyBySpecName(*CurrentStruct, Segment.PropertyName) : nullptr;
		if (!Property)
		{
			OutFailureMessage = FString::Printf(TEXT("property '%s' was not found on '%s'."), *Segment.PropertyName, CurrentStruct ? *CurrentStruct->GetName() : TEXT("<unknown>"));
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		const bool bIsLastSegment = SegmentIndex == Segments.Num() - 1;

		if (Segment.ArrayIndex.IsSet())
		{
			FArrayProperty* const ArrayProperty = CastField<FArrayProperty>(Property);
			if (!ArrayProperty)
			{
				OutFailureMessage = FString::Printf(TEXT("property '%s' is not an array."), *Segment.PropertyName);
				return false;
			}

			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			if (!ArrayHelper.IsValidIndex(Segment.ArrayIndex.GetValue()))
			{
				OutFailureMessage = FString::Printf(TEXT("array property '%s' does not contain index %d."), *Segment.PropertyName, Segment.ArrayIndex.GetValue());
				return false;
			}

			void* const ElementPtr = ArrayHelper.GetRawPtr(Segment.ArrayIndex.GetValue());
			if (bIsLastSegment)
			{
				return ApplyJsonValueToProperty(CurrentObject, *ArrayProperty->Inner, ElementPtr, Value, OutFailureMessage);
			}

			if (FStructProperty* const StructInner = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				CurrentObject = nullptr;
				ContainerPtr = ElementPtr;
				CurrentStruct = StructInner->Struct;
				continue;
			}

			if (FObjectPropertyBase* const ObjectInner = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				UObject* const NextObject = ObjectInner->GetObjectPropertyValue(ElementPtr);
				if (!NextObject)
				{
					OutFailureMessage = FString::Printf(TEXT("array element '%s[%d]' is null."), *Segment.PropertyName, Segment.ArrayIndex.GetValue());
					return false;
				}

				CurrentObject = NextObject;
				ContainerPtr = NextObject;
				CurrentStruct = NextObject->GetClass();
				continue;
			}

			OutFailureMessage = FString::Printf(TEXT("array property '%s' does not support nested traversal."), *Segment.PropertyName);
			return false;
		}

		if (bIsLastSegment)
		{
			return ApplyJsonValueToProperty(CurrentObject, *Property, ValuePtr, Value, OutFailureMessage);
		}

		if (FStructProperty* const StructProperty = CastField<FStructProperty>(Property))
		{
			CurrentObject = nullptr;
			ContainerPtr = ValuePtr;
			CurrentStruct = StructProperty->Struct;
			continue;
		}

		if (FObjectPropertyBase* const ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* const NextObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);
			if (!NextObject)
			{
				OutFailureMessage = FString::Printf(TEXT("property '%s' is null."), *Segment.PropertyName);
				return false;
			}

			CurrentObject = NextObject;
			ContainerPtr = NextObject;
			CurrentStruct = NextObject->GetClass();
			continue;
		}

		OutFailureMessage = FString::Printf(TEXT("property '%s' does not support nested traversal."), *Segment.PropertyName);
		return false;
	}

	OutFailureMessage = FString::Printf(TEXT("field path '%s' could not be resolved."), *FieldPath);
	return false;
}

UUESpecViewModelBase* FUESpecTestRunner::GetRootViewModel(UUserWidget& RootWidget)
{
	if (UUESpecWidgetBase* const UESpecWidget = Cast<UUESpecWidgetBase>(&RootWidget))
	{
		return UESpecWidget->GetUESpecViewModel();
	}

	if (UUESpecActivatableWidgetBase* const ActivatableWidget = Cast<UUESpecActivatableWidgetBase>(&RootWidget))
	{
		return ActivatableWidget->GetUESpecViewModel();
	}

	return nullptr;
}

UWidget* FUESpecTestRunner::FindWidgetBySlate(UUserWidget& RootWidget, const TSharedPtr<SWidget>& SlateWidget)
{
	if (!SlateWidget.IsValid() || !RootWidget.WidgetTree)
	{
		return nullptr;
	}

	TQueue<UWidgetTree*> SearchQueue;
	SearchQueue.Enqueue(RootWidget.WidgetTree);

	while (!SearchQueue.IsEmpty())
	{
		UWidgetTree* CurrentTree = nullptr;
		SearchQueue.Dequeue(CurrentTree);
		if (!CurrentTree)
		{
			continue;
		}

		TArray<UWidget*> Widgets;
		CurrentTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (!Widget)
			{
				continue;
			}

			if (ResolveSlateWidget(*Widget) == SlateWidget)
			{
				return Widget;
			}

			if (UUserWidget* const ChildUserWidget = Cast<UUserWidget>(Widget); ChildUserWidget && ChildUserWidget->WidgetTree)
			{
				SearchQueue.Enqueue(ChildUserWidget->WidgetTree);
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FUESpecTestRunner::ResolveSlateWidget(UWidget& Widget)
{
	TSharedPtr<SWidget> SlateWidget = Widget.GetCachedWidget();
	if (!SlateWidget.IsValid())
	{
		SlateWidget = Widget.TakeWidget();
	}

	return SlateWidget;
}

bool FUESpecTestRunner::FocusWidget(UWidget& Widget, FString& OutFailureMessage)
{
	TSharedPtr<SWidget> SlateWidget = ResolveSlateWidget(Widget);
	if (!SlateWidget.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' has no Slate widget."), *Widget.GetName());
		return false;
	}

	FSlateApplication::Get().ProcessApplicationActivationEvent(true);
	FSlateApplication::Get().SetKeyboardFocus(SlateWidget, EFocusCause::SetDirectly);
	if (FSlateApplication::Get().GetKeyboardFocusedWidget() != SlateWidget)
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' did not receive keyboard focus."), *Widget.GetName());
		return false;
	}

	return true;
}

bool FUESpecTestRunner::HoverWidget(UWidget& Widget, FString& OutFailureMessage)
{
	TSharedPtr<SWidget> SlateWidget = ResolveSlateWidget(Widget);
	if (!SlateWidget.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' has no Slate widget."), *Widget.GetName());
		return false;
	}

	const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(SlateWidget.ToSharedRef());
	if (!Window.IsValid())
	{
		OutFailureMessage = FString::Printf(TEXT("Widget '%s' is not attached to a Slate path."), *Widget.GetName());
		return false;
	}

	const FGeometry Geometry = SlateWidget->GetTickSpaceGeometry();
	const FVector2D ScreenPosition = Geometry.GetAbsolutePosition() + (Geometry.GetLocalSize() * 0.5f);
	const FPointerEvent MouseMoveEvent(
		FSlateApplication::Get().GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		ScreenPosition,
		ScreenPosition,
		TSet<FKey>(),
		EKeys::Invalid,
		0.0f,
		FSlateApplication::Get().GetModifierKeys());

	FSlateApplication::Get().ProcessApplicationActivationEvent(true);
	FSlateApplication::Get().ProcessMouseMoveEvent(MouseMoveEvent);
	if (!FApp::CanEverRender())
	{
		if (UButton* const Button = Cast<UButton>(&Widget))
		{
			Button->OnHovered.Broadcast();
		}
	}

	return true;
}

bool FUESpecTestRunner::ResolveStyleToken(const FString& InValue, FString& OutResolvedValue)
{
	if (!InValue.StartsWith(TEXT("@")))
	{
		OutResolvedValue = InValue;
		return true;
	}

	if (!GEngine)
	{
		return false;
	}

	if (UUESpecSupportSurfaceSubsystem* const SupportSurfaceSubsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
	{
		const FUESpecSupportSurfaceSnapshot Snapshot = SupportSurfaceSubsystem->BuildSnapshot();
		if (const FUESpecSurfaceStyleTokenEntry* const Entry = Snapshot.StyleTokens.FindByPredicate([&InValue](const FUESpecSurfaceStyleTokenEntry& Candidate)
		{
			return Candidate.Name == InValue;
		}))
		{
			OutResolvedValue = Entry->Value;
			return true;
		}
	}

	return false;
}

bool FUESpecTestRunner::TryGetWidgetText(const UWidget& Widget, FString& OutText)
{
	if (const UTextBlock* const TextBlock = Cast<UTextBlock>(&Widget))
	{
		OutText = TextBlock->GetText().ToString();
		return true;
	}

	if (const URichTextBlock* const RichTextBlock = Cast<URichTextBlock>(&Widget))
	{
		OutText = RichTextBlock->GetText().ToString();
		return true;
	}

	if (const UCommonTextBlock* const CommonTextBlock = Cast<UCommonTextBlock>(&Widget))
	{
		OutText = CommonTextBlock->GetText().ToString();
		return true;
	}

	return false;
}

bool FUESpecTestRunner::TryGetWidgetStyleValue(const UWidget& Widget, const FString& PropertyName, FString& OutValue)
{
	if (IsOpacityProperty(PropertyName))
	{
		OutValue = LexToString(Widget.GetRenderOpacity());
		return true;
	}

	if (IsVisibilityProperty(PropertyName))
	{
		OutValue = StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget.GetVisibility()));
		return true;
	}

	if (IsColorProperty(PropertyName))
	{
		if (const UTextBlock* const TextBlock = Cast<UTextBlock>(&Widget))
		{
			OutValue = TextBlock->GetColorAndOpacity().GetSpecifiedColor().ToString();
			return true;
		}

		if (const UImage* const Image = Cast<UImage>(&Widget))
		{
			OutValue = Image->GetColorAndOpacity().ToString();
			return true;
		}

		if (const UProgressBar* const ProgressBar = Cast<UProgressBar>(&Widget))
		{
			OutValue = ProgressBar->GetFillColorAndOpacity().ToString();
			return true;
		}
	}

	return false;
}

FString FUESpecTestRunner::BuildScreenshotPath(const FString& ScreenshotName)
{
	FString BaseName = ScreenshotName.TrimStartAndEnd();
	if (BaseName.IsEmpty())
	{
		BaseName = FDateTime::UtcNow().ToString(TEXT("yyyyMMdd_HHmmss"));
	}

	BaseName.ReplaceInline(TEXT("/"), TEXT("_"));
	BaseName.ReplaceInline(TEXT("\\"), TEXT("_"));
	BaseName.ReplaceInline(TEXT(":"), TEXT("_"));

	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Screenshots"), BaseName + TEXT(".png"));
}

FString FUESpecTestRunner::NormalizeStepDescriptionForCase(const FString& StepDescription, const FString& CaseName, bool& bOutShouldExecute)
{
	bOutShouldExecute = true;

	if (!StepDescription.StartsWith(TEXT("[")))
	{
		return StepDescription;
	}

	const int32 ClosingBracketIndex = StepDescription.Find(TEXT("]"));
	if (ClosingBracketIndex == INDEX_NONE)
	{
		return StepDescription;
	}

	const FString TaggedCaseName = StepDescription.Mid(1, ClosingBracketIndex - 1);
	if (!TaggedCaseName.Equals(CaseName, ESearchCase::IgnoreCase))
	{
		bOutShouldExecute = false;
		return StepDescription;
	}

	return StepDescription.Mid(ClosingBracketIndex + 1).TrimStartAndEnd();
}

UWidget* FUESpecTestRunner::FindWidgetById(UUserWidget& RootWidget, const FString& WidgetId)
{
	if (!RootWidget.WidgetTree)
	{
		return nullptr;
	}

	TQueue<UWidgetTree*> SearchQueue;
	SearchQueue.Enqueue(RootWidget.WidgetTree);

	while (!SearchQueue.IsEmpty())
	{
		UWidgetTree* CurrentTree = nullptr;
		SearchQueue.Dequeue(CurrentTree);
		if (!CurrentTree)
		{
			continue;
		}

		if (UWidget* FoundWidget = CurrentTree->FindWidget(FName(*WidgetId)))
		{
			return FoundWidget;
		}

		TArray<UWidget*> Widgets;
		CurrentTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget); ChildUserWidget && ChildUserWidget->WidgetTree)
			{
				SearchQueue.Enqueue(ChildUserWidget->WidgetTree);
			}
		}
	}

	return nullptr;
}

FString FUESpecTestRunner::BuildDefaultJUnitPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("TestReports"), TEXT("UESpecTestRunner.junit.xml"));
}

FString FUESpecTestRunner::BuildXmlEscape(const FString& InValue)
{
	FString OutValue = InValue;
	OutValue.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	OutValue.ReplaceInline(TEXT("\""), TEXT("&quot;"));
	OutValue.ReplaceInline(TEXT("'"), TEXT("&apos;"));
	OutValue.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	OutValue.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	return OutValue;
}

FString FUESpecTestRunner::StatusToJUnitString(const EUESpecTestStatus Status)
{
	switch (Status)
	{
	case EUESpecTestStatus::Pass:
		return TEXT("PASS");
	case EUESpecTestStatus::Skip:
		return TEXT("SKIP");
	default:
		return TEXT("FAIL");
	}
}

void FUESpecTestRunner::AppendLogLine(FUESpecTestRunReport& Report, const FString& Line)
{
	Report.LogLines.Add(Line);
	UE_LOG(LogUESpecTestRunner, Display, TEXT("%s"), *Line);
}

void FUESpecTestRunner::WriteJUnitReport(const FUESpecTestRunReport& Report)
{
	const FString Directory = FPaths::GetPath(Report.JUnitReportPath);
	IFileManager::Get().MakeDirectory(*Directory, true);

	int32 FailureCount = 0;
	int32 SkipCount = 0;
	for (const FUESpecTestResult& Result : Report.Results)
	{
		if (Result.Status == EUESpecTestStatus::Fail)
		{
			++FailureCount;
		}
		else if (Result.Status == EUESpecTestStatus::Skip)
		{
			++SkipCount;
		}
	}

	FString Xml;
	Xml += TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	Xml += FString::Printf(TEXT("<testsuite name=\"UESpecTestRunner\" tests=\"%d\" failures=\"%d\" skipped=\"%d\">\n"), Report.Results.Num(), FailureCount, SkipCount);

	for (const FUESpecTestResult& Result : Report.Results)
	{
		Xml += FString::Printf(
			TEXT("  <testcase name=\"%s\" time=\"%0.3f\">"),
			*BuildXmlEscape(Result.Name),
			Result.DurationSec);

		if (Result.Status == EUESpecTestStatus::Fail)
		{
			Xml += FString::Printf(TEXT("<failure message=\"%s\" />"), *BuildXmlEscape(Result.FailureMessage));
		}
		else if (Result.Status == EUESpecTestStatus::Skip)
		{
			Xml += FString::Printf(TEXT("<skipped message=\"%s\" />"), *BuildXmlEscape(Result.FailureMessage));
		}

		Xml += TEXT("</testcase>\n");
	}

	Xml += TEXT("</testsuite>\n");
	FFileHelper::SaveStringToFile(Xml, *Report.JUnitReportPath);
}
