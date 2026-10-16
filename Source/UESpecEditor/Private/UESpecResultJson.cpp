#include "UESpecResultJson.h"

#include "Commandlets/UESpecCommandletUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UE::UESpec::Editor::ResultJson
{
	TSharedRef<FJsonObject> MakeCompilerResultObject(const FUESpecCompilerResult& Result)
	{
		return Commandlet::MakeCompilerResultObject(Result);
	}

	TSharedRef<FJsonObject> MakeTestReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors)
	{
		return Commandlet::MakeTestReportObject(Report, Errors);
	}

	TSharedRef<FJsonObject> MakeFailureReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors, const FString& SpecPath)
	{
		return Commandlet::MakeFailureReportObject(Report, Errors, SpecPath);
	}

	TSharedPtr<FJsonValue> MakeStructuredErrorValue(const FString& Code, const FString& Path, const FString& Message, const FString& Hint, const TArray<FString>& NearestDeclared)
	{
		return Commandlet::MakeStructuredErrorValue(Code, Path, Message, Hint, NearestDeclared);
	}
}
