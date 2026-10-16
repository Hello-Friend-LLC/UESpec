#include "Commandlets/UESpecCommandletUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UE::UESpec::Editor::Commandlet
{
	namespace
	{
		static FString SerializeJsonObject(const TSharedRef<FJsonObject>& JsonObject)
		{
			FString JsonString;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(JsonObject, Writer);
			return JsonString;
		}

		static void SetStringArrayField(FJsonObject& JsonObject, const FString& FieldName, const TArray<FString>& Values)
		{
			TArray<TSharedPtr<FJsonValue>> JsonValues;
			JsonValues.Reserve(Values.Num());
			for (const FString& Value : Values)
			{
				JsonValues.Add(MakeShared<FJsonValueString>(Value));
			}
			JsonObject.SetArrayField(FieldName, JsonValues);
		}

		static bool TryDeserializeJsonValue(const FString& JsonText, TSharedPtr<FJsonValue>& OutValue)
		{
			OutValue.Reset();
			if (JsonText.TrimStartAndEnd().IsEmpty())
			{
				return false;
			}

			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
			return FJsonSerializer::Deserialize(Reader, OutValue) && OutValue.IsValid();
		}

		static void SetSnapshotField(FJsonObject& JsonObject, const FString& FieldName, const FString& SnapshotText)
		{
			if (SnapshotText.IsEmpty())
			{
				JsonObject.SetField(FieldName, MakeShared<FJsonValueNull>());
				return;
			}

			TSharedPtr<FJsonValue> SnapshotValue;
			if (TryDeserializeJsonValue(SnapshotText, SnapshotValue))
			{
				JsonObject.SetField(FieldName, SnapshotValue.ToSharedRef());
				return;
			}

			JsonObject.SetStringField(FieldName, SnapshotText);
		}

		static TArray<FString> ExtractStringArrayField(const FJsonObject& JsonObject, const FString& FieldName)
		{
			TArray<FString> Values;
			const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
			if (!JsonObject.TryGetArrayField(FieldName, JsonArray) || !JsonArray)
			{
				return Values;
			}

			Values.Reserve(JsonArray->Num());
			for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
			{
				if (Entry.IsValid())
				{
					FString StringValue;
					if (Entry->TryGetString(StringValue))
					{
						Values.Add(StringValue);
					}
				}
			}

			return Values;
		}

		static FString StructuredErrorCodeToFailureCategory(const FString& Code)
		{
			if (Code == TEXT("EDITOR_UNAVAILABLE") || Code == TEXT("COMPILER_SUBSYSTEM_UNAVAILABLE"))
			{
				return TEXT("UNKNOWN");
			}

			return TEXT("COMPILE_ERROR");
		}

		static TSharedRef<FJsonObject> MakeFailureContextObject(const FUESpecTestFailureContext& Context, const FString& TestName = FString())
		{
			const TSharedRef<FJsonObject> FailureObject = MakeShared<FJsonObject>();
			if (!TestName.IsEmpty())
			{
				FailureObject->SetStringField(TEXT("testName"), TestName);
			}

			FailureObject->SetStringField(TEXT("category"), Context.Category);
			FailureObject->SetStringField(TEXT("specPath"), Context.SpecPath);
			FailureObject->SetStringField(TEXT("testStep"), Context.TestStep);
			FailureObject->SetStringField(TEXT("failureMessage"), Context.FailureMessage);
			FailureObject->SetStringField(TEXT("expectedValue"), Context.ExpectedValue);
			FailureObject->SetStringField(TEXT("actualValue"), Context.ActualValue);
			SetStringArrayField(*FailureObject, TEXT("missingIds"), Context.MissingIds);
			SetSnapshotField(*FailureObject, TEXT("widgetTreeSnapshot"), Context.WidgetTreeSnapshot);
			SetSnapshotField(*FailureObject, TEXT("stateMachineSnapshot"), Context.StateMachineSnapshot);
			SetStringArrayField(*FailureObject, TEXT("nearestDeclared"), Context.NearestDeclared);
			FailureObject->SetStringField(TEXT("compilerErrorCode"), Context.CompilerErrorCode);
			return FailureObject;
		}

		static FUESpecTestFailureContext MakeFailureContextFromStructuredError(const TSharedPtr<FJsonValue>& ErrorValue, const FString& SpecPath)
		{
			FUESpecTestFailureContext Context;
			Context.SpecPath = SpecPath;

			const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
			if (!ErrorValue.IsValid() || !ErrorValue->TryGetObject(ErrorObject) || !ErrorObject || !(*ErrorObject).IsValid())
			{
				Context.Category = TEXT("UNKNOWN");
				Context.FailureMessage = TEXT("Unknown structured error.");
				return Context;
			}

			FString Code;
			FString Path;
			FString Message;
			(*ErrorObject)->TryGetStringField(TEXT("code"), Code);
			(*ErrorObject)->TryGetStringField(TEXT("path"), Path);
			(*ErrorObject)->TryGetStringField(TEXT("message"), Message);

			Context.Category = StructuredErrorCodeToFailureCategory(Code);
			Context.TestStep = Path;
			Context.FailureMessage = Message;
			Context.NearestDeclared = ExtractStringArrayField(**ErrorObject, TEXT("nearestDeclared"));
			if (Context.Category == TEXT("COMPILE_ERROR"))
			{
				Context.CompilerErrorCode = Code;
			}

			return Context;
		}
	}

	bool TryGetRequiredCommandletValue(const FString& Params, const TCHAR* Key, FString& OutValue, FString& OutError)
	{
		OutValue.Reset();
		if (!FParse::Value(*Params, Key, OutValue) || OutValue.TrimStartAndEnd().IsEmpty())
		{
			OutError = FString::Printf(TEXT("Missing required parameter '-%s<value>'."), Key);
			return false;
		}

		OutValue = OutValue.TrimStartAndEnd();
		return true;
	}

	void GetOptionalCommandletValue(const FString& Params, const TCHAR* Key, FString& OutValue)
	{
		OutValue.Reset();
		if (FParse::Value(*Params, Key, OutValue))
		{
			OutValue = OutValue.TrimStartAndEnd();
		}
	}

	TSharedPtr<FJsonValue> MakeStructuredErrorValue(const FString& Code, const FString& Path, const FString& Message, const FString& Hint, const TArray<FString>& NearestDeclared)
	{
		const TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("code"), Code);
		ErrorObject->SetStringField(TEXT("path"), Path);
		ErrorObject->SetStringField(TEXT("message"), Message);
		ErrorObject->SetStringField(TEXT("hint"), Hint);

		TArray<TSharedPtr<FJsonValue>> Suggestions;
		Suggestions.Reserve(NearestDeclared.Num());
		for (const FString& Candidate : NearestDeclared)
		{
			Suggestions.Add(MakeShared<FJsonValueString>(Candidate));
		}
		ErrorObject->SetArrayField(TEXT("nearestDeclared"), Suggestions);
		return MakeShared<FJsonValueObject>(ErrorObject);
	}

	TSharedPtr<FJsonValue> MakeCompilerErrorValue(const FUESpecCompilerError& Error)
	{
		return MakeStructuredErrorValue(Error.Code, Error.Path, Error.Message, Error.Hint, Error.NearestDeclared);
	}

	TSharedRef<FJsonObject> MakeCompilerResultObject(const FUESpecCompilerResult& Result)
	{
		const TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("ok"), Result.bOk);
		if (Result.NormalizedJson.IsEmpty())
		{
			ResultObject->SetField(TEXT("normalizedJson"), MakeShared<FJsonValueNull>());
		}
		else
		{
			ResultObject->SetStringField(TEXT("normalizedJson"), Result.NormalizedJson);
		}

		TArray<TSharedPtr<FJsonValue>> ErrorArray;
		ErrorArray.Reserve(Result.Errors.Num());
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			ErrorArray.Add(MakeCompilerErrorValue(Error));
		}
		ResultObject->SetArrayField(TEXT("errors"), ErrorArray);

		TArray<TSharedPtr<FJsonValue>> InfoArray;
		InfoArray.Reserve(Result.Infos.Num());
		for (const FUESpecCompilerError& Info : Result.Infos)
		{
			InfoArray.Add(MakeCompilerErrorValue(Info));
		}
		ResultObject->SetArrayField(TEXT("infos"), InfoArray);

		TArray<TSharedPtr<FJsonValue>> AssetArray;
		AssetArray.Reserve(Result.GeneratedAssets.Num());
		for (const FString& AssetPath : Result.GeneratedAssets)
		{
			AssetArray.Add(MakeShared<FJsonValueString>(AssetPath));
		}
		ResultObject->SetArrayField(TEXT("generatedAssets"), AssetArray);

		// Asset identity (only meaningful after a successful compile; empty strings otherwise).
		ResultObject->SetStringField(TEXT("assetPath"), Result.AssetPath);
		ResultObject->SetStringField(TEXT("blueprintGuid"), Result.BlueprintGuid);
		ResultObject->SetStringField(TEXT("assetAction"), Result.AssetAction);
		ResultObject->SetStringField(TEXT("specHash"), Result.SpecHash);
		ResultObject->SetStringField(TEXT("savedPackageFile"), Result.SavedPackageFile);
		return ResultObject;
	}

	FString TestStatusToString(const EUESpecTestStatus Status)
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

	TSharedRef<FJsonObject> MakeTestReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors)
	{
		int32 PassedCount = 0;
		int32 FailedCount = 0;
		int32 SkippedCount = 0;

		TArray<TSharedPtr<FJsonValue>> ResultArray;
		ResultArray.Reserve(Report.Results.Num());
		for (const FUESpecTestResult& TestResult : Report.Results)
		{
			switch (TestResult.Status)
			{
			case EUESpecTestStatus::Pass:
				++PassedCount;
				break;
			case EUESpecTestStatus::Skip:
				++SkippedCount;
				break;
			default:
				++FailedCount;
				break;
			}

			const TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
			ResultObject->SetStringField(TEXT("name"), TestResult.Name);
			ResultObject->SetStringField(TEXT("status"), TestStatusToString(TestResult.Status));
			ResultObject->SetStringField(TEXT("failureMessage"), TestResult.FailureMessage);
			ResultObject->SetNumberField(TEXT("durationSec"), TestResult.DurationSec);
			ResultObject->SetNumberField(TEXT("executedSteps"), TestResult.ExecutedSteps);
			ResultObject->SetObjectField(TEXT("failureContext"), MakeFailureContextObject(TestResult.FailureContext));

			TArray<TSharedPtr<FJsonValue>> PhaseArray;
			PhaseArray.Reserve(TestResult.ExecutedPhases.Num());
			for (const FString& PhaseName : TestResult.ExecutedPhases)
			{
				PhaseArray.Add(MakeShared<FJsonValueString>(PhaseName));
			}
			ResultObject->SetArrayField(TEXT("executedPhases"), PhaseArray);
			ResultArray.Add(MakeShared<FJsonValueObject>(ResultObject));
		}

		TArray<TSharedPtr<FJsonValue>> LogLineArray;
		LogLineArray.Reserve(Report.LogLines.Num());
		for (const FString& LogLine : Report.LogLines)
		{
			LogLineArray.Add(MakeShared<FJsonValueString>(LogLine));
		}

		TArray<TSharedPtr<FJsonValue>> ErrorArray = Errors;
		for (const FUESpecTestResult& TestResult : Report.Results)
		{
			if (TestResult.Status == EUESpecTestStatus::Fail)
			{
				ErrorArray.Add(MakeStructuredErrorValue(
					TEXT("TEST_CASE_FAILED"),
					TEXT("testSpec"),
					FString::Printf(TEXT("Test case '%s' failed: %s"), *TestResult.Name, *TestResult.FailureMessage),
					TEXT("Inspect results, logLines, and the JUnit report for the full test trace.")));
			}
		}

		const TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
		SummaryObject->SetNumberField(TEXT("total"), Report.Results.Num());
		SummaryObject->SetNumberField(TEXT("passed"), PassedCount);
		SummaryObject->SetNumberField(TEXT("failed"), FailedCount);
		SummaryObject->SetNumberField(TEXT("skipped"), SkippedCount);
		SummaryObject->SetStringField(TEXT("reportPath"), Report.JUnitReportPath);

		const TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("ok"), Report.bOk && FailedCount == 0 && ErrorArray.Num() == 0);
		ResultObject->SetObjectField(TEXT("summary"), SummaryObject);
		ResultObject->SetStringField(TEXT("reportPath"), Report.JUnitReportPath);
		ResultObject->SetArrayField(TEXT("results"), ResultArray);
		ResultObject->SetArrayField(TEXT("logLines"), LogLineArray);
		ResultObject->SetArrayField(TEXT("errors"), ErrorArray);
		return ResultObject;
	}

	TSharedRef<FJsonObject> MakeFailureReportObject(const FUESpecTestRunReport& Report, const TArray<TSharedPtr<FJsonValue>>& Errors, const FString& SpecPath)
	{
		int32 PassedCount = 0;
		int32 FailedCount = 0;
		int32 SkippedCount = 0;
		double DurationSeconds = 0.0;

		TArray<TSharedPtr<FJsonValue>> FailureArray;
		FailureArray.Reserve(Report.Results.Num() + Errors.Num());

		for (const FUESpecTestResult& TestResult : Report.Results)
		{
			DurationSeconds += TestResult.DurationSec;
			switch (TestResult.Status)
			{
			case EUESpecTestStatus::Pass:
				++PassedCount;
				break;
			case EUESpecTestStatus::Skip:
				++SkippedCount;
				break;
			default:
				++FailedCount;
				break;
			}

			if (TestResult.Status != EUESpecTestStatus::Fail)
			{
				continue;
			}

			FUESpecTestFailureContext FailureContext = TestResult.FailureContext;
			if (FailureContext.Category.IsEmpty())
			{
				FailureContext.Category = TEXT("UNKNOWN");
				FailureContext.SpecPath = SpecPath;
				FailureContext.FailureMessage = TestResult.FailureMessage;
			}

			FailureArray.Add(MakeShared<FJsonValueObject>(MakeFailureContextObject(FailureContext, TestResult.Name)));
		}

		for (const TSharedPtr<FJsonValue>& ErrorValue : Errors)
		{
			FUESpecTestFailureContext FailureContext = MakeFailureContextFromStructuredError(ErrorValue, SpecPath);
			FailureArray.Add(MakeShared<FJsonValueObject>(MakeFailureContextObject(FailureContext, TEXT("Compiler"))));
			++FailedCount;
		}

		const int32 TotalCount = PassedCount + FailedCount + SkippedCount;

		const TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
		SummaryObject->SetNumberField(TEXT("total"), TotalCount);
		SummaryObject->SetNumberField(TEXT("pass"), PassedCount);
		SummaryObject->SetNumberField(TEXT("fail"), FailedCount);
		SummaryObject->SetNumberField(TEXT("skip"), SkippedCount);
		SummaryObject->SetNumberField(TEXT("duration"), DurationSeconds);

		const TSharedRef<FJsonObject> ReportObject = MakeShared<FJsonObject>();
		ReportObject->SetObjectField(TEXT("summary"), SummaryObject);
		ReportObject->SetArrayField(TEXT("failures"), FailureArray);
		return ReportObject;
	}

	bool EmitJsonOutput(const TSharedRef<FJsonObject>& Payload, const FString& OutputPath, FString& OutError)
	{
		const FString JsonText = SerializeJsonObject(Payload);
		if (!OutputPath.IsEmpty())
		{
			const FString AbsoluteOutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteOutputPath), true);
			if (!FFileHelper::SaveStringToFile(JsonText, *AbsoluteOutputPath))
			{
				OutError = FString::Printf(TEXT("Failed to write JSON output to '%s'."), *AbsoluteOutputPath);
				return false;
			}

			return true;
		}

		GLog->Log(JsonText);
		return true;
	}
}
