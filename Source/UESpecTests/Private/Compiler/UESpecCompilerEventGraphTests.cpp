#include "Misc/AutomationTest.h"

#include "Components/Button.h"
#include "EdGraphSchema_K2.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UESpecTestHelpers.h"
#include "WidgetBlueprint.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static FString BuildTempEventSpecPath(const FString& SpecName)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UESpec"), TEXT("Temp"), FString::Printf(TEXT("%s.json"), *SpecName));
	}

	static bool SaveTempEventSpec(const FString& FilePath, const FString& JsonText)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		return FFileHelper::SaveStringToFile(JsonText, *FilePath);
	}

	static UK2Node_ComponentBoundEvent* FindBoundEvent(UWidgetBlueprint& WidgetBlueprint, const FName WidgetName, const FName DelegateName)
	{
		TArray<UK2Node_ComponentBoundEvent*> EventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&WidgetBlueprint, EventNodes);
		for (UK2Node_ComponentBoundEvent* EventNode : EventNodes)
		{
			if (EventNode && EventNode->GetComponentPropertyName() == WidgetName && EventNode->DelegatePropertyName == DelegateName)
			{
				return EventNode;
			}
		}

		return nullptr;
	}

	static UK2Node_CallFunction* FindCallNode(UWidgetBlueprint& WidgetBlueprint, const FString& FunctionName)
	{
		TArray<UK2Node_CallFunction*> CallNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&WidgetBlueprint, CallNodes);
		for (UK2Node_CallFunction* CallNode : CallNodes)
		{
			const UFunction* TargetFunction = CallNode ? CallNode->GetTargetFunction() : nullptr;
			if (TargetFunction && TargetFunction->GetName() == FunctionName)
			{
				return CallNode;
			}
		}

		return nullptr;
	}

	static UK2Node_IfThenElse* FindBranchNode(UWidgetBlueprint& WidgetBlueprint)
	{
		TArray<UK2Node_IfThenElse*> BranchNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&WidgetBlueprint, BranchNodes);
		return BranchNodes.Num() > 0 ? BranchNodes[0] : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerEventGraphButtonClickCallsVmTest,
	"UESpec.Compiler.EventGraph.ButtonClickCallsVm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerEventGraphButtonClickCallsVmTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("ButtonClickCallsVmSpec");
	const FString SpecPath = BuildTempEventSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "ButtonClickCallsVmSpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "index", "type": "int" }], "pure": false }
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

	TestTrue(TEXT("ButtonClickCallsVm spec writes to disk"), SaveTempEventSpec(SpecPath, JsonText));
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

	TestTrue(TEXT("ButtonClickCallsVm spec compiles"), bCompiled);
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

	UK2Node_ComponentBoundEvent* EventNode = FindBoundEvent(*WidgetBlueprint, TEXT("triggerButton"), GET_MEMBER_NAME_CHECKED(UButton, OnClicked));
	TestNotNull(TEXT("OnClicked component bound event exists"), EventNode);

	UK2Node_CallFunction* VmCallNode = FindCallNode(*WidgetBlueprint, TEXT("UseSkill"));
	TestNotNull(TEXT("ViewModel call node exists"), VmCallNode);

	if (EventNode && VmCallNode)
	{
		UEdGraphPin* EventThenPin = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
		UEdGraphPin* CallExecPin = VmCallNode->GetExecPin();
		TestTrue(TEXT("OnClicked then pin links to ViewModel exec"), EventThenPin && CallExecPin && EventThenPin->LinkedTo.Contains(CallExecPin));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerEventGraphGuardGatesExecutionTest,
	"UESpec.Compiler.EventGraph.GuardGatesExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerEventGraphGuardGatesExecutionTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("GuardGatesExecutionSpec");
	const FString SpecPath = BuildTempEventSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "GuardGatesExecutionSpec",
  "viewModel": {
    "class": "VM_SkillBar",
    "scope": "PlayerState",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "index", "type": "int" }], "pure": false },
      { "name": "IsSkillReady", "params": [{ "name": "index", "type": "int" }], "pure": true }
    ]
  },
  "root": {
    "type": "Button",
    "id": "guardedButton",
    "actions": [
      { "on": "Clicked", "do": "vm.UseSkill(0)", "guard": "vm.IsSkillReady(0)" }
    ]
  }
}
)JSON");

	TestTrue(TEXT("GuardGatesExecution spec writes to disk"), SaveTempEventSpec(SpecPath, JsonText));
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

	TestTrue(TEXT("GuardGatesExecution spec compiles"), bCompiled);
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

	UK2Node_ComponentBoundEvent* EventNode = FindBoundEvent(*WidgetBlueprint, TEXT("guardedButton"), GET_MEMBER_NAME_CHECKED(UButton, OnClicked));
	UK2Node_IfThenElse* BranchNode = FindBranchNode(*WidgetBlueprint);
	UK2Node_CallFunction* GuardCallNode = FindCallNode(*WidgetBlueprint, TEXT("IsSkillReady"));

	TestNotNull(TEXT("OnClicked component bound event exists"), EventNode);
	TestNotNull(TEXT("Guard branch node exists"), BranchNode);
	TestNotNull(TEXT("Guard ViewModel call node exists"), GuardCallNode);

	if (EventNode && BranchNode)
	{
		UEdGraphPin* EventThenPin = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
		TestTrue(TEXT("OnClicked then pin links to branch exec"), EventThenPin && EventThenPin->LinkedTo.Contains(BranchNode->GetExecPin()));
	}

	if (BranchNode && GuardCallNode)
	{
		UEdGraphPin* GuardReturnPin = GuardCallNode->GetReturnValuePin();
		TestTrue(TEXT("Guard return pin links to branch condition"), GuardReturnPin && GuardReturnPin->LinkedTo.Contains(BranchNode->GetConditionPin()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerEventGraphActionBlockOpenLayerTest,
	"UESpec.Compiler.EventGraph.ActionBlockOpenLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerEventGraphActionBlockOpenLayerTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("ActionBlockOpenLayerSpec");
	const FString SpecPath = BuildTempEventSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "HUD",
  "name": "ActionBlockOpenLayerSpec",
  "root": {
    "type": "Button",
    "id": "inventoryButton",
    "actions": [
      { "on": "Clicked", "do": { "openLayer": "InventoryPanel" } }
    ]
  }
}
)JSON");

	TestTrue(TEXT("ActionBlockOpenLayer spec writes to disk"), SaveTempEventSpec(SpecPath, JsonText));
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

	TestTrue(TEXT("ActionBlockOpenLayer spec compiles"), bCompiled);
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

	UK2Node_ComponentBoundEvent* EventNode = FindBoundEvent(*WidgetBlueprint, TEXT("inventoryButton"), GET_MEMBER_NAME_CHECKED(UButton, OnClicked));
	UK2Node_CallFunction* PushLayerNode = FindCallNode(*WidgetBlueprint, TEXT("PushLayer"));

	TestNotNull(TEXT("OnClicked component bound event exists"), EventNode);
	TestNotNull(TEXT("PushLayer call node exists"), PushLayerNode);

	if (EventNode && PushLayerNode)
	{
		UEdGraphPin* EventThenPin = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
		TestTrue(TEXT("OnClicked then pin links to PushLayer exec"), EventThenPin && EventThenPin->LinkedTo.Contains(PushLayerNode->GetExecPin()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerEventGraphActionBlockSetStateTest,
	"UESpec.Compiler.EventGraph.ActionBlockSetState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerEventGraphActionBlockSetStateTest::RunTest(const FString& Parameters)
{
	UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(*this);
	if (!Compiler)
	{
		return false;
	}

	const FString SpecName = TEXT("ActionBlockSetStateSpec");
	const FString SpecPath = BuildTempEventSpecPath(SpecName);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*SpecPath, false, true, true);
	};

	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "HUD",
  "name": "ActionBlockSetStateSpec",
  "stateMachine": {
    "initial": "Normal",
    "states": [
      { "name": "Normal" },
      { "name": "Cooldown" }
    ]
  },
  "root": {
    "type": "Button",
    "id": "slotButton",
    "actions": [
      { "on": "Clicked", "do": { "setState": "Cooldown" } }
    ]
  }
}
)JSON");

	TestTrue(TEXT("ActionBlockSetState spec writes to disk"), SaveTempEventSpec(SpecPath, JsonText));
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

	TestTrue(TEXT("ActionBlockSetState spec compiles"), bCompiled);
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

	UK2Node_ComponentBoundEvent* EventNode = FindBoundEvent(*WidgetBlueprint, TEXT("slotButton"), GET_MEMBER_NAME_CHECKED(UButton, OnClicked));
	UK2Node_CallFunction* SetStateNode = FindCallNode(*WidgetBlueprint, TEXT("SetUIState"));

	TestNotNull(TEXT("OnClicked component bound event exists"), EventNode);
	TestNotNull(TEXT("SetUIState call node exists"), SetStateNode);

	if (EventNode && SetStateNode)
	{
		UEdGraphPin* EventThenPin = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
		TestTrue(TEXT("OnClicked then pin links to SetUIState exec"), EventThenPin && EventThenPin->LinkedTo.Contains(SetStateNode->GetExecPin()));

		UEdGraphPin* StatePin = SetStateNode->FindPin(TEXT("NewState"), EGPD_Input);
		TestTrue(TEXT("SetUIState receives the target state literal"), StatePin && StatePin->DefaultValue.Contains(TEXT("Cooldown")));
	}

	return true;
}
