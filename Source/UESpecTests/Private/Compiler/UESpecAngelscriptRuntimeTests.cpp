#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UESpecAngelscriptTestHelpers.h"
#include "UESpecTestHelpers.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWindow.h"
#include "Widgets/UESpecWidgetBase.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static bool StageAsSample(FAutomationTestBase& Test, FScopedAngelscriptTestScript& OutScript)
	{
		return OutScript.StageFromTestData(Test, TEXT("as/VM_AsSample.as"), TEXT("UESpecTests/VM_AsSample.as"))
			&& OutScript.Reload(Test, TEXT("VM_AsSample"));
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

	static UWidgetBlueprint* CompileSpec(
		FAutomationTestBase& Test,
		const FString& SpecPath,
		const FString& OutputDirectory,
		FUESpecCompilerResult& OutResult,
		TArray<UObject*>& OutCleanupObjects)
	{
		UUESpecCompilerSubsystem* const Compiler = GetCompilerSubsystem(Test);
		if (!Compiler)
		{
			return nullptr;
		}

		Test.TestTrue(TEXT("Spec file exists"), FPaths::FileExists(SpecPath));
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

		Test.TestTrue(TEXT("Spec compiles"), bCompiled);
		Test.TestTrue(TEXT("Compiler reports success"), OutResult.bOk);
		Test.TestEqual(TEXT("Exactly one generated asset is returned"), OutResult.GeneratedAssets.Num(), 1);
		if (!bCompiled || OutResult.GeneratedAssets.Num() != 1)
		{
			return nullptr;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *OutResult.GeneratedAssets[0]);
		Test.TestNotNull(TEXT("Compiled WidgetBlueprint loads"), WidgetBlueprint);
		if (WidgetBlueprint)
		{
			OutCleanupObjects.Add(WidgetBlueprint);
		}

		return WidgetBlueprint;
	}

	static UWorld* GetEditorWorld(FAutomationTestBase& Test)
	{
		if (!GEditor)
		{
			Test.AddError(TEXT("GEditor is not available."));
			return nullptr;
		}

		UWorld* const World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			Test.AddError(TEXT("Editor world is not available."));
		}

		return World;
	}

	template <typename TWidgetBase>
	static TWidgetBase* CreateCompiledWidgetInstance(FAutomationTestBase& Test, UWidgetBlueprint& WidgetBlueprint, TSharedPtr<SWindow>& OutWindow)
	{
		UWorld* const World = GetEditorWorld(Test);
		if (!World)
		{
			return nullptr;
		}

		const TSubclassOf<UUserWidget> WidgetClass(WidgetBlueprint.GeneratedClass);
		TWidgetBase* const Widget = CreateWidget<TWidgetBase>(World, WidgetClass);
		Test.TestNotNull(TEXT("Widget instance allocates"), Widget);
		if (!Widget)
		{
			return nullptr;
		}

		OutWindow = SNew(SWindow).AutoCenter(EAutoCenter::None).SizingRule(ESizingRule::Autosized);
		OutWindow->SetContent(Widget->TakeWidget());
		FSlateApplication::Get().AddWindow(OutWindow.ToSharedRef(), false);
		FSlateApplication::Get().Tick();
		Widget->Tick(FGeometry(), 0.0f);
		FSlateApplication::Get().Tick();
		return Widget;
	}

	static void DestroyWindow(TSharedPtr<SWindow>& Window)
	{
		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
			FSlateApplication::Get().Tick();
			Window.Reset();
		}
	}

	static bool ReadIntProperty(const UObject& Object, const TCHAR* PropertyName, int32& OutValue)
	{
		if (const FIntProperty* const Property = FindFProperty<FIntProperty>(Object.GetClass(), FName(PropertyName)))
		{
			OutValue = Property->GetPropertyValue_InContainer(&Object);
			return true;
		}

		return false;
	}

	static bool ReadFloatProperty(const UObject& Object, const TCHAR* PropertyName, float& OutValue)
	{
		if (const FFloatProperty* const Property = FindFProperty<FFloatProperty>(Object.GetClass(), FName(PropertyName)))
		{
			OutValue = Property->GetPropertyValue_InContainer(&Object);
			return true;
		}

		if (const FDoubleProperty* const Property = FindFProperty<FDoubleProperty>(Object.GetClass(), FName(PropertyName)))
		{
			OutValue = static_cast<float>(Property->GetPropertyValue_InContainer(&Object));
			return true;
		}

		if (const FNumericProperty* const Property = FindFProperty<FNumericProperty>(Object.GetClass(), FName(PropertyName)))
		{
			if (Property->IsFloatingPoint())
			{
				OutValue = static_cast<float>(Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Object)));
				return true;
			}
		}

		return false;
	}

	static bool ReadBoolProperty(const UObject& Object, const TCHAR* PropertyName, bool& OutValue)
	{
		if (const FBoolProperty* const Property = FindFProperty<FBoolProperty>(Object.GetClass(), FName(PropertyName)))
		{
			OutValue = Property->GetPropertyValue_InContainer(&Object);
			return true;
		}

		return false;
	}

	static bool ReadTextProperty(const UObject& Object, const TCHAR* PropertyName, FString& OutValue)
	{
		if (const FTextProperty* const Property = FindFProperty<FTextProperty>(Object.GetClass(), FName(PropertyName)))
		{
			OutValue = Property->GetPropertyValue_InContainer(&Object).ToString();
			return true;
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptEventGraphCallAsMethodTest,
	"UESpec.Angelscript.EventGraph.CallAsMethod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptEventGraphCallAsMethodTest::RunTest(const FString& Parameters)
{
	FScopedAngelscriptTestScript Script;
	if (!StageAsSample(*this, Script))
	{
		return false;
	}

	const FString SpecName = TEXT("AsEventGraphCallSpec");
	const FString SpecPath = BuildTempSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "AsEventGraphCallSpec",
  "viewModel": {
    "class": "VM_AsSample",
    "scope": "PlayerState",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "Index", "type": "int" }], "pure": false }
    ]
  },
  "root": {
    "type": "Button",
    "id": "triggerButton",
    "actions": [
      { "on": "Clicked", "do": "vm.UseSkill(0)" }
    ]
  }
}
)JSON");

	TestTrue(TEXT("AS event graph spec writes to disk"), SaveTempSpecFile(SpecPath, JsonText));
	if (!FPaths::FileExists(SpecPath))
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
	UWidgetBlueprint* const WidgetBlueprint = CompileSpec(*this, SpecPath, UniqueOutputDirectory, Result, CleanupObjects);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	TSharedPtr<SWindow> Window;
	ON_SCOPE_EXIT
	{
		DestroyWindow(Window);
	};

	UUESpecWidgetBase* const WidgetInstance = CreateCompiledWidgetInstance<UUESpecWidgetBase>(*this, *WidgetBlueprint, Window);
	TestNotNull(TEXT("Compiled widget instance is created"), WidgetInstance);
	if (!WidgetInstance)
	{
		return false;
	}

	UButton* const Button = Cast<UButton>(WidgetInstance->WidgetTree ? WidgetInstance->WidgetTree->FindWidget(TEXT("triggerButton")) : nullptr);
	TestNotNull(TEXT("Compiled button is present"), Button);

	UUESpecViewModelBase* const ViewModel = WidgetInstance->GetUESpecViewModel();
	TestNotNull(TEXT("ViewModel instance is created"), ViewModel);
	if (!Button || !ViewModel)
	{
		return false;
	}

	int32 LastUsedSkillIndex = 0;
	TestTrue(TEXT("LastUsedSkillIndex is readable before click"), ReadIntProperty(*ViewModel, TEXT("LastUsedSkillIndex"), LastUsedSkillIndex));
	TestEqual(TEXT("LastUsedSkillIndex starts at -1"), LastUsedSkillIndex, -1);

	Button->OnClicked.Broadcast();
	WidgetInstance->Tick(FGeometry(), 0.0f);
	FSlateApplication::Get().Tick();

	TestTrue(TEXT("LastUsedSkillIndex is readable after click"), ReadIntProperty(*ViewModel, TEXT("LastUsedSkillIndex"), LastUsedSkillIndex));
	TestEqual(TEXT("Click routes to AngelScript UseSkill"), LastUsedSkillIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecAngelscriptBindingTextBindToAsFieldTest,
	"UESpec.Angelscript.Binding.TextBindToAsField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecAngelscriptBindingTextBindToAsFieldTest::RunTest(const FString& Parameters)
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
	UWidgetBlueprint* const WidgetBlueprint = CompileSpec(*this, GetTestDataPath(TEXT("hud-skill-bar-as.json")), UniqueOutputDirectory, Result, CleanupObjects);
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
	{
		return false;
	}

	UUESpecActivatableWidgetBase* const Defaults = Cast<UUESpecActivatableWidgetBase>(WidgetBlueprint->GeneratedClass->GetDefaultObject());
	TestNotNull(TEXT("Compiled AS HUD uses activatable widget defaults"), Defaults);
	if (!Defaults)
	{
		return false;
	}

	const UWidgetBlueprintGeneratedClass* const WidgetBlueprintClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass);
	TestTrue(TEXT("Generated AS HUD class requires native tick for manual bindings"), WidgetBlueprintClass && WidgetBlueprintClass->ClassRequiresNativeTick());
	TestEqual(TEXT("AS binding mode falls back to manual"), Defaults->BindingMode, EUESpecBindingMode::Manual);
	TestEqual(TEXT("AS HUD stores three manual bindings"), Defaults->ManualBindings.Num(), 3);

	TSharedPtr<SWindow> Window;
	ON_SCOPE_EXIT
	{
		DestroyWindow(Window);
	};

	UUESpecActivatableWidgetBase* const WidgetInstance = CreateCompiledWidgetInstance<UUESpecActivatableWidgetBase>(*this, *WidgetBlueprint, Window);
	TestNotNull(TEXT("Compiled AS HUD widget instance is created"), WidgetInstance);
	if (!WidgetInstance)
	{
		return false;
	}

	UTextBlock* const Label = Cast<UTextBlock>(WidgetInstance->WidgetTree ? WidgetInstance->WidgetTree->FindWidget(TEXT("slotQ_label")) : nullptr);
	UProgressBar* const CooldownBar = Cast<UProgressBar>(WidgetInstance->WidgetTree ? WidgetInstance->WidgetTree->FindWidget(TEXT("slotQ_cooldownBar")) : nullptr);
	UButton* const TriggerButton = Cast<UButton>(WidgetInstance->WidgetTree ? WidgetInstance->WidgetTree->FindWidget(TEXT("slotQ")) : nullptr);
	UUESpecViewModelBase* const ViewModel = WidgetInstance->GetUESpecViewModel();
	TestNotNull(TEXT("Label widget exists"), Label);
	TestNotNull(TEXT("Cooldown bar widget exists"), CooldownBar);
	TestNotNull(TEXT("Trigger button exists"), TriggerButton);
	TestNotNull(TEXT("AS HUD ViewModel instance exists"), ViewModel);
	if (!Label || !CooldownBar || !TriggerButton || !ViewModel)
	{
		return false;
	}

	WidgetInstance->RefreshUESpecBindings();
	FSlateApplication::Get().Tick();

	TestEqual(TEXT("Manual binding initializes text from AngelScript field"), Label->GetText().ToString(), FString(TEXT("AS Slash")));
	TestEqual(TEXT("Manual binding initializes percent from AngelScript field"), CooldownBar->GetPercent(), 0.0f);
	TestEqual(TEXT("Manual binding initializes visibility from AngelScript field"), CooldownBar->GetVisibility(), ESlateVisibility::Collapsed);

	TriggerButton->OnClicked.Broadcast();
	WidgetInstance->RefreshUESpecBindings();
	FSlateApplication::Get().Tick();

	int32 LastUsedSkillIndex = -1;
	float CooldownRatio = 0.0f;
	bool bShowCooldown = false;
	FString SkillLabel;
	TestTrue(TEXT("Click updates LastUsedSkillIndex on the AngelScript VM"), ReadIntProperty(*ViewModel, TEXT("LastUsedSkillIndex"), LastUsedSkillIndex));
	TestTrue(TEXT("Click updates CooldownRatio on the AngelScript VM"), ReadFloatProperty(*ViewModel, TEXT("CooldownRatio"), CooldownRatio));
	TestTrue(TEXT("Click updates bShowCooldown on the AngelScript VM"), ReadBoolProperty(*ViewModel, TEXT("bShowCooldown"), bShowCooldown));
	TestTrue(TEXT("Click updates SkillLabel on the AngelScript VM"), ReadTextProperty(*ViewModel, TEXT("SkillLabel"), SkillLabel));
	TestEqual(TEXT("LastUsedSkillIndex records the clicked skill"), LastUsedSkillIndex, 0);
	TestEqual(TEXT("CooldownRatio is updated on the AngelScript VM"), CooldownRatio, 0.75f);
	TestTrue(TEXT("bShowCooldown is updated on the AngelScript VM"), bShowCooldown);
	TestEqual(TEXT("SkillLabel is updated on the AngelScript VM"), SkillLabel, FString(TEXT("AS Skill Used")));
	TestEqual(TEXT("Manual binding refreshes label text after VM change"), Label->GetText().ToString(), FString(TEXT("AS Skill Used")));
	TestEqual(TEXT("Manual binding refreshes progress bar percent after VM change"), CooldownBar->GetPercent(), 0.75f);
	TestEqual(TEXT("Manual binding refreshes visibility after VM change"), CooldownBar->GetVisibility(), ESlateVisibility::Visible);
	return true;
}
