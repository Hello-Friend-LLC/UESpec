#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "MVVMBlueprintPin.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "ObjectTools.h"
#include "UESpecTestHelpers.h"
#include "WidgetBlueprint.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static FString BuildTempBindingSpecPath(const FString& SpecName)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Temp"), FString::Printf(TEXT("%s.json"), *SpecName));
	}

	static bool SaveTempBindingSpec(const FString& FilePath, const FString& JsonText)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		return FFileHelper::SaveStringToFile(JsonText, *FilePath);
	}

	static const UMVVMBlueprintView* GetBlueprintView(const UWidgetBlueprint& WidgetBlueprint)
	{
		const UMVVMWidgetBlueprintExtension_View* const Extension = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(&WidgetBlueprint);
		return Extension ? Extension->GetBlueprintView() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerBindingTextBindToVmFieldTest,
	"UESpec.Compiler.Binding.TextBindToVmField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerBindingTextBindToVmFieldTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("BindingTextBindSpec");
	const FString SpecPath = BuildTempBindingSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "BindingTextBindSpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "fields": [
      { "name": "SkillLabel", "type": "FText", "notify": true }
    ]
  },
  "root": {
    "type": "TextBlock",
    "id": "skillLabel",
    "content": { "bind": "SkillLabel" }
  }
}
)JSON");

	TestTrue(TEXT("Binding text spec writes to disk"), SaveTempBindingSpec(SpecPath, JsonText));

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	if (!bCompiled)
	{
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			AddError(FString::Printf(TEXT("%s @ %s: %s | %s"), *Error.Code, *Error.Path, *Error.Message, *Error.Hint));
		}
	}

	TestTrue(TEXT("Binding text spec compiles"), bCompiled);
	if (!bCompiled || Result.GeneratedAssets.Num() != 1)
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *Result.GeneratedAssets[0]);
	ON_SCOPE_EXIT
	{
		if (WidgetBlueprint)
		{
			ObjectTools::DeleteSingleObject(WidgetBlueprint, false);
		}
	};

	TestNotNull(TEXT("Generated WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	const UMVVMBlueprintView* const BlueprintView = GetBlueprintView(*WidgetBlueprint);
	TestNotNull(TEXT("WidgetBlueprint MVVM view exists"), BlueprintView);
	if (!BlueprintView)
	{
		return false;
	}

	TestEqual(TEXT("Exactly one MVVM binding is generated"), BlueprintView->GetBindings().Num(), 1);
	if (BlueprintView->GetBindings().Num() != 1)
	{
		return false;
	}

	const FMVVMBlueprintViewBinding& Binding = BlueprintView->GetBindings()[0];
	const TArray<FName> SourceFields = Binding.SourcePath.GetFieldNames(WidgetBlueprint->GeneratedClass);
	const TArray<FName> DestinationFields = Binding.DestinationPath.GetFieldNames(WidgetBlueprint->GeneratedClass);

	TestEqual(TEXT("Binding source path uses SkillLabel"), SourceFields.Num(), 1);
	TestEqual(TEXT("Binding source field is SkillLabel"), SourceFields[0], FName(TEXT("SkillLabel")));
	TestEqual(TEXT("Binding destination widget is skillLabel"), Binding.DestinationPath.GetWidgetName(), FName(TEXT("skillLabel")));
	TestEqual(TEXT("Binding destination property is Text"), DestinationFields.Num(), 1);
	TestEqual(TEXT("Binding destination field is Text"), DestinationFields[0], FName(TEXT("Text")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerBindingWithConverterTest,
	"UESpec.Compiler.Binding.WithConverter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerBindingWithConverterTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("BindingConverterSpec");
	const FString SpecPath = BuildTempBindingSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "BindingConverterSpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "fields": [
      { "name": "CooldownRatio", "type": "float", "notify": true }
    ]
  },
  "root": {
    "type": "TextBlock",
    "id": "cooldownLabel",
    "content": { "bind": "CooldownRatio", "convert": "Percent" }
  }
}
)JSON");

	TestTrue(TEXT("Binding converter spec writes to disk"), SaveTempBindingSpec(SpecPath, JsonText));

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	if (!bCompiled)
	{
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			AddError(FString::Printf(TEXT("%s @ %s: %s | %s"), *Error.Code, *Error.Path, *Error.Message, *Error.Hint));
		}
	}

	TestTrue(TEXT("Binding converter spec compiles"), bCompiled);
	if (!bCompiled || Result.GeneratedAssets.Num() != 1)
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *Result.GeneratedAssets[0]);
	ON_SCOPE_EXIT
	{
		if (WidgetBlueprint)
		{
			ObjectTools::DeleteSingleObject(WidgetBlueprint, false);
		}
	};

	TestNotNull(TEXT("Generated WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	const UMVVMBlueprintView* const BlueprintView = GetBlueprintView(*WidgetBlueprint);
	TestNotNull(TEXT("WidgetBlueprint MVVM view exists"), BlueprintView);
	if (!BlueprintView || BlueprintView->GetBindings().Num() != 1)
	{
		return false;
	}

	const FMVVMBlueprintViewBinding& Binding = BlueprintView->GetBindings()[0];
	const UMVVMBlueprintViewConversionFunction* const Conversion = Binding.Conversion.GetConversionFunction(true);
	TestNotNull(TEXT("Binding creates a source-to-destination conversion function"), Conversion);
	if (!Conversion)
	{
		return false;
	}

	const UFunction* const ConversionFunction = Conversion->GetConversionFunction().GetFunction(WidgetBlueprint);
	TestNotNull(TEXT("Conversion function resolves"), ConversionFunction);
	if (!ConversionFunction)
	{
		return false;
	}

	TestEqual(TEXT("Conversion function uses Conv_Percent"), ConversionFunction->GetName(), FString(TEXT("Conv_Percent")));

	bool bFoundCooldownRatioPin = false;
	for (const FMVVMBlueprintPin& Pin : Conversion->GetPins())
	{
		const TArray<FName> PinFields = Pin.GetPath().GetFieldNames(WidgetBlueprint->GeneratedClass);
		if (PinFields.Num() == 1 && PinFields[0] == TEXT("CooldownRatio"))
		{
			bFoundCooldownRatioPin = true;
			break;
		}
	}

	TestTrue(TEXT("Conversion input pin is wired to CooldownRatio"), bFoundCooldownRatioPin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerBindingVisibilityWithConverterTest,
	"UESpec.Compiler.Binding.VisibilityWithConverter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerBindingVisibilityWithConverterTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("BindingVisibilitySpec");
	const FString SpecPath = BuildTempBindingSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "BindingVisibilitySpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "fields": [
      { "name": "bShowCooldown", "type": "bool", "notify": true }
    ]
  },
  "root": {
    "type": "Image",
    "id": "cooldownIcon",
    "visibility": { "bind": "bShowCooldown", "convert": "BoolToVisibility" }
  }
}
)JSON");

	TestTrue(TEXT("Binding visibility spec writes to disk"), SaveTempBindingSpec(SpecPath, JsonText));

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	if (!bCompiled)
	{
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			AddError(FString::Printf(TEXT("%s @ %s: %s | %s"), *Error.Code, *Error.Path, *Error.Message, *Error.Hint));
		}
	}

	TestTrue(TEXT("Binding visibility spec compiles"), bCompiled);
	if (!bCompiled || Result.GeneratedAssets.Num() != 1)
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *Result.GeneratedAssets[0]);
	ON_SCOPE_EXIT
	{
		if (WidgetBlueprint)
		{
			ObjectTools::DeleteSingleObject(WidgetBlueprint, false);
		}
	};

	TestNotNull(TEXT("Generated WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	const UMVVMBlueprintView* const BlueprintView = GetBlueprintView(*WidgetBlueprint);
	TestNotNull(TEXT("WidgetBlueprint MVVM view exists"), BlueprintView);
	if (!BlueprintView || BlueprintView->GetBindings().Num() != 1)
	{
		return false;
	}

	const FMVVMBlueprintViewBinding& Binding = BlueprintView->GetBindings()[0];
	const TArray<FName> DestinationFields = Binding.DestinationPath.GetFieldNames(WidgetBlueprint->GeneratedClass);
	TestEqual(TEXT("Binding destination widget is cooldownIcon"), Binding.DestinationPath.GetWidgetName(), FName(TEXT("cooldownIcon")));
	TestEqual(TEXT("Binding destination property is Visibility"), DestinationFields.Num(), 1);
	TestEqual(TEXT("Binding destination field is Visibility"), DestinationFields[0], FName(TEXT("Visibility")));

	const UMVVMBlueprintViewConversionFunction* const Conversion = Binding.Conversion.GetConversionFunction(true);
	TestNotNull(TEXT("Visibility binding creates a conversion function"), Conversion);
	return true;
}
