#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CoreGlobals.h"
#include "Components/CanvasPanel.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Samples/UESpecSampleWidgets.h"
#include "Serialization/JsonSerializer.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "UESpecTestHelpers.h"
#include "TestSpec/UESpecTestRunner.h"
#include "Widgets/SWindow.h"

namespace
{
	struct FUESpecWindowFixture
	{
		UUESpecTestHostWidget* RootWidget = nullptr;
		UButton* Button = nullptr;
		TSharedPtr<SWindow> Window;
	};

	struct FUESpecSkillBarFixture
	{
		UUESpecStateMachineTestWidget* RootWidget = nullptr;
		TSharedPtr<SWindow> Window;
	};

	struct FUESpecCombatStatusFixture
	{
		UUESpecCombatStatusTestWidget* RootWidget = nullptr;
		TSharedPtr<SWindow> Window;
	};

	struct FUESpecAdventureOverlayFixture
	{
		UUESpecAdventureOverlayTestWidget* RootWidget = nullptr;
		TSharedPtr<SWindow> Window;
	};

	struct FUESpecInventorySocialOverlayFixture
	{
		UUESpecInventorySocialOverlayTestWidget* RootWidget = nullptr;
		TSharedPtr<SWindow> Window;
	};

	struct FUESpecRound6ShowcaseFixture
	{
		UUESpecRound6ShowcaseTestWidget* RootWidget = nullptr;
		TSharedPtr<SWindow> Window;
	};

	static bool ParseTestSpecJson(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	static bool CreateWindowFixture(FAutomationTestBase& Test, FUESpecWindowFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecTestHostWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->WidgetTree = NewObject<UWidgetTree>(OutFixture.RootWidget, NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Widget tree allocated"), OutFixture.RootWidget->WidgetTree.Get());
		if (!OutFixture.RootWidget->WidgetTree)
		{
			return false;
		}

		UCanvasPanel* RootPanel = OutFixture.RootWidget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("skillRow"));
		Test.TestNotNull(TEXT("Root panel allocated"), RootPanel);
		if (!RootPanel)
		{
			return false;
		}

		OutFixture.Button = OutFixture.RootWidget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("slotQ"));
		Test.TestNotNull(TEXT("Button allocated"), OutFixture.Button);
		if (!OutFixture.Button)
		{
			return false;
		}

		RootPanel->AddChild(OutFixture.Button);
		OutFixture.RootWidget->WidgetTree->RootWidget = RootPanel;
		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroyWindowFixture(FUESpecWindowFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool CreateSkillBarFixture(FAutomationTestBase& Test, FUESpecSkillBarFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecStateMachineTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Skill bar root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->InitializeTestTree();
		OutFixture.RootWidget->RunTestLifecycle();

		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroySkillBarFixture(FUESpecSkillBarFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool CreateCombatStatusFixture(FAutomationTestBase& Test, FUESpecCombatStatusFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecCombatStatusTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Combat status root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->InitializeCombatStatusTree();
		OutFixture.RootWidget->RunCombatStatusLifecycle();

		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroyCombatStatusFixture(FUESpecCombatStatusFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool CreateAdventureOverlayFixture(FAutomationTestBase& Test, FUESpecAdventureOverlayFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecAdventureOverlayTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Adventure overlay root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->InitializeAdventureOverlayTree();
		OutFixture.RootWidget->RunAdventureOverlayLifecycle();

		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroyAdventureOverlayFixture(FUESpecAdventureOverlayFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool CreateInventorySocialOverlayFixture(FAutomationTestBase& Test, FUESpecInventorySocialOverlayFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecInventorySocialOverlayTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Inventory-social root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->InitializeInventorySocialTree();
		OutFixture.RootWidget->RunInventorySocialLifecycle();

		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroyInventorySocialOverlayFixture(FUESpecInventorySocialOverlayFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool CreateRound6ShowcaseFixture(FAutomationTestBase& Test, FUESpecRound6ShowcaseFixture& OutFixture)
	{
		OutFixture.RootWidget = NewObject<UUESpecRound6ShowcaseTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("Round6 showcase root widget allocated"), OutFixture.RootWidget);
		if (!OutFixture.RootWidget)
		{
			return false;
		}

		OutFixture.RootWidget->InitializeRound6ShowcaseTree();
		OutFixture.RootWidget->RunRound6ShowcaseLifecycle();

		OutFixture.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutFixture.Window->SetContent(OutFixture.RootWidget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutFixture.Window.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		return true;
	}

	static void DestroyRound6ShowcaseFixture(FUESpecRound6ShowcaseFixture& Fixture)
	{
		if (Fixture.Window.IsValid())
		{
			Fixture.Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Fixture.Window.Reset();
		}
	}

	static bool ExecuteSpecObject(FAutomationTestBase& Test, UUserWidget* RootWidget, const TSharedPtr<FJsonObject>& TestSpecObject, const FString& ReportName, FUESpecTestRunReport& OutReport)
	{
		Test.TestNotNull(TEXT("Root widget is valid"), RootWidget);
		if (!RootWidget || !TestSpecObject.IsValid())
		{
			return false;
		}

		const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("TestReports"), ReportName);
		IFileManager::Get().Delete(*ReportPath, false, true, true);
		OutReport = FUESpecTestRunner::Execute(*RootWidget, TestSpecObject, ReportPath);
		return true;
	}

	static bool ExecuteSpecJson(FAutomationTestBase& Test, UUserWidget* RootWidget, const FString& JsonText, const FString& ReportName, FUESpecTestRunReport& OutReport)
	{
		TSharedPtr<FJsonObject> TestSpecObject;
		Test.TestTrue(TEXT("Test spec parses"), ParseTestSpecJson(JsonText, TestSpecObject));
		if (!TestSpecObject.IsValid())
		{
			return false;
		}

		return ExecuteSpecObject(Test, RootWidget, TestSpecObject, ReportName, OutReport);
	}

	static bool HasLogLineContaining(const FUESpecTestRunReport& Report, const FString& ExpectedSubstring)
	{
		for (const FString& Line : Report.LogLines)
		{
			if (Line.Contains(ExpectedSubstring))
			{
				return true;
			}
		}

		return false;
	}

	static bool KeepSupportedExampleSubset(const TSharedPtr<FJsonObject>& RootObject)
	{
		if (!RootObject.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* TestSpecObject = nullptr;
		if (!RootObject->TryGetObjectField(TEXT("testSpec"), TestSpecObject) || !TestSpecObject || !(*TestSpecObject).IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* PhasesObject = nullptr;
		if (!(*TestSpecObject)->TryGetObjectField(TEXT("phases"), PhasesObject) || !PhasesObject || !(*PhasesObject).IsValid())
		{
			return false;
		}

		const auto FilterPhase = [&](const FString& PhaseName, const TFunctionRef<bool(const TSharedPtr<FJsonObject>&)> Predicate)
		{
			const TArray<TSharedPtr<FJsonValue>>* ExistingSteps = nullptr;
			if (!(*PhasesObject)->TryGetArrayField(PhaseName, ExistingSteps) || !ExistingSteps)
			{
				return;
			}

			TArray<TSharedPtr<FJsonValue>> FilteredSteps;
			for (const TSharedPtr<FJsonValue>& StepValue : *ExistingSteps)
			{
				const TSharedPtr<FJsonObject>* StepObject = nullptr;
				if (StepValue.IsValid() && StepValue->TryGetObject(StepObject) && StepObject && (*StepObject).IsValid() && Predicate(*StepObject))
				{
					FilteredSteps.Add(StepValue);
				}
			}

			(*PhasesObject)->SetArrayField(PhaseName, FilteredSteps);
		};

		FilterPhase(TEXT("arrange"), [](const TSharedPtr<FJsonObject>& StepObject)
		{
			return StepObject->HasField(TEXT("waitForState"));
		});
		FilterPhase(TEXT("act"), [](const TSharedPtr<FJsonObject>& StepObject)
		{
			return StepObject->HasField(TEXT("click"));
		});
		FilterPhase(TEXT("assert"), [](const TSharedPtr<FJsonObject>& StepObject)
		{
			return StepObject->HasField(TEXT("assertVisible"));
		});

		const TArray<TSharedPtr<FJsonValue>>* ParametersArray = nullptr;
		if ((*TestSpecObject)->TryGetArrayField(TEXT("parameters"), ParametersArray) && ParametersArray && ParametersArray->Num() > 1)
		{
			(*TestSpecObject)->SetArrayField(TEXT("parameters"), { (*ParametersArray)[0] });
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAaaFlowTest,
	"UESpec.Tests.TestRunner.AaaFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAaaFlowTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "assume": [
      { "step": "slot is visible", "assertVisible": { "id": "slotQ", "expected": true } }
    ],
    "arrange": [
      { "step": "state becomes Normal", "waitForState": { "state": "Normal", "timeout": 1 } }
    ],
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "slot still visible", "assertVisible": { "id": "slotQ", "expected": true } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("AaaFlow.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("AAA flow reports success"), Report.bOk);
	TestEqual(TEXT("Exactly one test result is produced"), Report.Results.Num(), 1);
	TestTrue(TEXT("JUnit report written"), FPaths::FileExists(Report.JUnitReportPath));
	TestTrue(TEXT("Step log line emitted for assume"), HasLogLineContaining(Report, TEXT("Case1 assume[0] slot is visible")));
	TestTrue(TEXT("Step log line emitted for act"), HasLogLineContaining(Report, TEXT("Case1 act[0] click slot")));
	if (Report.Results.Num() == 1)
	{
		const FUESpecTestResult& Result = Report.Results[0];
		TestEqual(TEXT("Result status is pass"), Result.Status, EUESpecTestStatus::Pass);
		TestEqual(TEXT("All four phases executed"), Result.ExecutedPhases.Num(), 4);
		TestEqual(TEXT("Four steps executed"), Result.ExecutedSteps, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerExampleSubsetTest,
	"UESpec.Tests.TestRunner.ExampleSubset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerExampleSubsetTest::RunTest(const FString& Parameters)
{
	using namespace UE::UESpec::Tests;

	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	FString JsonText;
	const FString SpecPath = GetTestDataPath(TEXT("test-skill-bar.json"));
	TestTrue(TEXT("test-skill-bar.json exists"), FPaths::FileExists(SpecPath));
	TestTrue(TEXT("test-skill-bar.json loads"), LoadTextFile(SpecPath, JsonText));

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("test-skill-bar.json parses"), ParseJsonObject(JsonText, RootObject));
	TestTrue(TEXT("supported subset is extracted"), KeepSupportedExampleSubset(RootObject));
	if (!RootObject.IsValid())
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("TestReports"), TEXT("ExampleSubset.junit.xml"));
	IFileManager::Get().Delete(*ReportPath, false, true, true);
	const FUESpecTestRunReport Report = FUESpecTestRunner::Execute(*Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), ReportPath);
	DestroyWindowFixture(Fixture);

	TestTrue(TEXT("Supported subset of example passes"), Report.bOk);
	TestEqual(TEXT("Exactly one result is produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		const FUESpecTestResult& Result = Report.Results[0];
		TestEqual(TEXT("Example subset uses the first parameter name"), Result.Name, FString(TEXT("WhenReady")));
		TestTrue(TEXT("Arrange waitForState from example executed"), HasLogLineContaining(Report, TEXT("WhenReady arrange[0] initial state is Normal or Cooldown")));
		TestTrue(TEXT("Act click from example executed"), HasLogLineContaining(Report, TEXT("WhenReady act[0] click Q slot")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerSkipOnAssumeFailureTest,
	"UESpec.Tests.TestRunner.SkipOnAssumeFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerSkipOnAssumeFailureTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "assume": [
      { "step": "missing slot is visible", "assertVisible": { "id": "missingSlot", "expected": true } }
    ],
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "slot still visible", "assertVisible": { "id": "slotQ", "expected": true } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("SkipOnAssumeFailure.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Skip does not mark the whole report failed"), Report.bOk);
	TestEqual(TEXT("Exactly one result is produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		const FUESpecTestResult& Result = Report.Results[0];
		TestEqual(TEXT("Assume failure becomes skip"), Result.Status, EUESpecTestStatus::Skip);
		TestEqual(TEXT("Only assume phase executed"), Result.ExecutedPhases.Num(), 1);
		TestEqual(TEXT("Failed assume step counted as executed"), Result.ExecutedSteps, 1);
	}

	TestTrue(TEXT("Failure log line emitted for assume"), HasLogLineContaining(Report, TEXT("[FAIL] Case1 assume[0] missing slot is visible")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerJUnitEscapeTest,
	"UESpec.Tests.TestRunner.JUnitEscape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerJUnitEscapeTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "parameters": [
    { "name": "Case<&\"'>"}
  ],
  "phases": {
    "assert": [
      { "step": "slot <&> stays hidden", "assertVisible": { "id": "slotQ", "expected": false } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("JUnitEscape.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Failing assert marks the report failed"), Report.bOk);
	TestEqual(TEXT("Exactly one result is produced"), Report.Results.Num(), 1);

	FString XmlText;
	TestTrue(TEXT("JUnit report can be read"), FFileHelper::LoadFileToString(XmlText, *Report.JUnitReportPath));
	TestTrue(TEXT("Test case name is XML escaped"), XmlText.Contains(TEXT("Case&lt;&amp;&quot;&apos;&gt;")));
	TestTrue(TEXT("Failure message is XML escaped"), XmlText.Contains(TEXT("Widget &apos;slotQ&apos; visibility")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerHoverTest,
	"UESpec.Tests.TestRunner.Hover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerHoverTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "hover slot", "hover": "slotQ" }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("Hover.junit.xml"), Report);
	TestTrue(TEXT("Hover handler was triggered"), Fixture.RootWidget && Fixture.RootWidget->IsSlotQHovered());
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Hover report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerFocusTest,
	"UESpec.Tests.TestRunner.Focus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerFocusTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "focus slot", "focus": "slotQ" }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("Focus.junit.xml"), Report);
	const UWidget* const SlotQ = Fixture.RootWidget ? Fixture.RootWidget->FindTestWidgetByName(TEXT("slotQ")) : nullptr;
	TestTrue(TEXT("slotQ exists"), SlotQ != nullptr);
	TestTrue(TEXT("slotQ received keyboard focus"), SlotQ && FSlateApplication::Get().GetKeyboardFocusedWidget() == SlotQ->GetCachedWidget());
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Focus report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerNavigateKeyTest,
	"UESpec.Tests.TestRunner.NavigateKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerNavigateKeyTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "arrange": [
      { "step": "focus slot", "focus": "slotQ" }
    ],
    "act": [
      { "step": "navigate right", "navigateKey": { "key": "Right" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("NavigateKey.junit.xml"), Report);
	const UWidget* const SlotW = Fixture.RootWidget ? Fixture.RootWidget->FindTestWidgetByName(TEXT("slotW")) : nullptr;
	TestTrue(TEXT("slotW exists"), SlotW != nullptr);
	TestTrue(TEXT("slotW received focus after navigation"), SlotW && FSlateApplication::Get().GetKeyboardFocusedWidget() == SlotW->GetCachedWidget());
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("NavigateKey report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerSetVMFieldTest,
	"UESpec.Tests.TestRunner.SetVMField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerSetVMFieldTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "mark skill not ready", "setVMField": { "field": "skills[0].isReady", "value": false } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("SetVMField.junit.xml"), Report);
	UVM_SkillBar* const ViewModel = Fixture.RootWidget ? Fixture.RootWidget->GetSkillBarViewModel() : nullptr;
	TestNotNull(TEXT("Skill bar viewmodel exists"), ViewModel);
	TestTrue(TEXT("setVMField updated the viewmodel"), ViewModel && !ViewModel->IsSkillReady(0));
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("SetVMField report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerWaitForVisibleTest,
	"UESpec.Tests.TestRunner.WaitForVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerWaitForVisibleTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "wait for cooldown", "waitForVisible": { "id": "slotQ_cooldownBar", "timeout": 1.0 } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("WaitForVisible.junit.xml"), Report);
	const UWidget* const CooldownBar = Fixture.RootWidget ? Fixture.RootWidget->FindTestWidgetByName(TEXT("slotQ_cooldownBar")) : nullptr;
	TestTrue(TEXT("Cooldown bar exists"), CooldownBar != nullptr);
	TestTrue(TEXT("Cooldown bar became visible"), CooldownBar && CooldownBar->IsVisible());
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("WaitForVisible report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerHeadlessAssertVisibleTest,
	"UESpec.Tests.TestRunner.HeadlessAssertVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerHeadlessAssertVisibleTest::RunTest(const FString& Parameters)
{
	UUESpecLayerTestWidget* const RootWidget = NewObject<UUESpecLayerTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Layer widget allocated"), RootWidget);
	if (!RootWidget)
	{
		return false;
	}

	RootWidget->WidgetTree = NewObject<UWidgetTree>(RootWidget, NAME_None, RF_Transient);
	TestNotNull(TEXT("Widget tree allocated"), RootWidget->WidgetTree.Get());
	if (!RootWidget->WidgetTree)
	{
		return false;
	}

	UCanvasPanel* const RootPanel = RootWidget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("rootPanel"));
	UImage* const AuraBadge = RootWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("auraBadge"));
	TestNotNull(TEXT("Root panel allocated"), RootPanel);
	TestNotNull(TEXT("Aura badge allocated"), AuraBadge);
	if (!RootPanel || !AuraBadge)
	{
		return false;
	}

	RootPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AuraBadge->SetVisibility(ESlateVisibility::Visible);
	RootPanel->AddChild(AuraBadge);
	RootWidget->WidgetTree->RootWidget = RootPanel;

	const FString JsonText = TEXT(R"JSON(
{
  "target": "HeadlessHUD",
  "phases": {
    "assert": [
      { "step": "badge appears", "assertVisible": { "id": "auraBadge", "expected": true } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, RootWidget, JsonText, TEXT("HeadlessAssertVisible.junit.xml"), Report);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Headless assertVisible report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerHeadlessClickFallbackTest,
	"UESpec.Tests.TestRunner.HeadlessClickFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerHeadlessClickFallbackTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "state is cooldown", "assertState": { "expected": "Cooldown" } }
    ]
  }
}
)JSON");

	TGuardValue<bool> ScopedRunningCommandlet(PRIVATE_GIsRunningCommandlet, true);

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("HeadlessClickFallback.junit.xml"), Report);
	TestTrue(TEXT("Headless click drives the widget into Cooldown"), Fixture.RootWidget && Fixture.RootWidget->GetUIState() == TEXT("Cooldown"));
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Headless click report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerHeadlessClickUnboundFailsTest,
	"UESpec.Tests.TestRunner.HeadlessClickUnboundFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerHeadlessClickUnboundFailsTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	Fixture.Button->OnClicked.Clear();

	const FString JsonText = TEXT(R"JSON(
{
  "target": "LayerHUD",
  "phases": {
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "slot remains visible", "assertVisible": { "id": "slotQ", "expected": true } }
    ]
  }
}
)JSON");

	TGuardValue<bool> ScopedRunningCommandlet(PRIVATE_GIsRunningCommandlet, true);

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("HeadlessClickUnboundFails.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Unbound headless click report fails"), Report.bOk);
	TestEqual(TEXT("Single result produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Result failed"), Report.Results[0].Status, EUESpecTestStatus::Fail);
	TestEqual(TEXT("Category classified"), Report.Results[0].FailureContext.Category, FString(TEXT("SLATE_EVENT_NOT_HANDLED")));
	TestTrue(TEXT("Failure message mentions unhandled Slate click"), Report.Results[0].FailureMessage.Contains(TEXT("Slate click on widget 'slotQ' was not handled.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerEmitSignalTest,
	"UESpec.Tests.TestRunner.EmitSignal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerEmitSignalTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	TestTrue(TEXT("State machine has a Normal state"), Fixture.RootWidget && Fixture.RootWidget->StateMachine.States.Num() > 0);
	if (!Fixture.RootWidget || Fixture.RootWidget->StateMachine.States.Num() == 0)
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	Fixture.RootWidget->StateMachine.States[0].Transitions.Reset();
	FUESpecStateTransition& Transition = Fixture.RootWidget->StateMachine.States[0].Transitions.AddDefaulted_GetRef();
	Transition.To = TEXT("Cooldown");
	Transition.When = TEXT("onSkillUsed");
	Fixture.RootWidget->SetUIState(TEXT("Normal"));

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "emit skill signal", "emitSignal": "onSkillUsed" }
    ],
    "assert": [
      { "step": "state is cooldown", "assertState": { "expected": "Cooldown" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("EmitSignal.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("emitSignal drives the widget into Cooldown"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAssertTextTest,
	"UESpec.Tests.TestRunner.AssertText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAssertTextTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "assert": [
      { "step": "keybind matches regex", "assertText": { "id": "slotQ_keybind", "expected": "Q|W", "match": "Regex" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("AssertText.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("AssertText report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAssertStyleTest,
	"UESpec.Tests.TestRunner.AssertStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAssertStyleTest::RunTest(const FString& Parameters)
{
	if (GEngine)
	{
		if (UUESpecSupportSurfaceSubsystem* const SupportSurface = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
		{
			SupportSurface->RegisterStyleTokenSurface(TEXT("style.cooldownOpacity"), TEXT("0.6"));
		}
	}

	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "slot is dimmed", "assertStyle": { "id": "slotQ", "property": "opacity", "expected": "@style.cooldownOpacity" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("AssertStyle.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("AssertStyle report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAssertStateTest,
	"UESpec.Tests.TestRunner.AssertState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAssertStateTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "click slot", "click": "slotQ" }
    ],
    "assert": [
      { "step": "state is cooldown", "assertState": { "expected": "Cooldown" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("AssertState.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("AssertState report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAssertFocusTest,
	"UESpec.Tests.TestRunner.AssertFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAssertFocusTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "focus slot", "focus": "slotQ" }
    ],
    "assert": [
      { "step": "focus stayed on slot", "assertFocus": { "expected": "slotQ" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("AssertFocus.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("AssertFocus report passes"), Report.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerScreenshotTest,
	"UESpec.Tests.TestRunner.Screenshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerScreenshotTest::RunTest(const FString& Parameters)
{
	const FString ScreenshotPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Screenshots"), TEXT("runner_screenshot.png"));
	IFileManager::Get().Delete(*ScreenshotPath, false, true, true);

	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "assert": [
      { "step": "capture screenshot", "screenshot": { "name": "runner_screenshot" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("Screenshot.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Screenshot report passes"), Report.bOk);
	TestTrue(TEXT("Screenshot file was written"), FPaths::FileExists(ScreenshotPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerFullExampleSkillBarTest,
	"UESpec.Tests.TestRunner.FullExampleSkillBarTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerFullExampleSkillBarTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	using namespace UE::UESpec::Tests;

	FString JsonText;
	const FString SpecPath = GetTestDataPath(TEXT("test-skill-bar.json"));
	TestTrue(TEXT("test-skill-bar.json exists"), FPaths::FileExists(SpecPath));
	TestTrue(TEXT("test-skill-bar.json loads"), LoadTextFile(SpecPath, JsonText));

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("test-skill-bar.json parses"), ParseJsonObject(JsonText, RootObject));
	if (!RootObject.IsValid())
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString ScreenshotPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Screenshots"), TEXT("cooldown_state.png"));
	IFileManager::Get().Delete(*ScreenshotPath, false, true, true);

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecObject(*this, Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), TEXT("FullExampleSkillBarTest.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestTrue(TEXT("Full example passes"), Report.bOk);
	TestEqual(TEXT("Both parameter cases ran"), Report.Results.Num(), 2);
	if (Report.Results.Num() == 2)
	{
		TestEqual(TEXT("First case name is WhenReady"), Report.Results[0].Name, FString(TEXT("WhenReady")));
		TestEqual(TEXT("Second case name is WhenCooldown"), Report.Results[1].Name, FString(TEXT("WhenCooldown")));
		TestEqual(TEXT("First case passes"), Report.Results[0].Status, EUESpecTestStatus::Pass);
		TestEqual(TEXT("Second case passes"), Report.Results[1].Status, EUESpecTestStatus::Pass);
	}

	TestTrue(TEXT("Full example captured a screenshot"), FPaths::FileExists(ScreenshotPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerCombatStatusExampleTest,
	"UESpec.Tests.TestRunner.CombatStatusExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerCombatStatusExampleTest::RunTest(const FString& Parameters)
{
	FUESpecCombatStatusFixture Fixture;
	TestTrue(TEXT("Combat-status fixture is created"), CreateCombatStatusFixture(*this, Fixture));
	if (!Fixture.RootWidget)
	{
		DestroyCombatStatusFixture(Fixture);
		return false;
	}

	ON_SCOPE_EXIT
	{
		DestroyCombatStatusFixture(Fixture);
	};

	const FString TestSpecPath = UE::UESpec::Tests::GetExamplePath(TEXT("Tests/combat-status-hud.test.json"));
	TestTrue(TEXT("Combat-status test example exists"), FPaths::FileExists(TestSpecPath));
	if (!FPaths::FileExists(TestSpecPath))
	{
		return false;
	}

	FString JsonText;
	TestTrue(TEXT("Combat-status test example loads"), UE::UESpec::Tests::LoadTextFile(TestSpecPath, JsonText));
	if (JsonText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Combat-status test example parses"), ParseTestSpecJson(JsonText, RootObject));
	if (!RootObject.IsValid() || !RootObject->HasTypedField<EJson::Object>(TEXT("testSpec")))
	{
		return false;
	}

	FUESpecTestRunReport Report;
	TestTrue(TEXT("Combat-status test example executes"), ExecuteSpecObject(*this, Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), TEXT("combat_status_example.xml"), Report));
	TestTrue(TEXT("Combat-status report passes"), Report.bOk);
	TestEqual(TEXT("Combat-status example result count"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		TestEqual(TEXT("Combat-status example executes all assume/assert steps"), Report.Results[0].ExecutedSteps, 6);
	}
	TestTrue(TEXT("Combat-status example recorded health label assertion"), HasLogLineContaining(Report, TEXT("health label is shown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerAdventureOverlayExampleTest,
	"UESpec.Tests.TestRunner.AdventureOverlayExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerAdventureOverlayExampleTest::RunTest(const FString& Parameters)
{
	FUESpecAdventureOverlayFixture Fixture;
	TestTrue(TEXT("Adventure-overlay fixture is created"), CreateAdventureOverlayFixture(*this, Fixture));
	if (!Fixture.RootWidget)
	{
		DestroyAdventureOverlayFixture(Fixture);
		return false;
	}

	ON_SCOPE_EXIT
	{
		DestroyAdventureOverlayFixture(Fixture);
	};

	const FString TestSpecPath = UE::UESpec::Tests::GetExamplePath(TEXT("Tests/adventure-overlay.test.json"));
	TestTrue(TEXT("Adventure-overlay test example exists"), FPaths::FileExists(TestSpecPath));
	if (!FPaths::FileExists(TestSpecPath))
	{
		return false;
	}

	FString JsonText;
	TestTrue(TEXT("Adventure-overlay test example loads"), UE::UESpec::Tests::LoadTextFile(TestSpecPath, JsonText));
	if (JsonText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Adventure-overlay test example parses"), ParseTestSpecJson(JsonText, RootObject));
	if (!RootObject.IsValid() || !RootObject->HasTypedField<EJson::Object>(TEXT("testSpec")))
	{
		return false;
	}

	FUESpecTestRunReport Report;
	TestTrue(TEXT("Adventure-overlay test example executes"), ExecuteSpecObject(*this, Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), TEXT("adventure_overlay_example.xml"), Report));
	TestTrue(TEXT("Adventure-overlay report passes"), Report.bOk);
	TestEqual(TEXT("Adventure-overlay example result count"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		TestEqual(TEXT("Adventure-overlay example executes all assume/assert steps"), Report.Results[0].ExecutedSteps, 8);
	}

	TestTrue(TEXT("Adventure-overlay example recorded quest assertion"), HasLogLineContaining(Report, TEXT("tracked quest title is shown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerInventorySocialOverlayExampleTest,
	"UESpec.Tests.TestRunner.InventorySocialOverlayExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerInventorySocialOverlayExampleTest::RunTest(const FString& Parameters)
{
	FUESpecInventorySocialOverlayFixture Fixture;
	TestTrue(TEXT("Inventory-social fixture is created"), CreateInventorySocialOverlayFixture(*this, Fixture));
	if (!Fixture.RootWidget)
	{
		DestroyInventorySocialOverlayFixture(Fixture);
		return false;
	}

	ON_SCOPE_EXIT
	{
		DestroyInventorySocialOverlayFixture(Fixture);
	};

	const FString TestSpecPath = UE::UESpec::Tests::GetExamplePath(TEXT("Tests/inventory-social-overlay.test.json"));
	TestTrue(TEXT("Inventory-social test example exists"), FPaths::FileExists(TestSpecPath));
	if (!FPaths::FileExists(TestSpecPath))
	{
		return false;
	}

	FString JsonText;
	TestTrue(TEXT("Inventory-social test example loads"), UE::UESpec::Tests::LoadTextFile(TestSpecPath, JsonText));
	if (JsonText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Inventory-social test example parses"), ParseTestSpecJson(JsonText, RootObject));
	if (!RootObject.IsValid() || !RootObject->HasTypedField<EJson::Object>(TEXT("testSpec")))
	{
		return false;
	}

	FUESpecTestRunReport Report;
	TestTrue(TEXT("Inventory-social test example executes"), ExecuteSpecObject(*this, Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), TEXT("inventory_social_overlay_example.xml"), Report));
	TestTrue(TEXT("Inventory-social report passes"), Report.bOk);
	TestEqual(TEXT("Inventory-social example result count"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		TestEqual(TEXT("Inventory-social example executes all assume/assert steps"), Report.Results[0].ExecutedSteps, 9);
	}

	TestTrue(TEXT("Inventory-social example recorded chat assertion"), HasLogLineContaining(Report, TEXT("chat message is shown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecTestRunnerRound6ShowcaseExampleTest,
	"UESpec.Tests.TestRunner.Round6ShowcaseExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecTestRunnerRound6ShowcaseExampleTest::RunTest(const FString& Parameters)
{
	FUESpecRound6ShowcaseFixture Fixture;
	TestTrue(TEXT("Round6 showcase fixture is created"), CreateRound6ShowcaseFixture(*this, Fixture));
	if (!Fixture.RootWidget)
	{
		DestroyRound6ShowcaseFixture(Fixture);
		return false;
	}

	ON_SCOPE_EXIT
	{
		DestroyRound6ShowcaseFixture(Fixture);
	};

	const FString TestSpecPath = UE::UESpec::Tests::GetExamplePath(TEXT("Tests/round6-showcase-hud.test.json"));
	TestTrue(TEXT("Round6 showcase test example exists"), FPaths::FileExists(TestSpecPath));
	if (!FPaths::FileExists(TestSpecPath))
	{
		return false;
	}

	FString JsonText;
	TestTrue(TEXT("Round6 showcase test example loads"), UE::UESpec::Tests::LoadTextFile(TestSpecPath, JsonText));
	if (JsonText.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Round6 showcase test example parses"), ParseTestSpecJson(JsonText, RootObject));
	if (!RootObject.IsValid() || !RootObject->HasTypedField<EJson::Object>(TEXT("testSpec")))
	{
		return false;
	}

	FUESpecTestRunReport Report;
	TestTrue(TEXT("Round6 showcase test example executes"), ExecuteSpecObject(*this, Fixture.RootWidget, RootObject->GetObjectField(TEXT("testSpec")), TEXT("round6_showcase_example.xml"), Report));
	TestTrue(TEXT("Round6 showcase report passes"), Report.bOk);
	TestEqual(TEXT("Round6 showcase example result count"), Report.Results.Num(), 1);
	if (Report.Results.Num() == 1)
	{
		TestEqual(TEXT("Round6 showcase example executes all assume/assert steps"), Report.Results[0].ExecutedSteps, 12);
	}

	TestTrue(TEXT("Round6 showcase example recorded equipped item power assertion"), HasLogLineContaining(Report, TEXT("equipped item power is shown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecFailureReportWidgetMissingCategoryTest,
	"UESpec.Tests.FailureReport.WidgetMissingCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecFailureReportWidgetMissingCategoryTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "act": [
      { "step": "click missing slot", "click": "slotQ_missing" }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("FailureReportWidgetMissing.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Spec fails"), Report.bOk);
	TestEqual(TEXT("Single result produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Result failed"), Report.Results[0].Status, EUESpecTestStatus::Fail);
	TestEqual(TEXT("Category classified"), Report.Results[0].FailureContext.Category, FString(TEXT("WIDGET_MISSING")));
	TestTrue(TEXT("Missing id recorded"), Report.Results[0].FailureContext.MissingIds.Contains(TEXT("slotQ_missing")));
	TestEqual(TEXT("Step recorded"), Report.Results[0].FailureContext.TestStep, FString(TEXT("click missing slot")));
	TestTrue(TEXT("Widget tree snapshot populated"), !Report.Results[0].FailureContext.WidgetTreeSnapshot.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecFailureReportAssertionFailedCategoryTest,
	"UESpec.Tests.FailureReport.AssertionFailedCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecFailureReportAssertionFailedCategoryTest::RunTest(const FString& Parameters)
{
	FUESpecWindowFixture Fixture;
	if (!CreateWindowFixture(*this, Fixture))
	{
		DestroyWindowFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "assert": [
      { "step": "slot should be hidden", "assertVisible": { "id": "slotQ", "expected": false } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("FailureReportAssertionFailed.junit.xml"), Report);
	DestroyWindowFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Spec fails"), Report.bOk);
	TestEqual(TEXT("Single result produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Result failed"), Report.Results[0].Status, EUESpecTestStatus::Fail);
	TestEqual(TEXT("Category classified"), Report.Results[0].FailureContext.Category, FString(TEXT("ASSERTION_FAILED")));
	TestEqual(TEXT("Expected visibility recorded"), Report.Results[0].FailureContext.ExpectedValue, FString(TEXT("false")));
	TestEqual(TEXT("Actual visibility recorded"), Report.Results[0].FailureContext.ActualValue, FString(TEXT("true")));
	TestEqual(TEXT("Step recorded"), Report.Results[0].FailureContext.TestStep, FString(TEXT("slot should be hidden")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecFailureReportStateUnsetCategoryTest,
	"UESpec.Tests.FailureReport.StateUnsetCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecFailureReportStateUnsetCategoryTest::RunTest(const FString& Parameters)
{
	UUESpecLayerTestWidget* const RootWidget = NewObject<UUESpecLayerTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Layer widget allocated"), RootWidget);
	if (!RootWidget)
	{
		return false;
	}

	RootWidget->WidgetTree = NewObject<UWidgetTree>(RootWidget, NAME_None, RF_Transient);
	TestNotNull(TEXT("Widget tree allocated"), RootWidget->WidgetTree.Get());
	if (!RootWidget->WidgetTree)
	{
		return false;
	}

	UCanvasPanel* const RootPanel = RootWidget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("rootPanel"));
	TestNotNull(TEXT("Root panel allocated"), RootPanel);
	if (!RootPanel)
	{
		return false;
	}

	RootWidget->WidgetTree->RootWidget = RootPanel;

	const FString JsonText = TEXT(R"JSON(
{
  "target": "LayerHUD",
  "phases": {
    "assert": [
      { "step": "cooldown state exists", "assertState": { "expected": "Cooldown" } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, RootWidget, JsonText, TEXT("FailureReportStateUnset.junit.xml"), Report);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Spec fails"), Report.bOk);
	TestEqual(TEXT("Single result produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Result failed"), Report.Results[0].Status, EUESpecTestStatus::Fail);
	TestEqual(TEXT("Category classified"), Report.Results[0].FailureContext.Category, FString(TEXT("ASSERTION_FAILED")));
	TestEqual(TEXT("Expected state recorded"), Report.Results[0].FailureContext.ExpectedValue, FString(TEXT("Cooldown")));
	TestEqual(TEXT("Actual state recorded"), Report.Results[0].FailureContext.ActualValue, FString(TEXT("None")));
	TestTrue(TEXT("Failure message distinguishes unset state"), Report.Results[0].FailureMessage.Contains(TEXT("Widget state was 'None'")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecFailureReportTimeoutCategoryTest,
	"UESpec.Tests.FailureReport.TimeoutCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecFailureReportTimeoutCategoryTest::RunTest(const FString& Parameters)
{
	FUESpecSkillBarFixture Fixture;
	if (!CreateSkillBarFixture(*this, Fixture))
	{
		DestroySkillBarFixture(Fixture);
		return false;
	}

	const FString JsonText = TEXT(R"JSON(
{
  "target": "SkillBarHUD",
  "phases": {
    "arrange": [
      { "step": "wait for impossible state", "waitForState": { "state": "Never", "timeout": 0.05 } }
    ]
  }
}
)JSON");

	FUESpecTestRunReport Report;
	const bool bExecuted = ExecuteSpecJson(*this, Fixture.RootWidget, JsonText, TEXT("FailureReportTimeout.junit.xml"), Report);
	DestroySkillBarFixture(Fixture);
	if (!bExecuted)
	{
		return false;
	}

	TestFalse(TEXT("Spec fails"), Report.bOk);
	TestEqual(TEXT("Single result produced"), Report.Results.Num(), 1);
	if (Report.Results.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Result failed"), Report.Results[0].Status, EUESpecTestStatus::Fail);
	TestEqual(TEXT("Category classified"), Report.Results[0].FailureContext.Category, FString(TEXT("TIMEOUT")));
	TestEqual(TEXT("Expected state recorded"), Report.Results[0].FailureContext.ExpectedValue, FString(TEXT("Never")));
	TestTrue(TEXT("Actual state recorded"), !Report.Results[0].FailureContext.ActualValue.IsEmpty());
	TestEqual(TEXT("Step recorded"), Report.Results[0].FailureContext.TestStep, FString(TEXT("wait for impossible state")));
	TestTrue(TEXT("State machine snapshot populated"), !Report.Results[0].FailureContext.StateMachineSnapshot.IsEmpty());
	return true;
}
