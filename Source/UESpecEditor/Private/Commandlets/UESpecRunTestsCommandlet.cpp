#include "Commandlets/UESpecRunTestsCommandlet.h"

#include "Commandlets/UESpecCommandletUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateNullRendererModule.h"
#include "Misc/Paths.h"
#include "UESpecTestRun.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecRunTestsCommandlet)

namespace
{
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
		ReportPath = UE::UESpec::Editor::TestRun::GetDefaultJUnitReportPath();
	}
	if (JsonReportPath.IsEmpty())
	{
		JsonReportPath = UE::UESpec::Editor::TestRun::GetDefaultFailureReportPath(SpecFilePath);
	}

	// HF (WI-1586 t305): the run itself is shared with the live-editor bridge (UESpecTestRun.h).
	FScopedSlateApplicationForUESpecTests SlateApplicationScope;
	const UE::UESpec::Editor::TestRun::FUESpecTestRunOutcome Outcome =
		UE::UESpec::Editor::TestRun::RunTestSpecFile(SpecFilePath, ReportPath);

	TSharedRef<FJsonObject> Payload = UE::UESpec::Editor::Commandlet::MakeTestReportObject(Outcome.Report, Outcome.Errors);
	Payload->SetStringField(TEXT("failureReportPath"), JsonReportPath);
	const TSharedRef<FJsonObject> FailureReportPayload = UE::UESpec::Editor::Commandlet::MakeFailureReportObject(Outcome.Report, Outcome.Errors, SpecFilePath);
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

	return Outcome.Succeeded() ? 0 : 10;
}
