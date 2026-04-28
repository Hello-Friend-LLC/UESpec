#pragma once

#include "CoreMinimal.h"
#include "Compiler/UESpecCompilerTypes.h"

class UUserWidget;
class UWidget;
class FJsonObject;

struct FUESpecTestExecutionResult
{
	bool bOk = false;
	int32 ExecutedSteps = 0;
	TArray<FUESpecCompilerError> Errors;
};

class FUESpecTestSpecExecutor
{
public:
	static FUESpecTestExecutionResult ExecuteTestSpec(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& TestSpecObject);
	static bool ExecuteClick(UUserWidget& RootWidget, const FString& WidgetId, FUESpecCompilerError& OutError);
	static bool ExecuteAssertVisible(UUserWidget& RootWidget, const FString& WidgetId, bool bExpected, FUESpecCompilerError& OutError);

private:
	static bool ExecuteStep(UUserWidget& RootWidget, const TSharedPtr<FJsonObject>& StepObject, FUESpecCompilerError& OutError);
	static UWidget* FindWidgetById(UUserWidget& RootWidget, const FString& WidgetId);
	static void AddError(FUESpecTestExecutionResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint);
};
