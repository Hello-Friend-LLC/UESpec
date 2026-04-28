#include "Commandlets/UESpecCompileCommandlet.h"

#include "Commandlets/UESpecCommandletUtils.h"
#include "Compiler/UESpecCompilerSubsystem.h"
#include "Editor.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecCompileCommandlet)

UUESpecCompileCommandlet::UUESpecCompileCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UUESpecCompileCommandlet::Main(const FString& Params)
{
	FString SpecFilePath;
	FString OutputDirectory;
	FString OutputPath;
	FString Error;
	if (!UE::UESpec::Editor::Commandlet::TryGetRequiredCommandletValue(Params, TEXT("SpecFile="), SpecFilePath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 1;
	}

	UE::UESpec::Editor::Commandlet::GetOptionalCommandletValue(Params, TEXT("OutputDir="), OutputDirectory);
	UE::UESpec::Editor::Commandlet::GetOptionalCommandletValue(Params, TEXT("Output="), OutputPath);

	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("GEditor is not available."));
		return 2;
	}

	UUESpecCompilerSubsystem* const Compiler = GEditor->GetEditorSubsystem<UUESpecCompilerSubsystem>();
	if (!Compiler)
	{
		UE_LOG(LogTemp, Error, TEXT("UESpec compiler subsystem is not available."));
		return 3;
	}

	FUESpecCompilerResult Result;
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecFilePath, OutputDirectory, Result);
	if (!UE::UESpec::Editor::Commandlet::EmitJsonOutput(UE::UESpec::Editor::Commandlet::MakeCompilerResultObject(Result), OutputPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 4;
	}

	if (!OutputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Wrote compile result to %s"), *FPaths::ConvertRelativePathToFull(OutputPath));
	}

	return bCompiled ? 0 : 10;
}
