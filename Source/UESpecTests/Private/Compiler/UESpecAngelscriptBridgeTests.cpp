#include "Misc/AutomationTest.h"

#include "AngelscriptManager.h"
#include "Blueprint/WidgetTree.h"
#include "ClassGenerator/ASClass.h"
#include "Compiler/UESpecCompilerSubsystem.h"
#include "Compiler/UESpecCompilerTypes.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UESpecAngelscriptTestHelpers.h"
#include "UESpecTestHelpers.h"
#include "ViewModel/UESpecAngelscriptBridge.h"
#include "WidgetBlueprint.h"
#include "Widgets/UESpecWidgetBase.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static bool HasErrorCode(const FUESpecCompilerResult& Result, const FString& ExpectedCode)
	{
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			if (Error.Code == ExpectedCode)
			{
				return true;
			}
		}

		return false;
	}

	static bool HasInfoCode(const FUESpecCompilerResult& Result, const FString& ExpectedCode)
	{
		for (const FUESpecCompilerError& Info : Result.Infos)
		{
			if (Info.Code == ExpectedCode)
			{
				return true;
			}
		}

		return false;
	}

	static bool StageAsSample(FAutomationTestBase& Test, FScopedAngelscriptTestScript& OutScript)
	{
		return OutScript.StageFromTestData(Test, TEXT("as/VM_AsSample.as"), TEXT("UESpecTests/VM_AsSample.as"))
			&& OutScript.Reload(Test, TEXT("VM_AsSample"));
	}

	static UClass* ResolveAsSampleClass()
	{
		return UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("VM_AsSample"));
	}

	static bool StageWidgetExperiment(
		FAutomationTestBase& Test,
		FScopedAngelscriptTestScript& OutScript,
		const TCHAR* TestDataFileName,
		const TCHAR* RelativeScriptPath,
		const TCHAR* ExpectedClassName)
	{
		return OutScript.StageFromTestData(Test, TestDataFileName, RelativeScriptPath)
			&& OutScript.Reload(Test, ExpectedClassName);
	}

	static UWidgetBlueprint* CompileAsHudSpec(
		FAutomationTestBase& Test,
		const FString& OutputDirectory,
		FUESpecCompilerResult& OutResult,
		TArray<UObject*>& OutCleanupObjects)
	{
		UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(Test);
		if (!Compiler)
		{
			return nullptr;
		}

		const FString SpecPath = GetTestDataPath(TEXT("hud-skill-bar-as.json"));
		Test.TestTrue(TEXT("hud-skill-bar-as.json exists"), FPaths::FileExists(SpecPath));
		if (!FPaths::FileExists(SpecPath))
		{
			return nullptr;
		}

		const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, OutputDirectory, OutResult);
		if (!bCompiled)
		{
			for (const FUESpecCompilerError& Error : OutResult.Errors)
			{
				Test.AddError(FString::Printf(TEXT("%s @ %s: %s | %s"), *Error.Code, *Error.Path, *Error.Message, *Error.Hint));
			}
		}

		Test.TestTrue(TEXT("AS HUD spec compiles"), bCompiled);
		Test.TestTrue(TEXT("AS HUD compiler result is ok"), OutResult.bOk);
		Test.TestEqual(TEXT("AS HUD returns one generated asset"), OutResult.GeneratedAssets.Num(), 1);
		if (!bCompiled || OutResult.GeneratedAssets.Num() != 1)
		{
			return nullptr;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *OutResult.GeneratedAssets[0]);
		Test.TestNotNull(TEXT("Compiled AS WidgetBlueprint loads"), WidgetBlueprint);
		if (WidgetBlueprint)
		{
			OutCleanupObjects.Add(WidgetBlueprint);
		}

		return WidgetBlueprint;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptBridgeDetectAsClassTest,
	"UESpec.Angelscript.Bridge.DetectAsClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptBridgeDetectAsClassTest::RunTest(const FString& Parameters)
{
	FScopedAngelscriptTestScript Script;
	if (!StageAsSample(*this, Script))
	{
		return false;
	}

	UClass* const AsClass = ResolveAsSampleClass();
	TestNotNull(TEXT("AS sample class resolves"), AsClass);
	TestTrue(TEXT("Bridge recognizes AS-generated class"), UE::UESpec::Angelscript::IsAngelscriptGeneratedClass(AsClass));

	const FString SourcePath = UE::UESpec::Angelscript::GetAngelscriptSourcePath(AsClass);
	TestTrue(TEXT("AS source path is reported"), !SourcePath.IsEmpty());
	TestTrue(TEXT("AS source path points at staged file"), SourcePath.EndsWith(TEXT("UESpecTests/VM_AsSample.as")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptBridgeResolveByNameTest,
	"UESpec.Angelscript.Bridge.ResolveByName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptBridgeResolveByNameTest::RunTest(const FString& Parameters)
{
	FScopedAngelscriptTestScript Script;
	if (!StageAsSample(*this, Script))
	{
		return false;
	}

	UClass* const ByShortName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("VM_AsSample"));
	UClass* const ByPrefixedName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UVM_AsSample"));
	UClass* const ByModuleStyleName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UESpecTests.VM_AsSample"));

	TestNotNull(TEXT("AS class resolves by short name"), ByShortName);
	TestEqual(TEXT("AS class resolves by prefixed name"), ByPrefixedName, ByShortName);
	TestEqual(TEXT("AS class resolves by module-style name"), ByModuleStyleName, ByShortName);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptViewModelReflectMethodsTest,
	"UESpec.Angelscript.ViewModel.ReflectMethods",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptViewModelReflectMethodsTest::RunTest(const FString& Parameters)
{
	FScopedAngelscriptTestScript Script;
	if (!StageAsSample(*this, Script))
	{
		return false;
	}

	UClass* const AsClass = ResolveAsSampleClass();
	TestNotNull(TEXT("AS sample class resolves"), AsClass);
	if (!AsClass)
	{
		return false;
	}

	TestNotNull(TEXT("AS ViewModel exposes SkillLabel field"), FindFProperty<FTextProperty>(AsClass, TEXT("SkillLabel")));
	TestNotNull(TEXT("AS ViewModel exposes bSkillReady field"), FindFProperty<FBoolProperty>(AsClass, TEXT("bSkillReady")));

	TArray<FUESpecViewModelMethodDescriptor> Methods;
	UUESpecViewModelBase::DescribeViewModelClass(AsClass, Methods);

	TestTrue(TEXT("AS ViewModel exposes at least four methods"), Methods.Num() >= 4);

	const FUESpecViewModelMethodDescriptor* const UseSkillMethod = Methods.FindByPredicate([](const FUESpecViewModelMethodDescriptor& Descriptor)
	{
		return Descriptor.Name == TEXT("UseSkill");
	});
	TestNotNull(TEXT("UseSkill is reflected"), UseSkillMethod);
	if (UseSkillMethod)
	{
		TestFalse(TEXT("UseSkill is not pure"), UseSkillMethod->bPure);
		TestEqual(TEXT("UseSkill has one parameter"), UseSkillMethod->Parameters.Num(), 1);
		if (UseSkillMethod->Parameters.Num() == 1)
		{
			TestEqual(TEXT("UseSkill parameter name matches"), UseSkillMethod->Parameters[0].Name, FString(TEXT("Index")));
		}
	}

	const FUESpecViewModelMethodDescriptor* const IsSkillReadyMethod = Methods.FindByPredicate([](const FUESpecViewModelMethodDescriptor& Descriptor)
	{
		return Descriptor.Name == TEXT("IsSkillReady");
	});
	TestNotNull(TEXT("IsSkillReady is reflected"), IsSkillReadyMethod);
	if (IsSkillReadyMethod)
	{
		TestTrue(TEXT("IsSkillReady is pure"), IsSkillReadyMethod->bPure);
		TestEqual(TEXT("IsSkillReady has one parameter"), IsSkillReadyMethod->Parameters.Num(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptViewModelCompileWithAsVmTest,
	"UESpec.Angelscript.ViewModel.CompileWithAsVm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptViewModelCompileWithAsVmTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	FScopedAngelscriptTestScript Script;
	if (!StageAsSample(*this, Script))
	{
		return false;
	}

	TArray<UObject*> CleanupObjects;
	ON_SCOPE_EXIT
	{
		for (UObject* Object : CleanupObjects)
		{
			if (Object)
			{
				ObjectTools::DeleteSingleObject(Object, false);
			}
		}
	};

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UWidgetBlueprint* const WidgetBlueprint = CompileAsHudSpec(*this, UniqueOutputDirectory, Result, CleanupObjects);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	TestTrue(TEXT("Compiler emits VM_CLASS_IS_ANGELSCRIPT info"), HasInfoCode(Result, TEXT("VM_CLASS_IS_ANGELSCRIPT")));
	TestFalse(TEXT("Compiler emitted no errors"), HasErrorCode(Result, TEXT("VM_CLASS_NOT_FOUND")));

	UUESpecActivatableWidgetBase* const Defaults = Cast<UUESpecActivatableWidgetBase>(WidgetBlueprint->GeneratedClass->GetDefaultObject());
	TestNotNull(TEXT("Compiled AS HUD uses activatable widget base"), Defaults);
	if (!Defaults)
	{
		return false;
	}

	UClass* const AsClass = ResolveAsSampleClass();
	TestEqual(TEXT("Default ViewModelClass points to AS class"), Defaults->ViewModelClass.LoadSynchronous(), AsClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptBridgeResolveWidgetBaseExperimentTest,
	"UESpec.Angelscript.Bridge.ResolveWidgetBaseExperiment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptBridgeResolveWidgetBaseExperimentTest::RunTest(const FString& Parameters)
{
	FScopedAngelscriptTestScript WidgetScript;
	FScopedAngelscriptTestScript ActivatableScript;
	if (!StageWidgetExperiment(*this, WidgetScript, TEXT("as/UW_AsExperiment.as"), TEXT("UESpecTests/UW_AsExperiment.as"), TEXT("UUW_AsExperiment"))
		|| !StageWidgetExperiment(*this, ActivatableScript, TEXT("as/UAW_AsExperiment.as"), TEXT("UESpecTests/UAW_AsExperiment.as"), TEXT("UUAW_AsExperiment")))
	{
		return false;
	}

	UClass* const WidgetByScriptName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UUW_AsExperiment"));
	UClass* const WidgetByGeneratedName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UW_AsExperiment"));
	UClass* const ActivatableByScriptName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UUAW_AsExperiment"));
	UClass* const ActivatableByGeneratedName = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(TEXT("UAW_AsExperiment"));

	TestNotNull(TEXT("Widget experiment resolves by script name"), WidgetByScriptName);
	TestEqual(TEXT("Widget experiment resolves by generated-style name"), WidgetByGeneratedName, WidgetByScriptName);
	TestNotNull(TEXT("Activatable experiment resolves by script name"), ActivatableByScriptName);
	TestEqual(TEXT("Activatable experiment resolves by generated-style name"), ActivatableByGeneratedName, ActivatableByScriptName);
	TestTrue(TEXT("Widget experiment derives from UUESpecWidgetBase"), WidgetByScriptName && WidgetByScriptName->IsChildOf(UUESpecWidgetBase::StaticClass()));
	TestTrue(TEXT("Activatable experiment derives from UUESpecActivatableWidgetBase"), ActivatableByScriptName && ActivatableByScriptName->IsChildOf(UUESpecActivatableWidgetBase::StaticClass()));
	return true;
}
