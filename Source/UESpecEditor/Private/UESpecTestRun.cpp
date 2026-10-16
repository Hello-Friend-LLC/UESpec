// HF (WI-1586 t305): moved verbatim out of UESpecRunTestsCommandlet.cpp so the commandlet and the live-editor bridge
// share one test-run sequence. Behaviour of the commandlet is unchanged.
#include "UESpecTestRun.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Commandlets/UESpecCommandletUtils.h"
#include "Compiler/UESpecCompilerSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWindow.h"

namespace
{
	static TSharedPtr<FJsonObject> LoadNormalizedRootObject(UUESpecCompilerSubsystem& Compiler, const FString& SpecFilePath, FUESpecCompilerResult& OutValidationResult)
	{
		if (!Compiler.ValidateJsonFile(SpecFilePath, OutValidationResult) || OutValidationResult.NormalizedJson.IsEmpty())
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> RootObject;
		if (!UUESpecCompilerSubsystem::ParseJsonTextToObject(OutValidationResult.NormalizedJson, RootObject))
		{
			UUESpecCompilerSubsystem::AddError(
				OutValidationResult,
				TEXT("INVALID_NORMALIZED_JSON"),
				TEXT("$"),
				TEXT("Compiler failed to parse its normalized UISpec."),
				TEXT("Fix the compiler normalization logic before running tests."));
			OutValidationResult.bOk = false;
			return nullptr;
		}

		return RootObject;
	}

	static UClass* ResolveTargetWidgetClass(const FString& TargetName, FString& OutResolvedTarget, TArray<FString>& OutCandidates)
	{
		OutResolvedTarget.Reset();
		OutCandidates.Reset();

		const auto AddCandidate = [&OutCandidates](const FString& Candidate)
		{
			if (!Candidate.IsEmpty())
			{
				OutCandidates.AddUnique(Candidate);
			}
		};

		if (TargetName.IsEmpty())
		{
			return nullptr;
		}

		const auto TryResolveFromBlueprint = [&OutResolvedTarget](UWidgetBlueprint* WidgetBlueprint) -> UClass*
		{
			if (!WidgetBlueprint)
			{
				return nullptr;
			}

			if (!WidgetBlueprint->GeneratedClass)
			{
				FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
			}

			if (WidgetBlueprint->GeneratedClass && WidgetBlueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
			{
				OutResolvedTarget = WidgetBlueprint->GetPathName();
				return WidgetBlueprint->GeneratedClass;
			}

			return nullptr;
		};

		if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *TargetName, nullptr, LOAD_NoWarn))
		{
			if (LoadedClass->IsChildOf(UUserWidget::StaticClass()))
			{
				OutResolvedTarget = LoadedClass->GetPathName();
				return LoadedClass;
			}
		}

		if (!TargetName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
		{
			if (UClass* LoadedGeneratedClass = LoadObject<UClass>(nullptr, *(TargetName + TEXT("_C")), nullptr, LOAD_NoWarn))
			{
				if (LoadedGeneratedClass->IsChildOf(UUserWidget::StaticClass()))
				{
					OutResolvedTarget = LoadedGeneratedClass->GetPathName();
					return LoadedGeneratedClass;
				}
			}
		}

		if (UWidgetBlueprint* LoadedBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *TargetName, nullptr, LOAD_NoWarn))
		{
			if (UClass* ResolvedClass = TryResolveFromBlueprint(LoadedBlueprint))
			{
				return ResolvedClass;
			}
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		AssetRegistry.SearchAllAssets(true);

		TArray<FAssetData> WidgetBlueprintAssets;
		AssetRegistry.GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetBlueprintAssets, true);

		const FString ExpectedAssetName = FString::Printf(TEXT("WBP_%s"), *TargetName);
		for (const FAssetData& AssetData : WidgetBlueprintAssets)
		{
			AddCandidate(AssetData.AssetName.ToString());
		}

		for (const FAssetData& AssetData : WidgetBlueprintAssets)
		{
			const FString AssetName = AssetData.AssetName.ToString();
			if (!AssetName.Equals(ExpectedAssetName, ESearchCase::CaseSensitive) && !AssetName.Equals(TargetName, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (UClass* ResolvedClass = TryResolveFromBlueprint(Cast<UWidgetBlueprint>(AssetData.GetAsset())))
			{
				return ResolvedClass;
			}
		}

		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* CandidateClass = *ClassIt;
			if (!CandidateClass->IsChildOf(UUserWidget::StaticClass()) || CandidateClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			AddCandidate(CandidateClass->GetName());
			if (CandidateClass->GetName().Equals(TargetName, ESearchCase::CaseSensitive) || CandidateClass->GetName().Equals(TargetName + TEXT("_C"), ESearchCase::CaseSensitive))
			{
				OutResolvedTarget = CandidateClass->GetPathName();
				return CandidateClass;
			}
		}

		return nullptr;
	}

	static UUserWidget* CreateTestWidgetInstance(UClass& WidgetClass, TSharedPtr<SWindow>& OutWindow, FString& OutError)
	{
		if (!FSlateApplication::IsInitialized())
		{
			OutError = TEXT("Slate application is not initialized for test widget instantiation.");
			return nullptr;
		}

		UUserWidget* const RootWidget = NewObject<UUserWidget>(GetTransientPackage(), &WidgetClass, NAME_None, RF_Transient);
		if (!RootWidget)
		{
			OutError = FString::Printf(TEXT("Failed to instantiate target widget class '%s'."), *WidgetClass.GetPathName());
			return nullptr;
		}

		RootWidget->Initialize();

		OutWindow = SNew(SWindow)
			.AutoCenter(EAutoCenter::None)
			.SizingRule(ESizingRule::Autosized);
		OutWindow->SetContent(RootWidget->TakeWidget());
		// HF (WI-1586 t606): when the app can render, FUESpecTestRunner::ExecuteClick/hover/press dispatch real Slate
		// pointer events, and Slate never hit-tests a hidden window: the click then lands on whatever visible window
		// sits under the point (the editor), is reported handled, and the target Button's OnClicked never fires.
		// Show the window in that case (as the UESpecTests fixtures do); headless runs keep it hidden and click via
		// OnClicked.Broadcast().
		FSlateApplication::Get().AddWindow(OutWindow.ToSharedRef(), FApp::CanEverRender());
		FSlateApplication::Get().Tick();

		return RootWidget;
	}
}

namespace UE::UESpec::Editor::TestRun
{
	FString GetDefaultJUnitReportPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("TestReports"), TEXT("UESpecRunTests.junit.xml"));
	}

	FString GetDefaultFailureReportPath(const FString& SpecFilePath)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("UESpec"),
			TEXT("TestReports"),
			FPaths::GetBaseFilename(SpecFilePath) + TEXT(".failure-report.json"));
	}

	bool WriteJsonFile(const TSharedRef<FJsonObject>& Payload, const FString& Path, FString& OutError)
	{
		if (Path.IsEmpty())
		{
			OutError = TEXT("WriteJsonFile needs a non-empty path.");
			return false;
		}
		return Commandlet::EmitJsonOutput(Payload, Path, OutError);
	}

	FUESpecTestRunOutcome RunTestSpecFile(const FString& SpecFilePath, const FString& JUnitReportPath)
	{
		FUESpecTestRunOutcome Outcome;
		FUESpecTestRunReport& Report = Outcome.Report;
		TArray<TSharedPtr<FJsonValue>>& StructuredErrors = Outcome.Errors;
		Report.bOk = false;
		Report.JUnitReportPath = JUnitReportPath;
		FString Error;

		if (!GEditor)
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("EDITOR_UNAVAILABLE"),
				TEXT("$"),
				TEXT("GEditor is not available."),
				TEXT("Run this commandlet from UnrealEditor-Cmd.")));
			return Outcome;
		}
		if (!FSlateApplication::IsInitialized())
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("SLATE_APPLICATION_UNAVAILABLE"),
				TEXT("$"),
				TEXT("Slate application could not be initialized for headless UESpec test execution."),
				TEXT("Ensure the SlateNullRenderer module is available before running UESpecRunTests.")));
			return Outcome;
		}

		UUESpecCompilerSubsystem* const Compiler = GEditor->GetEditorSubsystem<UUESpecCompilerSubsystem>();
		if (!Compiler)
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("COMPILER_SUBSYSTEM_UNAVAILABLE"),
				TEXT("$"),
				TEXT("UESpec compiler subsystem is not available."),
				TEXT("Ensure the UESpec editor module is loaded before running tests.")));
			return Outcome;
		}

		FUESpecCompilerResult ValidationResult;
		const TSharedPtr<FJsonObject> RootObject = LoadNormalizedRootObject(*Compiler, SpecFilePath, ValidationResult);
		if (!RootObject.IsValid())
		{
			for (const FUESpecCompilerError& ValidationError : ValidationResult.Errors)
			{
				StructuredErrors.Add(Commandlet::MakeCompilerErrorValue(ValidationError));
			}
			return Outcome;
		}

		FString Kind;
		RootObject->TryGetStringField(TEXT("kind"), Kind);
		if (Kind != TEXT("Test"))
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("SPEC_KIND_MUST_BE_TEST"),
				TEXT("kind"),
				FString::Printf(TEXT("Expected kind='Test', but got '%s'."), *Kind),
				TEXT("Pass a UISpec test document to UESpecRunTests.")));
			return Outcome;
		}

		const TSharedPtr<FJsonObject>* TestSpecObject = nullptr;
		if (!RootObject->TryGetObjectField(TEXT("testSpec"), TestSpecObject) || !TestSpecObject || !(*TestSpecObject).IsValid())
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("MISSING_TEST_SPEC"),
				TEXT("testSpec"),
				TEXT("Test UISpec is missing testSpec."),
				TEXT("Add a valid testSpec object before running tests.")));
			return Outcome;
		}

		FString TargetName;
		(*TestSpecObject)->TryGetStringField(TEXT("target"), TargetName);

		TArray<FString> Candidates;
		FString ResolvedTarget;
		UClass* const TargetWidgetClass = ResolveTargetWidgetClass(TargetName, ResolvedTarget, Candidates);
		if (!TargetWidgetClass)
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("TEST_TARGET_NOT_FOUND"),
				TEXT("testSpec.target"),
				FString::Printf(TEXT("Could not resolve test target '%s' to a compiled WidgetBlueprint or widget class."), *TargetName),
				TEXT("Compile the target UISpec first, or pass a widget asset/class path that already exists."),
				Candidates));
			return Outcome;
		}

		TSharedPtr<SWindow> Window;
		UUserWidget* const RootWidget = CreateTestWidgetInstance(*TargetWidgetClass, Window, Error);
		if (!RootWidget)
		{
			StructuredErrors.Add(Commandlet::MakeStructuredErrorValue(
				TEXT("TEST_TARGET_WIDGET_CREATE_FAILED"),
				TEXT("testSpec.target"),
				Error,
				TEXT("Ensure the compiled widget class can be instantiated in editor commandlet mode.")));
			return Outcome;
		}

		Report = FUESpecTestRunner::Execute(*RootWidget, *TestSpecObject, JUnitReportPath, SpecFilePath);
		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
		}
		return Outcome;
	}
}
