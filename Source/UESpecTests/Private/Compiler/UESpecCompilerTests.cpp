#include "Misc/AutomationTest.h"

#include "Compiler/UESpecCompilerSubsystem.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerStructuredErrorTest,
	"UESpec.Compiler.InvalidJson.StructuredError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerStructuredErrorTest::RunTest(const FString& Parameters)
{
	if (!GEditor)
	{
		AddError(TEXT("GEditor is not available."));
		return false;
	}

	UUESpecCompilerSubsystem* Compiler = GEditor->GetEditorSubsystem<UUESpecCompilerSubsystem>();
	if (!TestNotNull(TEXT("UESpec compiler subsystem"), Compiler))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const bool bValidated = Compiler->ValidateJsonText(TEXT("{"), Result);

	TestFalse(TEXT("Malformed JSON is rejected"), bValidated);
	TestFalse(TEXT("Malformed JSON result is not ok"), Result.bOk);
	TestTrue(TEXT("Compiler returns at least one structured error"), Result.Errors.Num() > 0);

	if (Result.Errors.Num() > 0)
	{
		TestEqual(TEXT("Structured error code"), Result.Errors[0].Code, FString(TEXT("INVALID_JSON")));
		TestEqual(TEXT("Structured error path"), Result.Errors[0].Path, FString(TEXT("$")));
		TestFalse(TEXT("Structured error message is not empty"), Result.Errors[0].Message.IsEmpty());
		TestFalse(TEXT("Structured error hint is not empty"), Result.Errors[0].Hint.IsEmpty());
	}

	return true;
}
