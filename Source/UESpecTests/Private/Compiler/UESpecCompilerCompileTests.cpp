#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "Components/UESpecArpgWidgets.h"
#include "UESpecTestHelpers.h"
#include "WidgetBlueprint.h"
#include "Widgets/UESpecWidgetBase.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static bool CompileTestHasErrorCode(const FUESpecCompilerResult& Result, const FString& ExpectedCode)
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

	static FString BuildExpectedObjectPath(const FString& OutputDirectory, const FString& SpecName)
	{
		const FString AssetName = FString::Printf(TEXT("WBP_%s"), *SpecName);
		return FString::Printf(TEXT("%s/%s.%s"), *OutputDirectory, *AssetName, *AssetName);
	}

	static FString BuildTempSpecPath(const FString& SpecName)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Temp"), FString::Printf(TEXT("%s.json"), *SpecName));
	}

	static bool SaveTempSpecFile(const FString& FilePath, const FString& JsonText)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		return FFileHelper::SaveStringToFile(JsonText, *FilePath);
	}

	static void StripBindExpressions(const TSharedPtr<FJsonObject>& NodeObject)
	{
		if (!NodeObject.IsValid())
		{
			return;
		}

		static const TCHAR* const BindableKeys[] = { TEXT("content"), TEXT("visibility") };
		for (const TCHAR* const Key : BindableKeys)
		{
			const TSharedPtr<FJsonObject>* FieldObject = nullptr;
			if (NodeObject->TryGetObjectField(Key, FieldObject) && FieldObject && (*FieldObject).IsValid() && (*FieldObject)->HasField(TEXT("bind")))
			{
				NodeObject->RemoveField(Key);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray)
		{
			for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray)
			{
				const TSharedPtr<FJsonObject>* ChildObject = nullptr;
				if (ChildValue.IsValid() && ChildValue->TryGetObject(ChildObject) && ChildObject)
				{
					StripBindExpressions(*ChildObject);
				}
			}
		}
	}

	// hud-skill-bar.json binds indexed paths (skills[0].cooldownProgress), which the compiler rejects with UNSUPPORTED_BINDING_PATH.
	// Tests about the rest of the HUD compile a copy with every bind expression removed.
	static bool WriteBindFreeHudSkillBarSpec(FString& OutSpecPath)
	{
		FString JsonText;
		TSharedPtr<FJsonObject> RootObject;
		if (!LoadTextFile(GetTestDataPath(TEXT("hud-skill-bar.json")), JsonText) || !ParseJsonObject(JsonText, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* RootWidget = nullptr;
		if (RootObject->TryGetObjectField(TEXT("root"), RootWidget) && RootWidget)
		{
			StripBindExpressions(*RootWidget);
		}

		FString StrippedJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&StrippedJson);
		if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			return false;
		}

		OutSpecPath = BuildTempSpecPath(TEXT("hud-skill-bar-bind-free"));
		return SaveTempSpecFile(OutSpecPath, StrippedJson);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileIndexedBindPathFailsTest,
	"UESpec.Compiler.Compile.IndexedBindPathFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileIndexedBindPathFailsTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = GetTestDataPath(TEXT("hud-skill-bar.json"));
	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);

	TestFalse(TEXT("HUD example with indexed bind paths does not compile"), bCompiled);
	TestFalse(TEXT("Compiler reports bOk=false"), Result.bOk);
	TestEqual(TEXT("No generated asset is returned"), Result.GeneratedAssets.Num(), 0);
	TestFalse(TEXT("No asset is left at the deterministic path"), FPackageName::DoesPackageExist(FString::Printf(TEXT("%s/WBP_SkillBarHUD"), *UniqueOutputDirectory)));

	bool bFoundCooldownProgress = false;
	for (const FUESpecCompilerError& Error : Result.Errors)
	{
		if (Error.Code == TEXT("UNSUPPORTED_BINDING_PATH") && Error.Message.Contains(TEXT("skills[0].cooldownProgress")))
		{
			bFoundCooldownProgress = true;
		}
	}

	TestTrue(TEXT("skills[0].cooldownProgress reports UNSUPPORTED_BINDING_PATH"), bFoundCooldownProgress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileMinimalHudTest,
	"UESpec.Compiler.Compile.MinimalHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileMinimalHudTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	FString SpecPath;
	TestTrue(TEXT("bind-free hud-skill-bar.json copy is written"), WriteBindFreeHudSkillBarSpec(SpecPath));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

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

	TestTrue(TEXT("HUD example compiles"), bCompiled);
	TestTrue(TEXT("Compiler reports success"), Result.bOk);
	TestEqual(TEXT("One generated asset returned"), Result.GeneratedAssets.Num(), 1);
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
	if (!WidgetBlueprint)
	{
		return false;
	}

	TestTrue(TEXT("Generated WidgetBlueprint is up to date"), WidgetBlueprint->IsUpToDate());
	TestNotNull(TEXT("Generated WidgetTree exists"), WidgetBlueprint->WidgetTree.Get());
	if (WidgetBlueprint->WidgetTree)
	{
		TestNotNull(TEXT("Generated root widget exists"), WidgetBlueprint->WidgetTree->RootWidget.Get());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileWritesStateMachineDefaultsTest,
	"UESpec.Compiler.Compile.WritesStateMachineDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileWritesStateMachineDefaultsTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	FString SpecPath;
	TestTrue(TEXT("bind-free hud-skill-bar.json copy is written"), WriteBindFreeHudSkillBarSpec(SpecPath));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	TestTrue(TEXT("HUD example compiles for state machine default test"), bCompiled);
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

	UUESpecActivatableWidgetBase* const Defaults = Cast<UUESpecActivatableWidgetBase>(WidgetBlueprint->GeneratedClass->GetDefaultObject());
	TestNotNull(TEXT("Generated defaults use activatable widget base"), Defaults);
	if (!Defaults)
	{
		return false;
	}

	TestEqual(TEXT("State machine initial state is serialized"), Defaults->StateMachine.InitialState, FName(TEXT("Normal")));
	TestEqual(TEXT("State machine stores three states"), Defaults->StateMachine.States.Num(), 3);

	TArray<const FUESpecWidgetStateBinding*> CooldownBindings;
	Defaults->StateMachine.FindBindingsForState(TEXT("Cooldown"), CooldownBindings);

	bool bFoundSlotQOpacity = false;
	for (const FUESpecWidgetStateBinding* Binding : CooldownBindings)
	{
		if (!Binding || Binding->WidgetId != TEXT("slotQ"))
		{
			continue;
		}

		for (const FUESpecStateBindingProperty& PropertyBinding : Binding->Properties)
		{
			if (PropertyBinding.Property == TEXT("opacity") && PropertyBinding.Value == TEXT("0.6"))
			{
				bFoundSlotQOpacity = true;
				break;
			}
		}
	}

	TestTrue(TEXT("Cooldown state binding captures slotQ opacity override"), bFoundSlotQOpacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileFailureCleansUpAssetTest,
	"UESpec.Compiler.Compile.FailureCleansUpAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileFailureCleansUpAssetTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("BrokenHierarchySpec");
	const FString SpecPath = BuildTempSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "BrokenHierarchySpec",
  "root": {
    "type": "TextBlock",
    "id": "rootLabel",
    "content": "Hello",
    "children": [
      {
        "type": "TextBlock",
        "id": "childLabel",
        "content": "World"
      }
    ]
  }
}
)JSON");

	TestTrue(TEXT("Broken hierarchy spec writes to disk"), SaveTempSpecFile(SpecPath, JsonText));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ExpectedObjectPath = BuildExpectedObjectPath(UniqueOutputDirectory, SpecName);
	const FString ExpectedPackageName = FPackageName::ObjectPathToPackageName(ExpectedObjectPath);
	const FString ExpectedPackageFile = FPackageName::LongPackageNameToFilename(ExpectedPackageName, FPackageName::GetAssetPackageExtension());

	FUESpecCompilerResult Result;
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);

	TestFalse(TEXT("Broken hierarchy compile fails"), bCompiled);
	TestFalse(TEXT("Compiler reports failure"), Result.bOk);
	TestTrue(TEXT("Failure reports WIDGET_DOES_NOT_ACCEPT_CHILDREN"), CompileTestHasErrorCode(Result, TEXT("WIDGET_DOES_NOT_ACCEPT_CHILDREN")));
	TestEqual(TEXT("Failed compile does not report generated assets"), Result.GeneratedAssets.Num(), 0);
	TestNull(TEXT("Failed compile leaves no live WidgetBlueprint"), FindObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath));
	TestFalse(TEXT("Failed compile leaves no saved asset file"), FPaths::FileExists(ExpectedPackageFile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileRejectsAbstractWidgetClassTest,
	"UESpec.Compiler.Compile.RejectsAbstractWidgetClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileRejectsAbstractWidgetClassTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;

	FUESpecComponentRegistration BrokenRegistration;
	BrokenRegistration.Type = TEXT("BrokenSlot");
	BrokenRegistration.WidgetClass = UUESpecWidgetBase::StaticClass();
	RegistryOverride.SetComponents({ BrokenRegistration });

	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("AbstractWidgetSpec");
	const FString SpecPath = BuildTempSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "AbstractWidgetSpec",
  "root": {
    "type": "BrokenSlot",
    "id": "slotRoot"
  }
}
)JSON");

	TestTrue(TEXT("Abstract widget spec writes to disk"), SaveTempSpecFile(SpecPath, JsonText));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ExpectedObjectPath = BuildExpectedObjectPath(UniqueOutputDirectory, SpecName);
	const FString ExpectedPackageName = FPackageName::ObjectPathToPackageName(ExpectedObjectPath);
	const FString ExpectedPackageFile = FPackageName::LongPackageNameToFilename(ExpectedPackageName, FPackageName::GetAssetPackageExtension());

	FUESpecCompilerResult Result;
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);

	TestFalse(TEXT("Abstract widget class compile fails"), bCompiled);
	TestFalse(TEXT("Compiler reports failure"), Result.bOk);
	TestTrue(TEXT("Failure reports WIDGET_CLASS_NOT_CONSTRUCTIBLE"), CompileTestHasErrorCode(Result, TEXT("WIDGET_CLASS_NOT_CONSTRUCTIBLE")));
	TestEqual(TEXT("Failed compile does not report generated assets"), Result.GeneratedAssets.Num(), 0);
	TestNull(TEXT("Abstract widget class leaves no live WidgetBlueprint"), FindObject<UWidgetBlueprint>(nullptr, *ExpectedObjectPath));
	TestFalse(TEXT("Abstract widget class leaves no saved asset file"), FPaths::FileExists(ExpectedPackageFile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileCombatStatusExampleTest,
	"UESpec.Compiler.Compile.CombatStatusExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileCombatStatusExampleTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = GetExamplePath(TEXT("HUDs/combat-status-hud.json"));
	TestTrue(TEXT("combat-status-hud.json exists"), FPaths::FileExists(SpecPath));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	TestTrue(TEXT("Combat status example compiles"), bCompiled);
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

	TestNotNull(TEXT("Generated combat-status WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* const HealthShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerHealthShell"));
	UWidget* const ResourceShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerResourceShell"));
	TestTrue(TEXT("Health shell uses HealthBar component class"), HealthShell && HealthShell->IsA<UUESpecHealthBarWidget>());
	TestTrue(TEXT("Resource shell uses ResourceBar component class"), ResourceShell && ResourceShell->IsA<UUESpecResourceBarWidget>());
	TestNotNull(TEXT("Player name widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerName")));
	TestNotNull(TEXT("Health bar widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerHealthBar")));
	TestNotNull(TEXT("Resource value widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerResourceValue")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileAdventureOverlayExampleTest,
	"UESpec.Compiler.Compile.AdventureOverlayExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileAdventureOverlayExampleTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = GetExamplePath(TEXT("HUDs/adventure-overlay.json"));
	TestTrue(TEXT("adventure-overlay.json exists"), FPaths::FileExists(SpecPath));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	TestTrue(TEXT("Adventure overlay example compiles"), bCompiled);
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

	TestNotNull(TEXT("Generated adventure-overlay WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* const Nameplate = WidgetBlueprint->WidgetTree->FindWidget(TEXT("targetNameplate"));
	UWidget* const BuffShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("primaryBuffShell"));
	UWidget* const DamageNumber = WidgetBlueprint->WidgetTree->FindWidget(TEXT("recentDamage"));
	UWidget* const QuestEntry = WidgetBlueprint->WidgetTree->FindWidget(TEXT("trackedQuestRow"));
	TestTrue(TEXT("Nameplate uses PlayerNamePlate component class"), Nameplate && Nameplate->IsA<UUESpecPlayerNamePlateWidget>());
	TestTrue(TEXT("Buff shell uses StatusEffectIcon component class"), BuffShell && BuffShell->IsA<UUESpecStatusEffectIconWidget>());
	TestTrue(TEXT("Damage number uses DamageNumber component class"), DamageNumber && DamageNumber->IsA<UUESpecDamageNumberWidget>());
	TestTrue(TEXT("Tracked quest row uses QuestEntry component class"), QuestEntry && QuestEntry->IsA<UUESpecQuestEntryWidget>());
	TestNotNull(TEXT("Target name widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("targetName")));
	TestNotNull(TEXT("Target health bar widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("targetHealthBar")));
	TestNotNull(TEXT("Tracked quest progress widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("trackedQuestProgress")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileInventorySocialOverlayExampleTest,
	"UESpec.Compiler.Compile.InventorySocialOverlayExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileInventorySocialOverlayExampleTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = GetExamplePath(TEXT("HUDs/inventory-social-overlay.json"));
	TestTrue(TEXT("inventory-social-overlay.json exists"), FPaths::FileExists(SpecPath));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	TestTrue(TEXT("Inventory-social overlay example compiles"), bCompiled);
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

	TestNotNull(TEXT("Generated inventory-social WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* const EquipmentPanel = WidgetBlueprint->WidgetTree->FindWidget(TEXT("equipmentPanelRoot"));
	UWidget* const HelmetSlot = WidgetBlueprint->WidgetTree->FindWidget(TEXT("equippedHelmetSlot"));
	UWidget* const Tooltip = WidgetBlueprint->WidgetTree->FindWidget(TEXT("itemTooltipRoot"));
	UWidget* const ChatLine = WidgetBlueprint->WidgetTree->FindWidget(TEXT("chatLineRoot"));
	TestTrue(TEXT("Equipment panel uses EquipmentPanel component class"), EquipmentPanel && EquipmentPanel->IsA<UUESpecEquipmentPanelWidget>());
	TestTrue(TEXT("Helmet slot uses ItemSlot component class"), HelmetSlot && HelmetSlot->IsA<UUESpecItemSlotWidget>());
	TestTrue(TEXT("Tooltip uses ItemTooltip component class"), Tooltip && Tooltip->IsA<UUESpecItemTooltipWidget>());
	TestTrue(TEXT("Chat line uses ChatLine component class"), ChatLine && ChatLine->IsA<UUESpecChatLineWidget>());
	TestNotNull(TEXT("Helmet power widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("equippedHelmetPower")));
	TestNotNull(TEXT("Tooltip summary widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("itemTooltipSummary")));
	TestNotNull(TEXT("Chat message widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("chatMessage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCompileRound6ShowcaseExampleTest,
	"UESpec.Compiler.Compile.Round6ShowcaseExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCompileRound6ShowcaseExampleTest::RunTest(const FString& Parameters)
{
	FScopedRegistryOverride RegistryOverride;
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecPath = GetExamplePath(TEXT("HUDs/round6-showcase-hud.json"));
	TestTrue(TEXT("round6-showcase-hud.json exists"), FPaths::FileExists(SpecPath));
	if (!FPaths::FileExists(SpecPath))
	{
		return false;
	}

	FUESpecCompilerResult Result;
	const FString UniqueOutputDirectory = FString::Printf(TEXT("/Game/UESpecTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const bool bCompiled = Compiler->CompileJsonFileToWidgetBlueprint(SpecPath, UniqueOutputDirectory, Result);
	TestTrue(TEXT("Round 6 showcase example compiles"), bCompiled);
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

	TestNotNull(TEXT("Generated round6-showcase WidgetBlueprint loads"), WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* const PlayerHealthShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerHealthShell"));
	UWidget* const PlayerResourceShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("playerResourceShell"));
	UWidget* const PrimarySkillSlot = WidgetBlueprint->WidgetTree->FindWidget(TEXT("primarySkillSlot"));
	UWidget* const SkillAlertDot = WidgetBlueprint->WidgetTree->FindWidget(TEXT("skillAlertDot"));
	UWidget* const TargetNameplate = WidgetBlueprint->WidgetTree->FindWidget(TEXT("targetNameplate"));
	UWidget* const PrimaryBuffShell = WidgetBlueprint->WidgetTree->FindWidget(TEXT("primaryBuffShell"));
	UWidget* const RecentDamage = WidgetBlueprint->WidgetTree->FindWidget(TEXT("recentDamage"));
	UWidget* const TrackedQuestRow = WidgetBlueprint->WidgetTree->FindWidget(TEXT("trackedQuestRow"));
	UWidget* const EquipmentPanel = WidgetBlueprint->WidgetTree->FindWidget(TEXT("equipmentPanelRoot"));
	UWidget* const EquippedItemSlot = WidgetBlueprint->WidgetTree->FindWidget(TEXT("equippedItemSlot"));
	UWidget* const ItemTooltip = WidgetBlueprint->WidgetTree->FindWidget(TEXT("itemTooltipRoot"));
	UWidget* const ChatLine = WidgetBlueprint->WidgetTree->FindWidget(TEXT("chatLineRoot"));

	TestTrue(TEXT("Player health shell uses HealthBar"), PlayerHealthShell && PlayerHealthShell->IsA<UUESpecHealthBarWidget>());
	TestTrue(TEXT("Player resource shell uses ResourceBar"), PlayerResourceShell && PlayerResourceShell->IsA<UUESpecResourceBarWidget>());
	TestTrue(TEXT("Primary skill uses SkillSlot"), PrimarySkillSlot && PrimarySkillSlot->IsA<UUESpecSkillSlotWidget>());
	TestTrue(TEXT("Skill alert uses RedDot"), SkillAlertDot && SkillAlertDot->IsA<UUESpecRedDotWidget>());
	TestTrue(TEXT("Target nameplate uses PlayerNamePlate"), TargetNameplate && TargetNameplate->IsA<UUESpecPlayerNamePlateWidget>());
	TestTrue(TEXT("Primary buff uses StatusEffectIcon"), PrimaryBuffShell && PrimaryBuffShell->IsA<UUESpecStatusEffectIconWidget>());
	TestTrue(TEXT("Recent damage uses DamageNumber"), RecentDamage && RecentDamage->IsA<UUESpecDamageNumberWidget>());
	TestTrue(TEXT("Tracked quest row uses QuestEntry"), TrackedQuestRow && TrackedQuestRow->IsA<UUESpecQuestEntryWidget>());
	TestTrue(TEXT("Equipment panel uses EquipmentPanel"), EquipmentPanel && EquipmentPanel->IsA<UUESpecEquipmentPanelWidget>());
	TestTrue(TEXT("Equipped item slot uses ItemSlot"), EquippedItemSlot && EquippedItemSlot->IsA<UUESpecItemSlotWidget>());
	TestTrue(TEXT("Tooltip uses ItemTooltip"), ItemTooltip && ItemTooltip->IsA<UUESpecItemTooltipWidget>());
	TestTrue(TEXT("Chat line uses ChatLine"), ChatLine && ChatLine->IsA<UUESpecChatLineWidget>());
	TestNotNull(TEXT("Chat message widget exists"), WidgetBlueprint->WidgetTree->FindWidget(TEXT("chatMessage")));
	return true;
}
