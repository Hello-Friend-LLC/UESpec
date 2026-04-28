#include "TestSpec/UESpecTestSpecExecutor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"

namespace
{
	static void PopulateError(FUESpecCompilerError& OutError, const FString& Code, const FString& Path, const FString& Message, const FString& Hint)
	{
		OutError.Code = Code;
		OutError.Path = Path;
		OutError.Message = Message;
		OutError.Hint = Hint;
	}
}

FUESpecTestExecutionResult FUESpecTestSpecExecutor::ExecuteTestSpec(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& TestSpecObject)
{
	FUESpecTestExecutionResult Result;

	if (!TestSpecObject.IsValid())
	{
		AddError(Result, TEXT("INVALID_TEST_SPEC"), TEXT("testSpec"), TEXT("testSpec must be a JSON object."), TEXT("Provide a valid testSpec object."));
		return Result;
	}

	const TSharedPtr<FJsonObject>* PhasesObject = nullptr;
	if (!TestSpecObject->TryGetObjectField(TEXT("phases"), PhasesObject) || !PhasesObject || !(*PhasesObject).IsValid())
	{
		AddError(Result, TEXT("MISSING_TEST_PHASES"), TEXT("testSpec.phases"), TEXT("testSpec must declare phases."), TEXT("Add assume/arrange/act/assert arrays."));
		return Result;
	}

	for (const FString& PhaseName : { TEXT("assume"), TEXT("arrange"), TEXT("act"), TEXT("assert") })
	{
		const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
		if (!(*PhasesObject)->TryGetArrayField(PhaseName, StepsArray) || !StepsArray)
		{
			continue;
		}

		for (int32 StepIndex = 0; StepIndex < StepsArray->Num(); ++StepIndex)
		{
			const TSharedPtr<FJsonObject>* StepObject = nullptr;
			if (!(*StepsArray)[StepIndex].IsValid() || !(*StepsArray)[StepIndex]->TryGetObject(StepObject) || !StepObject || !(*StepObject).IsValid())
			{
				AddError(
					Result,
					TEXT("INVALID_TEST_STEP"),
					FString::Printf(TEXT("testSpec.phases.%s[%d]"), *PhaseName, StepIndex),
					TEXT("test steps must be JSON objects."),
					TEXT("Replace the step with a valid step object."));
				continue;
			}

			FUESpecCompilerError StepError;
			if (!ExecuteStep(RootWidget, *StepObject, StepError))
			{
				Result.Errors.Add(MoveTemp(StepError));
				continue;
			}

			++Result.ExecutedSteps;
		}
	}

	Result.bOk = Result.Errors.Num() == 0;
	return Result;
}

bool FUESpecTestSpecExecutor::ExecuteClick(UUserWidget& RootWidget, const FString& WidgetId, FUESpecCompilerError& OutError)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		PopulateError(
			OutError,
			TEXT("TEST_WIDGET_NOT_FOUND"),
			WidgetId,
			FString::Printf(TEXT("Widget '%s' was not found for click."), *WidgetId),
			TEXT("Check the widget id and ensure the widget exists in the instantiated tree."));
		return false;
	}

	TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	if (!SlateWidget.IsValid())
	{
		SlateWidget = Widget->TakeWidget();
	}

	if (!SlateWidget.IsValid())
	{
		PopulateError(
			OutError,
			TEXT("TEST_WIDGET_NO_SLATE"),
			WidgetId,
			FString::Printf(TEXT("Widget '%s' has no cached Slate widget."), *WidgetId),
			TEXT("Ensure the widget has been constructed and attached to a window before clicking."));
		return false;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	FWidgetPath WidgetPath;
	if (!SlateApplication.GeneratePathToWidgetUnchecked(SlateWidget.ToSharedRef(), WidgetPath))
	{
		PopulateError(
			OutError,
			TEXT("TEST_WIDGET_NO_PATH"),
			WidgetId,
			FString::Printf(TEXT("Widget '%s' is not attached to a Slate window path."), *WidgetId),
			TEXT("Attach the widget to a window or viewport before executing click."));
		return false;
	}

	const FGeometry Geometry = SlateWidget->GetTickSpaceGeometry();
	const FVector2D ScreenPosition = Geometry.GetAbsolutePosition() + (Geometry.GetLocalSize() * 0.5f);
	const FModifierKeysState Modifiers = SlateApplication.GetPlatformApplication().IsValid()
		? SlateApplication.GetPlatformApplication()->GetModifierKeys()
		: FModifierKeysState();

	SlateApplication.ProcessApplicationActivationEvent(true);
	SlateApplication.SetKeyboardFocus(SlateWidget, EFocusCause::SetDirectly);

	const FPointerEvent MouseDownEvent(
		SlateApplication.GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		ScreenPosition,
		ScreenPosition,
		SlateApplication.GetPressedMouseButtons(),
		EKeys::LeftMouseButton,
		0.0f,
		Modifiers);

	const bool bPressed = SlateApplication.RoutePointerDownEvent(WidgetPath, MouseDownEvent).IsEventHandled();

	const FPointerEvent MouseUpEvent(
		SlateApplication.GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		ScreenPosition,
		ScreenPosition,
		SlateApplication.GetPressedMouseButtons(),
		EKeys::LeftMouseButton,
		0.0f,
		Modifiers);

	const bool bReleased = SlateApplication.RoutePointerUpEvent(WidgetPath, MouseUpEvent).IsEventHandled();
	if (!bPressed && !bReleased)
	{
		PopulateError(
			OutError,
			TEXT("TEST_CLICK_NOT_HANDLED"),
			WidgetId,
			FString::Printf(TEXT("Slate click on widget '%s' was not handled."), *WidgetId),
			TEXT("Check whether the target widget is enabled, visible, and clickable."));
		return false;
	}

	return true;
}

bool FUESpecTestSpecExecutor::ExecuteAssertVisible(UUserWidget& RootWidget, const FString& WidgetId, const bool bExpected, FUESpecCompilerError& OutError)
{
	UWidget* Widget = FindWidgetById(RootWidget, WidgetId);
	if (!Widget)
	{
		PopulateError(
			OutError,
			TEXT("TEST_WIDGET_NOT_FOUND"),
			WidgetId,
			FString::Printf(TEXT("Widget '%s' was not found for assertVisible."), *WidgetId),
			TEXT("Check the widget id and ensure the widget exists in the instantiated tree."));
		return false;
	}

	if (Widget->IsVisible() != bExpected)
	{
		PopulateError(
			OutError,
			TEXT("ASSERT_VISIBLE_FAILED"),
			WidgetId,
			FString::Printf(TEXT("Widget '%s' visibility was %s, expected %s."), *WidgetId, Widget->IsVisible() ? TEXT("true") : TEXT("false"), bExpected ? TEXT("true") : TEXT("false")),
			TEXT("Check the widget visibility and the expected flag."));
		return false;
	}

	return true;
}

bool FUESpecTestSpecExecutor::ExecuteStep(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& StepObject, FUESpecCompilerError& OutError)
{
	if (!StepObject.IsValid())
	{
		PopulateError(OutError, TEXT("INVALID_TEST_STEP"), TEXT("testSpec"), TEXT("test step must be a JSON object."), TEXT("Replace the step with a valid object."));
		return false;
	}

	FString WidgetId;
	if (StepObject->TryGetStringField(TEXT("click"), WidgetId))
	{
		return ExecuteClick(RootWidget, WidgetId, OutError);
	}

	const TSharedPtr<FJsonObject>* AssertVisibleObject = nullptr;
	if (StepObject->TryGetObjectField(TEXT("assertVisible"), AssertVisibleObject) && AssertVisibleObject && (*AssertVisibleObject).IsValid())
	{
		if (!(*AssertVisibleObject)->TryGetStringField(TEXT("id"), WidgetId) || WidgetId.IsEmpty())
		{
			PopulateError(OutError, TEXT("ASSERT_VISIBLE_MISSING_ID"), TEXT("assertVisible.id"), TEXT("assertVisible requires an id."), TEXT("Add the target widget id."));
			return false;
		}

		bool bExpected = true;
		(*AssertVisibleObject)->TryGetBoolField(TEXT("expected"), bExpected);
		return ExecuteAssertVisible(RootWidget, WidgetId, bExpected, OutError);
	}

	PopulateError(
		OutError,
		TEXT("TEST_STEP_NOT_IMPLEMENTED"),
		TEXT("testSpec"),
		TEXT("Only click and assertVisible are implemented in the current executor skeleton."),
		TEXT("Use click/assertVisible for now, or extend the executor with more verbs."));
	return false;
}

UWidget* FUESpecTestSpecExecutor::FindWidgetById(UUserWidget& RootWidget, const FString& WidgetId)
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

void FUESpecTestSpecExecutor::AddError(FUESpecTestExecutionResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint)
{
	FUESpecCompilerError Error;
	PopulateError(Error, Code, Path, Message, Hint);
	OutResult.Errors.Add(MoveTemp(Error));
}
