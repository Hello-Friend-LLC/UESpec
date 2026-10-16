#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "ObjectTools.h"
#include "UESpecTestHelpers.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Widgets/UESpecWidgetBase.h"

// HF: spec -> asset identity (deterministic WBP_<name>, ownership tags, in-place update, restore on failure).
namespace
{
	using namespace UE::UESpec::Tests;

	// Five widgets; skillLabel carries the SkillLabel binding, spareButton has its own bound event.
	static const TCHAR* const IdentitySpecA = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "AssetIdentitySpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "index", "type": "int" }], "pure": false }
    ]
  },
  "root": {
    "type": "VerticalBox",
    "id": "panel",
    "children": [
      { "type": "TextBlock", "id": "skillLabel", "content": { "bind": "SkillLabel" } },
      { "type": "TextBlock", "id": "statusLabel", "content": "Ready" },
      { "type": "ProgressBar", "id": "cooldownBar", "content": { "bind": "CooldownRatio" } },
      { "type": "Button", "id": "triggerButton", "actions": [ { "on": "Clicked", "do": "vm.UseSkill(0)" } ] },
      { "type": "Button", "id": "spareButton", "actions": [ { "on": "Clicked", "do": "vm.UseSkill(1)" } ] }
    ]
  }
}
)JSON");

	// A': spareButton removed, the SkillLabel binding retargeted from skillLabel to statusLabel.
	static const TCHAR* const IdentitySpecARevised = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "AssetIdentitySpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "index", "type": "int" }], "pure": false }
    ]
  },
  "root": {
    "type": "VerticalBox",
    "id": "panel",
    "children": [
      { "type": "TextBlock", "id": "skillLabel", "content": "Static" },
      { "type": "TextBlock", "id": "statusLabel", "content": { "bind": "SkillLabel" } },
      { "type": "ProgressBar", "id": "cooldownBar", "content": { "bind": "CooldownRatio" } },
      { "type": "Button", "id": "triggerButton", "actions": [ { "on": "Clicked", "do": "vm.UseSkill(0)" } ] }
    ]
  }
}
)JSON");

	// Passes semantic validation, fails while building the tree (a TextBlock cannot take children).
	static const TCHAR* const IdentitySpecBroken = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "AssetIdentitySpec",
  "root": {
    "type": "TextBlock",
    "id": "rootLabel",
    "content": "Hello",
    "children": [ { "type": "TextBlock", "id": "childLabel", "content": "World" } ]
  }
}
)JSON");

	static FString MakeIdentityOutputDirectory()
	{
		return FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FString MakeIdentitySpecPath(const FString& Suffix)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Temp"), FString::Printf(TEXT("AssetIdentity_%s.json"), *Suffix));
	}

	// Writes JsonText to SpecPath, compiles into OutputDirectory, logs any compiler errors when bExpectSuccess.
	static bool CompileIdentitySpec(FAutomationTestBase& Test, UUESpecCompilerSubsystem& Compiler, const FString& SpecPath, const TCHAR* JsonText, const FString& OutputDirectory, FUESpecCompilerResult& OutResult, const bool bExpectSuccess = true)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(SpecPath), true);
		if (!FFileHelper::SaveStringToFile(FString(JsonText), *SpecPath))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write %s"), *SpecPath));
			return false;
		}

		const bool bCompiled = Compiler.CompileJsonFileToWidgetBlueprint(SpecPath, OutputDirectory, OutResult);
		if (!bCompiled && bExpectSuccess)
		{
			for (const FUESpecCompilerError& Error : OutResult.Errors)
			{
				Test.AddError(FString::Printf(TEXT("%s @ %s: %s | %s"), *Error.Code, *Error.Path, *Error.Message, *Error.Hint));
			}
		}
		return bCompiled;
	}

	static bool HasErrorCode(const FUESpecCompilerResult& Result, const FString& Code)
	{
		return Result.Errors.ContainsByPredicate([&Code](const FUESpecCompilerError& Error) { return Error.Code == Code; });
	}

	static const UMVVMBlueprintView* GetIdentityBlueprintView(const UWidgetBlueprint& WidgetBlueprint)
	{
		const UMVVMWidgetBlueprintExtension_View* const Extension = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(&WidgetBlueprint);
		return Extension ? Extension->GetBlueprintView() : nullptr;
	}

	static int32 CountBindingsTargeting(const UMVVMBlueprintView& View, const FName WidgetName)
	{
		int32 Count = 0;
		for (const FMVVMBlueprintViewBinding& Binding : View.GetBindings())
		{
			Count += Binding.DestinationPath.GetWidgetName() == WidgetName ? 1 : 0;
		}
		return Count;
	}

	template <typename NodeType>
	static int32 CountNodes(UWidgetBlueprint& WidgetBlueprint)
	{
		TArray<NodeType*> Nodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&WidgetBlueprint, Nodes);
		return Nodes.Num();
	}

	static int32 CountCallNodes(UWidgetBlueprint& WidgetBlueprint, const FString& FunctionName)
	{
		TArray<UK2Node_CallFunction*> CallNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&WidgetBlueprint, CallNodes);
		int32 Count = 0;
		for (const UK2Node_CallFunction* const CallNode : CallNodes)
		{
			const UFunction* const TargetFunction = CallNode ? CallNode->GetTargetFunction() : nullptr;
			Count += (TargetFunction && TargetFunction->GetName() == FunctionName) ? 1 : 0;
		}
		return Count;
	}

	static const UUESpecWidgetBase* GetGeneratedDefaults(const UWidgetBlueprint& WidgetBlueprint)
	{
		return WidgetBlueprint.GeneratedClass ? Cast<UUESpecWidgetBase>(WidgetBlueprint.GeneratedClass->GetDefaultObject()) : nullptr;
	}

	static void DeleteIdentityAsset(const FString& ObjectPath)
	{
		if (UWidgetBlueprint* const WidgetBlueprint = FindObject<UWidgetBlueprint>(nullptr, *ObjectPath))
		{
			ObjectTools::DeleteSingleObject(WidgetBlueprint, false);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAssetIdentityRecompileUpdatesInPlaceTest,
	"UESpec.Compiler.AssetIdentity.RecompileUpdatesInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAssetIdentityRecompileUpdatesInPlaceTest::RunTest(const FString& Parameters)
{
	// The tag value must match Python's hashlib.sha256 (FIPS 180-4 "abc" vector) so the backend can compare without loading.
	TestEqual(TEXT("ComputeSpecHash matches the SHA-256 'abc' vector"),
		UUESpecCompilerSubsystem::ComputeSpecHash(TEXT("abc")),
		FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));

	UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = MakeIdentitySpecPath(TEXT("Recompile"));
	const FString OutputDirectory = MakeIdentityOutputDirectory();
	const FString ExpectedObjectPath = OutputDirectory + TEXT("/WBP_AssetIdentitySpec.WBP_AssetIdentitySpec");
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
		DeleteIdentityAsset(ExpectedObjectPath);
	};

	FUESpecCompilerResult First;
	FUESpecCompilerResult Second;
	if (!CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, First) ||
		!CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, Second))
	{
		return false;
	}

	TestEqual(TEXT("First compile targets the deterministic path"), First.AssetPath, ExpectedObjectPath);
	TestEqual(TEXT("Second compile targets the same path"), Second.AssetPath, ExpectedObjectPath);
	TestEqual(TEXT("First compile creates"), First.AssetAction, FString(TEXT("created")));
	TestEqual(TEXT("Second compile updates"), Second.AssetAction, FString(TEXT("updated")));
	TestEqual(TEXT("blueprintGuid is stable across recompiles"), Second.BlueprintGuid, First.BlueprintGuid);
	TestEqual(TEXT("Identical spec yields identical hash"), Second.SpecHash, First.SpecHash);
	TestEqual(TEXT("specHash is SHA-256 of normalizedJson"), Second.SpecHash, UUESpecCompilerSubsystem::ComputeSpecHash(Second.NormalizedJson));
	TestFalse(TEXT("No WBP_AssetIdentitySpec_1 duplicate package"), FPackageName::DoesPackageExist(OutputDirectory + TEXT("/WBP_AssetIdentitySpec_1")));
	TestFalse(TEXT("No WBP_AssetIdentitySpec1 duplicate package"), FPackageName::DoesPackageExist(OutputDirectory + TEXT("/WBP_AssetIdentitySpec1")));
	TestTrue(TEXT("savedPackageFile exists on disk"), FPaths::FileExists(Second.SavedPackageFile));

	UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath);
	TestNotNull(TEXT("Updated WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint)
	{
		return false;
	}

	TestTrue(TEXT("Updated WidgetBlueprint is up to date"), WidgetBlueprint->IsUpToDate());
	const UMVVMBlueprintView* const View = GetIdentityBlueprintView(*WidgetBlueprint);
	TestNotNull(TEXT("MVVM view exists"), View);
	if (View)
	{
		TestEqual(TEXT("Recompile does not duplicate bindings"), View->GetBindings().Num(), 2);
		TestEqual(TEXT("Recompile keeps exactly one ViewModel context"), View->GetViewModels().Num(), 1);
	}
	TestEqual(TEXT("Recompile does not duplicate bound events"), CountNodes<UK2Node_ComponentBoundEvent>(*WidgetBlueprint), 2);
	TestEqual(TEXT("Recompile does not duplicate VM call nodes"), CountCallNodes(*WidgetBlueprint, TEXT("UseSkill")), 2);

	const UUESpecWidgetBase* const Defaults = GetGeneratedDefaults(*WidgetBlueprint);
	TestNotNull(TEXT("Generated CDO derives from UUESpecWidgetBase"), Defaults);
	if (Defaults)
	{
		TestEqual(TEXT("UESpecSpecName tag"), Defaults->UESpecSpecName, FString(TEXT("AssetIdentitySpec")));
		TestEqual(TEXT("UESpecSpecHash tag"), Defaults->UESpecSpecHash, Second.SpecHash);
		TestTrue(TEXT("UESpecGenerator tag names the compiler"), Defaults->UESpecGenerator.StartsWith(TEXT("UESpecCompilerSubsystem/")));
	}

	// The tags are published to the asset registry from the saved file, i.e. queryable without loading the asset.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanFilesSynchronous({ Second.SavedPackageFile }, /*bForceRescan*/ true);
	const FAssetData OnDiskAsset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ExpectedObjectPath), /*bIncludeOnlyOnDiskAssets*/ true);
	TestTrue(TEXT("On-disk asset data found"), OnDiskAsset.IsValid());
	if (OnDiskAsset.IsValid())
	{
		TestEqual(TEXT("Registry tag UESpecSpecName"), OnDiskAsset.GetTagValueRef<FString>(TEXT("UESpecSpecName")), FString(TEXT("AssetIdentitySpec")));
		TestEqual(TEXT("Registry tag UESpecSpecHash"), OnDiskAsset.GetTagValueRef<FString>(TEXT("UESpecSpecHash")), Second.SpecHash);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAssetIdentityUpdateRemovesStaleContentTest,
	"UESpec.Compiler.AssetIdentity.UpdateRemovesStaleContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAssetIdentityUpdateRemovesStaleContentTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = MakeIdentitySpecPath(TEXT("Stale"));
	const FString OutputDirectory = MakeIdentityOutputDirectory();
	const FString ExpectedObjectPath = OutputDirectory + TEXT("/WBP_AssetIdentitySpec.WBP_AssetIdentitySpec");
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
		DeleteIdentityAsset(ExpectedObjectPath);
	};

	FUESpecCompilerResult First;
	FUESpecCompilerResult Revised;
	if (!CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, First) ||
		!CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecARevised, OutputDirectory, Revised))
	{
		return false;
	}

	TestEqual(TEXT("Revised spec updates in place"), Revised.AssetAction, FString(TEXT("updated")));
	TestEqual(TEXT("blueprintGuid survives a content change"), Revised.BlueprintGuid, First.BlueprintGuid);
	TestNotEqual(TEXT("Changed spec changes the hash"), Revised.SpecHash, First.SpecHash);

	UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath);
	TestNotNull(TEXT("Updated WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	TestTrue(TEXT("Updated WidgetBlueprint is up to date"), WidgetBlueprint->IsUpToDate());

	TArray<UWidget*> Widgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
	TestEqual(TEXT("Tree holds exactly the revised widgets"), Widgets.Num(), 5);
	TestNull(TEXT("Removed widget is gone from the tree"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("spareButton")));
	TestNull(TEXT("No orphan spareButton subobject left under the WidgetTree"), FindObject<UWidget>(WidgetBlueprint->WidgetTree, TEXT("spareButton")));
	TestFalse(TEXT("Removed widget has no stale variable GUID"), WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(TEXT("spareButton")));

	const UMVVMBlueprintView* const View = GetIdentityBlueprintView(*WidgetBlueprint);
	TestNotNull(TEXT("MVVM view exists"), View);
	if (View)
	{
		TestEqual(TEXT("No duplicate bindings after update"), View->GetBindings().Num(), 2);
		TestEqual(TEXT("Retargeted binding left skillLabel"), CountBindingsTargeting(*View, TEXT("skillLabel")), 0);
		TestEqual(TEXT("Retargeted binding now drives statusLabel"), CountBindingsTargeting(*View, TEXT("statusLabel")), 1);
		TestEqual(TEXT("Exactly one ViewModel context"), View->GetViewModels().Num(), 1);
	}

	TArray<UK2Node_ComponentBoundEvent*> EventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(WidgetBlueprint, EventNodes);
	TestEqual(TEXT("Only the surviving button keeps a bound event"), EventNodes.Num(), 1);
	for (const UK2Node_ComponentBoundEvent* const EventNode : EventNodes)
	{
		TestEqual(TEXT("Bound event belongs to triggerButton"), EventNode->GetComponentPropertyName(), FName(TEXT("triggerButton")));
	}
	TestEqual(TEXT("Removed button's VM call chain is gone"), CountCallNodes(*WidgetBlueprint, TEXT("UseSkill")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAssetIdentityFailedUpdateRestoresPreviousTest,
	"UESpec.Compiler.AssetIdentity.FailedUpdateRestoresPrevious",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAssetIdentityFailedUpdateRestoresPreviousTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = MakeIdentitySpecPath(TEXT("Restore"));
	const FString OutputDirectory = MakeIdentityOutputDirectory();
	const FString ExpectedObjectPath = OutputDirectory + TEXT("/WBP_AssetIdentitySpec.WBP_AssetIdentitySpec");
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
		DeleteIdentityAsset(ExpectedObjectPath);
	};

	FUESpecCompilerResult Good;
	if (!CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, Good))
	{
		return false;
	}

	FUESpecCompilerResult Broken;
	const bool bBrokenCompiled = CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecBroken, OutputDirectory, Broken, /*bExpectSuccess*/ false);
	TestFalse(TEXT("Broken update fails"), bBrokenCompiled);
	TestTrue(TEXT("Broken update reports the build error"), HasErrorCode(Broken, TEXT("WIDGET_DOES_NOT_ACCEPT_CHILDREN")));
	TestFalse(TEXT("Restore itself succeeded"), HasErrorCode(Broken, TEXT("ASSET_RESTORE_FAILED")));
	TestEqual(TEXT("Failed update reports no generated assets"), Broken.GeneratedAssets.Num(), 0);

	UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath);
	TestNotNull(TEXT("Previous asset still loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	TestEqual(TEXT("Previous asset keeps its blueprintGuid"), WidgetBlueprint->GetBlueprintGuid().ToString(EGuidFormats::DigitsWithHyphens), Good.BlueprintGuid);
	TestNotNull(TEXT("Previous root widget is back"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("panel")));
	TestNull(TEXT("Broken tree did not survive"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("rootLabel")));
	const UUESpecWidgetBase* const Defaults = GetGeneratedDefaults(*WidgetBlueprint);
	TestTrue(TEXT("Hash tag still names the good spec"), Defaults && Defaults->UESpecSpecHash == Good.SpecHash);

	// The restored asset is still a valid update target.
	FUESpecCompilerResult Again;
	TestTrue(TEXT("Good spec compiles again after the failed update"), CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, Again));
	TestEqual(TEXT("Recompile after restore keeps the blueprintGuid"), Again.BlueprintGuid, Good.BlueprintGuid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAssetIdentityRefusesUntaggedTargetTest,
	"UESpec.Compiler.AssetIdentity.RefusesUntaggedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAssetIdentityRefusesUntaggedTargetTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = MakeIdentitySpecPath(TEXT("Untagged"));
	const FString OutputDirectory = MakeIdentityOutputDirectory();
	const FString ExpectedObjectPath = OutputDirectory + TEXT("/WBP_AssetIdentitySpec.WBP_AssetIdentitySpec");
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
		DeleteIdentityAsset(ExpectedObjectPath);
	};

	// A hand-authored WidgetBlueprint at the target path, on the same UESpec parent but without the ownership tag:
	// only the tag (not the parent class) may authorise an overwrite.
	UWidgetBlueprintFactory* const Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = UUESpecWidgetBase::StaticClass();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UWidgetBlueprint* const HandAuthored = Cast<UWidgetBlueprint>(AssetTools.CreateAsset(TEXT("WBP_AssetIdentitySpec"), OutputDirectory, UWidgetBlueprint::StaticClass(), Factory));
	TestNotNull(TEXT("Hand-authored WidgetBlueprint created"), HandAuthored);
	if (!HandAuthored)
	{
		return false;
	}
	const FGuid HandAuthoredGuid = HandAuthored->GetBlueprintGuid();

	FUESpecCompilerResult Result;
	const bool bCompiled = CompileIdentitySpec(*this, *Compiler, SpecPath, IdentitySpecA, OutputDirectory, Result, /*bExpectSuccess*/ false);
	TestFalse(TEXT("Compile refuses the untagged target"), bCompiled);
	TestTrue(TEXT("Refusal is TARGET_NOT_GENERATED"), HasErrorCode(Result, TEXT("TARGET_NOT_GENERATED")));
	TestEqual(TEXT("Refusal reports no generated assets"), Result.GeneratedAssets.Num(), 0);

	UWidgetBlueprint* const AfterRefusal = FindObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath);
	TestTrue(TEXT("Hand-authored asset is the same object"), AfterRefusal == HandAuthored);
	TestEqual(TEXT("Hand-authored asset keeps its guid"), HandAuthored->GetBlueprintGuid(), HandAuthoredGuid);
	TestTrue(TEXT("Hand-authored tree untouched (no generated root)"), !HandAuthored->WidgetTree || HandAuthored->WidgetTree->FindWidget(TEXT("panel")) == nullptr);
	return true;
}
