#pragma once

#include "CoreMinimal.h"
#include "TestSpec/UESpecTestRunner.h"

class FJsonObject;
class FJsonValue;

/**
 * HF (WI-1586 t305): the UESpecRunTests commandlet body, exported so the live-editor bridge (HF_UISpecEditor
 * uispec_run_tests) runs exactly the same resolve-target -> instantiate -> FUESpecTestRunner::Execute sequence.
 * The commandlet keeps only argument parsing, its headless Slate bootstrap and file emission.
 * Game thread only; requires GEditor and an initialized FSlateApplication (the editor's, or the commandlet's).
 */
namespace UE::UESpec::Editor::TestRun
{
	struct FUESpecTestRunOutcome
	{
		FUESpecTestRunReport Report;
		/** Structured errors (same {code, path, message, hint, nearestDeclared} shape as the commandlets). */
		TArray<TSharedPtr<FJsonValue>> Errors;

		bool Succeeded() const { return Errors.Num() == 0 && Report.bOk; }
	};

	/** <Project>/Saved/UESpec/TestReports/UESpecRunTests.junit.xml (the commandlet's default Report=). */
	UESPECEDITOR_API FString GetDefaultJUnitReportPath();

	/** <Project>/Saved/UESpec/TestReports/<spec basename>.failure-report.json (the commandlet's default JsonReport=). */
	UESPECEDITOR_API FString GetDefaultFailureReportPath(const FString& SpecFilePath);

	/** Validate the Test-kind UISpec at SpecFilePath, resolve testSpec.target, instantiate it in an SWindow and run it,
	 *  writing JUnit to JUnitReportPath. Never throws away errors: everything that blocks the run lands in Errors. */
	UESPECEDITOR_API FUESpecTestRunOutcome RunTestSpecFile(const FString& SpecFilePath, const FString& JUnitReportPath);

	/** Pretty-printed JSON to Path (directories created); same writer as the commandlets' Output=/JsonReport=. */
	UESPECEDITOR_API bool WriteJsonFile(const TSharedRef<FJsonObject>& Payload, const FString& Path, FString& OutError);
}
