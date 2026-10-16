#pragma once

#include "CoreMinimal.h"
#include "Compiler/UESpecCompilerTypes.h"
#include "TestSpec/UESpecTestRunner.h"

class FJsonObject;
class FJsonValue;

/**
 * HF (WI-1586 t300): exported JSON shapes of UESpec results, so an editor-side caller (the HF_UISpecEditor live
 * bridge, uispec_* UnrealMCPServer tools) emits exactly what the validate/compile/run_tests commandlets write.
 * Thin forwarders over the module-private commandlet utilities; one serializer, two transports.
 */
namespace UE::UESpec::Editor::ResultJson
{
	UESPECEDITOR_API TSharedRef<FJsonObject> MakeCompilerResultObject(const FUESpecCompilerResult& Result);
	UESPECEDITOR_API TSharedRef<FJsonObject> MakeTestReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors);
	UESPECEDITOR_API TSharedRef<FJsonObject> MakeFailureReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors, const FString& SpecPath);
	UESPECEDITOR_API TSharedPtr<FJsonValue> MakeStructuredErrorValue(const FString& Code, const FString& Path, const FString& Message, const FString& Hint = FString(), const TArray<FString>& NearestDeclared = {});
}
