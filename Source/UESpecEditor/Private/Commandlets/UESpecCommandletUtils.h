#pragma once

#include "CoreMinimal.h"
#include "Compiler/UESpecCompilerTypes.h"
#include "TestSpec/UESpecTestRunner.h"

class FJsonObject;
class FJsonValue;

namespace UE::UESpec::Editor::Commandlet
{
	bool TryGetRequiredCommandletValue(const FString& Params, const TCHAR* Key, FString& OutValue, FString& OutError);
	void GetOptionalCommandletValue(const FString& Params, const TCHAR* Key, FString& OutValue);

	TSharedPtr<FJsonValue> MakeStructuredErrorValue(const FString& Code, const FString& Path, const FString& Message, const FString& Hint = FString(), const TArray<FString>& NearestDeclared = {});
	TSharedPtr<FJsonValue> MakeCompilerErrorValue(const FUESpecCompilerError& Error);
	TSharedRef<FJsonObject> MakeCompilerResultObject(const FUESpecCompilerResult& Result);
	TSharedRef<FJsonObject> MakeTestReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors);
	TSharedRef<FJsonObject> MakeFailureReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors, const FString& SpecPath);

	bool EmitJsonOutput(const TSharedRef<FJsonObject>& Payload, const FString& OutputPath, FString& OutError);
	FString TestStatusToString(EUESpecTestStatus Status);
}
