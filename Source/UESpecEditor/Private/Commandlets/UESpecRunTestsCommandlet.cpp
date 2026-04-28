#include "Commandlets/UESpecRunTestsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Commandlets/UESpecCommandletUtils.h"
#include "Compiler/UESpecCompilerSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/ISlateNullRendererModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TestSpec/UESpecTestRunner.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecRunTestsCommandlet)

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
		FSlateApplication::Get().AddWindow(OutWindow.ToSharedRef(), false);
		FSlateApplication::Get().Tick();

		return RootWidget;
	}

	class FScopedSlateApplicationForUESpecTests
	{
	public:
		FScopedSlateApplicationForUESpecTests()
		{
			if (FSlateApplication::IsInitialized())
			{
				return;
			}

			const TSharedRef<FSlateRenderer> Renderer =
				FModuleManager::LoadModuleChecked<ISlateNullRendererModule>(TEXT("SlateNullRenderer")).CreateSlateNullRenderer();
			FSlateApplication::InitializeAsStandaloneApplication(Renderer);
			FSlateApplication::InitializeCoreStyle();
			bOwnsSlateApplication = true;
		}

		~FScopedSlateApplicationForUESpecTests()
		{
			if (bOwnsSlateApplication && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Shutdown();
			}
		}

		bool IsReady() const
		{
			return FSlateApplication::IsInitialized();
		}

	private:
		bool bOwnsSlateApplication = false;
	};
}

UUESpecRunTestsCommandlet::UUESpecRunTestsCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UUESpecRunTestsCommandlet::Main(const FString& Params)
{
	FString SpecFilePath;
	FString ReportPath;
	FString JsonReportPath;
	FString OutputPath;
	FString Error;
	if (!UE::UESpec::Editor::Commandlet::TryGetRequiredCommandletValue(Params, TEXT("SpecFile="), SpecFilePath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 1;
	}

	UE::UESpec::Editor::Commandlet::GetOptionalCommandletValue(Params, TEXT("Report="), ReportPath);
	UE::UESpec::Editor::Commandlet::GetOptionalCommandletValue(Params, TEXT("JsonReport="), JsonReportPath);
	UE::UESpec::Editor::Commandlet::GetOptionalCommandletValue(Params, TEXT("Output="), OutputPath);
	if (ReportPath.IsEmpty())
	{
		ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("TestReports"), TEXT("UESpecRunTests.junit.xml"));
	}
	if (JsonReportPath.IsEmpty())
	{
		JsonReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("UESpec"),
			TEXT("TestReports"),
			FPaths::GetBaseFilename(SpecFilePath) + TEXT(".failure-report.json"));
	}

	TArray<TSharedPtr<FJsonValue>> StructuredErrors;
	FUESpecTestRunReport Report;
	Report.bOk = false;
	Report.JUnitReportPath = ReportPath;
	FScopedSlateApplicationForUESpecTests SlateApplicationScope;

	if (!GEditor)
	{
		StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
			TEXT("EDITOR_UNAVAILABLE"),
			TEXT("$"),
			TEXT("GEditor is not available."),
			TEXT("Run this commandlet from UnrealEditor-Cmd.")));
	}
	else if (!SlateApplicationScope.IsReady())
	{
		StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
			TEXT("SLATE_APPLICATION_UNAVAILABLE"),
			TEXT("$"),
			TEXT("Slate application could not be initialized for headless UESpec test execution."),
			TEXT("Ensure the SlateNullRenderer module is available before running UESpecRunTests.")));
	}
	else
	{
		UUESpecCompilerSubsystem* const Compiler = GEditor->GetEditorSubsystem<UUESpecCompilerSubsystem>();
		if (!Compiler)
		{
			StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
				TEXT("COMPILER_SUBSYSTEM_UNAVAILABLE"),
				TEXT("$"),
				TEXT("UESpec compiler subsystem is not available."),
				TEXT("Ensure the UESpec editor module is loaded before running tests.")));
		}
		else
		{
			FUESpecCompilerResult ValidationResult;
			const TSharedPtr<FJsonObject> RootObject = LoadNormalizedRootObject(*Compiler, SpecFilePath, ValidationResult);
			if (!RootObject.IsValid())
			{
				for (const FUESpecCompilerError& ValidationError : ValidationResult.Errors)
				{
					StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeCompilerErrorValue(ValidationError));
				}
			}
			else
			{
				FString Kind;
				RootObject->TryGetStringField(TEXT("kind"), Kind);
				if (Kind != TEXT("Test"))
				{
					StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
						TEXT("SPEC_KIND_MUST_BE_TEST"),
						TEXT("kind"),
						FString::Printf(TEXT("Expected kind='Test', but got '%s'."), *Kind),
						TEXT("Pass a UISpec test document to UESpecRunTests.")));
				}
				else
				{
					const TSharedPtr<FJsonObject>* TestSpecObject = nullptr;
					if (!RootObject->TryGetObjectField(TEXT("testSpec"), TestSpecObject) || !TestSpecObject || !(*TestSpecObject).IsValid())
					{
						StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
							TEXT("MISSING_TEST_SPEC"),
							TEXT("testSpec"),
							TEXT("Test UISpec is missing testSpec."),
							TEXT("Add a valid testSpec object before running tests.")));
					}
					else
					{
						FString TargetName;
						(*TestSpecObject)->TryGetStringField(TEXT("target"), TargetName);

						TArray<FString> Candidates;
						FString ResolvedTarget;
						if (UClass* TargetWidgetClass = ResolveTargetWidgetClass(TargetName, ResolvedTarget, Candidates))
						{
							TSharedPtr<SWindow> Window;
							if (UUserWidget* RootWidget = CreateTestWidgetInstance(*TargetWidgetClass, Window, Error))
							{
								Report = FUESpecTestRunner::Execute(*RootWidget, *TestSpecObject, ReportPath, SpecFilePath);
								if (Window.IsValid())
								{
									Window->RequestDestroyWindow();
									FSlateApplication::Get().Tick();
								}
							}
							else
							{
								StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
									TEXT("TEST_TARGET_WIDGET_CREATE_FAILED"),
									TEXT("testSpec.target"),
									Error,
									TEXT("Ensure the compiled widget class can be instantiated in editor commandlet mode.")));
							}
						}
						else
						{
							StructuredErrors.Add(UE::UESpec::Editor::Commandlet::MakeStructuredErrorValue(
								TEXT("TEST_TARGET_NOT_FOUND"),
								TEXT("testSpec.target"),
								FString::Printf(TEXT("Could not resolve test target '%s' to a compiled WidgetBlueprint or widget class."), *TargetName),
								TEXT("Compile the target UISpec first, or pass a widget asset/class path that already exists."),
								Candidates));
						}
					}
				}
			}
		}
	}

	TSharedRef<FJsonObject> Payload = UE::UESpec::Editor::Commandlet::MakeTestReportObject(Report, StructuredErrors);
	Payload->SetStringField(TEXT("failureReportPath"), JsonReportPath);
	const TSharedRef<FJsonObject> FailureReportPayload = UE::UESpec::Editor::Commandlet::MakeFailureReportObject(Report, StructuredErrors, SpecFilePath);
	if (!UE::UESpec::Editor::Commandlet::EmitJsonOutput(FailureReportPayload, JsonReportPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 5;
	}

	if (!UE::UESpec::Editor::Commandlet::EmitJsonOutput(Payload, OutputPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 4;
	}

	if (!OutputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Wrote test result to %s"), *FPaths::ConvertRelativePathToFull(OutputPath));
	}
	if (!JsonReportPath.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Wrote failure report to %s"), *FPaths::ConvertRelativePathToFull(JsonReportPath));
	}

	const bool bSucceeded = StructuredErrors.Num() == 0 && Report.bOk;
	return bSucceeded ? 0 : 10;
}
