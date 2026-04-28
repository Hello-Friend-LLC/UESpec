#include "Compiler/UESpecEventGraphBuilder.h"

#include "Compiler/UESpecCompilerSubsystem.h"
#include "Compiler/UESpecCompilerTypes.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UESpecLayerSubsystem.h"
#include "UESpecSignalBus.h"
#include "UObject/UnrealType.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Widgets/UESpecWidgetBase.h"

namespace UE::UESpec::Editor::EventGraphBuilderPrivate
{
	struct FVmMethodReference
	{
		FString MethodName;
		TArray<FString> Arguments;
	};

	struct FEventBindingKey
	{
		FName WidgetName;
		FName DelegateName;

		friend uint32 GetTypeHash(const FEventBindingKey& Key)
		{
			return HashCombine(GetTypeHash(Key.WidgetName), GetTypeHash(Key.DelegateName));
		}

		bool operator==(const FEventBindingKey& Other) const
		{
			return WidgetName == Other.WidgetName && DelegateName == Other.DelegateName;
		}
	};

	struct FBuildContext
	{
		UWidgetBlueprint& WidgetBlueprint;
		const TSharedPtr<FJsonObject>& RootObject;
		FUESpecCompilerResult& Result;
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		UEdGraph* Graph = nullptr;
		UClass* ViewModelClass = nullptr;
		UFunction* ViewModelGetterFunction = nullptr;
		int32 NextEventY = 0;
		TMap<FEventBindingKey, UEdGraphPin*> EventTailPins;
	};

	static FString MakeChildPath(const FString& ParentPath, const FString& Key)
	{
		return ParentPath.IsEmpty() ? Key : ParentPath + TEXT(".") + Key;
	}

	static FString MakeArrayPath(const FString& ParentPath, const int32 Index)
	{
		return FString::Printf(TEXT("%s[%d]"), *ParentPath, Index);
	}

	static bool TryGetObjectField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedPtr<FJsonObject>* ObjectField = nullptr;
		if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(FieldName, ObjectField) || !ObjectField || !(*ObjectField).IsValid())
		{
			return false;
		}

		OutObject = *ObjectField;
		return true;
	}

	static void AddError(FBuildContext& Context, const FString& Code, const FString& Path, const FString& Message, const FString& Hint)
	{
		UUESpecCompilerSubsystem::AddError(Context.Result, Code, Path, Message, Hint);
	}

	static bool SplitArguments(const FString& ArgumentString, TArray<FString>& OutArguments)
	{
		OutArguments.Reset();

		FString Current;
		bool bInString = false;
		TCHAR StringDelimiter = 0;
		int32 ParenDepth = 0;
		int32 BracketDepth = 0;

		for (int32 Index = 0; Index < ArgumentString.Len(); ++Index)
		{
			const TCHAR Character = ArgumentString[Index];

			if (bInString)
			{
				Current.AppendChar(Character);
				if (Character == StringDelimiter && (Index == 0 || ArgumentString[Index - 1] != TEXT('\\')))
				{
					bInString = false;
				}
				continue;
			}

			if (Character == TEXT('"') || Character == TEXT('\''))
			{
				bInString = true;
				StringDelimiter = Character;
				Current.AppendChar(Character);
				continue;
			}

			if (Character == TEXT('('))
			{
				++ParenDepth;
				Current.AppendChar(Character);
				continue;
			}
			if (Character == TEXT(')'))
			{
				--ParenDepth;
				if (ParenDepth < 0)
				{
					return false;
				}
				Current.AppendChar(Character);
				continue;
			}
			if (Character == TEXT('['))
			{
				++BracketDepth;
				Current.AppendChar(Character);
				continue;
			}
			if (Character == TEXT(']'))
			{
				--BracketDepth;
				if (BracketDepth < 0)
				{
					return false;
				}
				Current.AppendChar(Character);
				continue;
			}

			if (Character == TEXT(',') && ParenDepth == 0 && BracketDepth == 0)
			{
				OutArguments.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}

			Current.AppendChar(Character);
		}

		if (bInString || ParenDepth != 0 || BracketDepth != 0)
		{
			return false;
		}

		const FString TrimmedCurrent = Current.TrimStartAndEnd();
		if (!TrimmedCurrent.IsEmpty())
		{
			OutArguments.Add(TrimmedCurrent);
		}

		return true;
	}

	static bool ParseVmMethodReference(const FString& ActionString, FVmMethodReference& OutMethodReference)
	{
		OutMethodReference = FVmMethodReference();

		if (!ActionString.StartsWith(TEXT("vm.")) || !ActionString.EndsWith(TEXT(")")))
		{
			return false;
		}

		const int32 OpenParenIndex = ActionString.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, 3);
		if (OpenParenIndex == INDEX_NONE || OpenParenIndex <= 3)
		{
			return false;
		}

		OutMethodReference.MethodName = ActionString.Mid(3, OpenParenIndex - 3);
		if (OutMethodReference.MethodName.IsEmpty())
		{
			return false;
		}

		const FString ArgumentsString = ActionString.Mid(OpenParenIndex + 1, ActionString.Len() - OpenParenIndex - 2).TrimStartAndEnd();
		if (!ArgumentsString.IsEmpty() && !SplitArguments(ArgumentsString, OutMethodReference.Arguments))
		{
			return false;
		}

		return true;
	}

	template <typename NodeType, typename InitializerType>
	static NodeType* SpawnNode(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY, InitializerType&& Initializer)
	{
		FGraphNodeCreator<NodeType> NodeCreator(Graph);
		NodeType* Node = NodeCreator.CreateNode();
		Node->NodePosX = NodePosX;
		Node->NodePosY = NodePosY;
		Initializer(Node);
		NodeCreator.Finalize();
		return Node;
	}

	template <typename NodeType>
	static NodeType* SpawnNode(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
	{
		return SpawnNode<NodeType>(Graph, NodePosX, NodePosY, [](NodeType*) {});
	}

	static bool TryConnect(const FBuildContext& Context, UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		return FromPin && ToPin && Context.Schema->TryCreateConnection(FromPin, ToPin);
	}

	static UK2Node_Self* CreateSelfNode(FBuildContext& Context, const int32 NodePosX, const int32 NodePosY)
	{
		return SpawnNode<UK2Node_Self>(*Context.Graph, NodePosX, NodePosY);
	}

	static UK2Node_CallFunction* CreateCallFunctionNode(FBuildContext& Context, const UFunction& Function, const int32 NodePosX, const int32 NodePosY)
	{
		return SpawnNode<UK2Node_CallFunction>(*Context.Graph, NodePosX, NodePosY, [&Function](UK2Node_CallFunction* Node)
		{
			Node->SetFromFunction(&Function);
		});
	}

	static UK2Node_DynamicCast* CreatePureCastNode(FBuildContext& Context, UClass& TargetClass, const int32 NodePosX, const int32 NodePosY)
	{
		return SpawnNode<UK2Node_DynamicCast>(*Context.Graph, NodePosX, NodePosY, [&TargetClass](UK2Node_DynamicCast* Node)
		{
			Node->TargetType = &TargetClass;
			Node->SetPurity(true);
		});
	}

	static UK2Node_IfThenElse* CreateBranchNode(FBuildContext& Context, const int32 NodePosX, const int32 NodePosY)
	{
		return SpawnNode<UK2Node_IfThenElse>(*Context.Graph, NodePosX, NodePosY);
	}

	static UK2Node_ComponentBoundEvent* CreateBoundEventNode(FBuildContext& Context, FObjectProperty& ComponentProperty, FMulticastDelegateProperty& DelegateProperty, const int32 NodePosY)
	{
		return SpawnNode<UK2Node_ComponentBoundEvent>(*Context.Graph, 0, NodePosY, [&ComponentProperty, &DelegateProperty](UK2Node_ComponentBoundEvent* Node)
		{
			Node->InitializeComponentBoundEventParams(&ComponentProperty, &DelegateProperty);
		});
	}

	static bool TryResolveDelegateProperty(UClass* WidgetClass, const FString& EventName, FName& OutDelegateName)
	{
		OutDelegateName = NAME_None;
		if (!WidgetClass)
		{
			return false;
		}

		if (EventName == TEXT("Clicked"))
		{
			if (WidgetClass->IsChildOf(UCheckBox::StaticClass()))
			{
				OutDelegateName = GET_MEMBER_NAME_CHECKED(UCheckBox, OnCheckStateChanged);
				return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
			}

			OutDelegateName = GET_MEMBER_NAME_CHECKED(UButton, OnClicked);
			if (FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName))
			{
				return true;
			}

			OutDelegateName = TEXT("OnClicked");
			return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
		}
		if (EventName == TEXT("Hovered"))
		{
			OutDelegateName = GET_MEMBER_NAME_CHECKED(UButton, OnHovered);
			if (FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName))
			{
				return true;
			}

			OutDelegateName = TEXT("OnHovered");
			return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
		}
		if (EventName == TEXT("Unhovered"))
		{
			OutDelegateName = GET_MEMBER_NAME_CHECKED(UButton, OnUnhovered);
			if (FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName))
			{
				return true;
			}

			OutDelegateName = TEXT("OnUnhovered");
			return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
		}
		if (EventName == TEXT("Pressed"))
		{
			OutDelegateName = GET_MEMBER_NAME_CHECKED(UButton, OnPressed);
			return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
		}
		if (EventName == TEXT("Released"))
		{
			OutDelegateName = GET_MEMBER_NAME_CHECKED(UButton, OnReleased);
			return FindFProperty<FMulticastDelegateProperty>(WidgetClass, OutDelegateName) != nullptr;
		}

		return false;
	}

	static bool ApplyLiteralValueToPin(FBuildContext& Context, UEdGraphPin& TargetPin, FString RawArgument, const FString& ActionPath)
	{
		RawArgument = RawArgument.TrimStartAndEnd();
		if (RawArgument.Len() >= 2 && ((RawArgument.StartsWith(TEXT("\"")) && RawArgument.EndsWith(TEXT("\""))) || (RawArgument.StartsWith(TEXT("'")) && RawArgument.EndsWith(TEXT("'")))))
		{
			RawArgument = RawArgument.Mid(1, RawArgument.Len() - 2);
		}

		bool bMarkAsModified = false;
		if (TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_String ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Name ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Int ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Int64 ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Real ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Float ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Double ||
			TargetPin.PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		{
			Context.Schema->TrySetDefaultValue(TargetPin, RawArgument, bMarkAsModified);
			return true;
		}

		AddError(
			Context,
			TEXT("UNSUPPORTED_ACTION_ARGUMENT"),
			ActionPath,
			FString::Printf(TEXT("Argument '%s' targets unsupported pin type '%s'."), *RawArgument, *TargetPin.PinType.PinCategory.ToString()),
			TEXT("Use literal bool/int/float/string arguments for v0.2 action generation."));
		return false;
	}

	static bool ApplyArgumentsToCallNode(FBuildContext& Context, UK2Node_CallFunction& CallNode, const UFunction& Function, const TArray<FString>& Arguments, const FString& ActionPath)
	{
		int32 ArgumentIndex = 0;
		for (TFieldIterator<FProperty> PropertyIt(&Function); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Parm) ||
				Property->HasAnyPropertyFlags(CPF_ReturnParm) ||
				(Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm)))
			{
				continue;
			}

			if (ArgumentIndex >= Arguments.Num())
			{
				break;
			}

			UEdGraphPin* ArgumentPin = CallNode.FindPin(Property->GetFName(), EGPD_Input);
			if (!ArgumentPin)
			{
				AddError(
					Context,
					TEXT("ACTION_ARGUMENT_PIN_MISSING"),
					ActionPath,
					FString::Printf(TEXT("Could not find input pin '%s' on function '%s'."), *Property->GetName(), *Function.GetName()),
					TEXT("Check the reflected ViewModel signature and rebuild the action."));
				return false;
			}

			if (!ApplyLiteralValueToPin(Context, *ArgumentPin, Arguments[ArgumentIndex], ActionPath))
			{
				return false;
			}

			++ArgumentIndex;
		}

		if (ArgumentIndex != Arguments.Num())
		{
			AddError(
				Context,
				TEXT("ACTION_ARGUMENT_COUNT_MISMATCH"),
				ActionPath,
				FString::Printf(TEXT("Action supplied %d arguments but function '%s' accepts fewer supported inputs."), Arguments.Num(), *Function.GetName()),
				TEXT("Match the action argument list to the reflected ViewModel method signature."));
			return false;
		}

		return true;
	}

	static bool BuildViewModelObjectPin(FBuildContext& Context, const int32 NodePosY, UEdGraphPin*& OutViewModelPin)
	{
		OutViewModelPin = nullptr;

		if (!Context.ViewModelGetterFunction || !Context.ViewModelClass)
		{
			AddError(
				Context,
				TEXT("ACTION_VIEWMODEL_NOT_CONFIGURED"),
				TEXT("viewModel.class"),
				TEXT("ViewModel action generation requires a valid ViewModel class and runtime getter."),
				TEXT("Declare viewModel.class in the UISpec and generate against a UESpec widget base."));
			return false;
		}

		UK2Node_Self* SelfNode = CreateSelfNode(Context, 80, NodePosY);
		UK2Node_CallFunction* GetterNode = CreateCallFunctionNode(Context, *Context.ViewModelGetterFunction, 300, NodePosY);
		UK2Node_DynamicCast* CastNode = CreatePureCastNode(Context, *Context.ViewModelClass, 540, NodePosY);

		if (UEdGraphPin* GetterSelfPin = GetterNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input))
		{
			TryConnect(Context, SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), GetterSelfPin);
		}

		if (!TryConnect(Context, GetterNode->GetReturnValuePin(), CastNode->GetCastSourcePin()))
		{
			AddError(
				Context,
				TEXT("ACTION_VIEWMODEL_CAST_CONNECT_FAILED"),
				TEXT("viewModel.class"),
				TEXT("Failed to connect GetUESpecViewModel() to the generated cast node."),
				TEXT("Check the widget base runtime getter signature."));
			return false;
		}

		OutViewModelPin = CastNode->GetCastResultPin();
		return OutViewModelPin != nullptr;
	}

	static bool BuildVmCall(FBuildContext& Context, UEdGraphPin* ExecInputPin, const FVmMethodReference& MethodReference, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = nullptr;

		if (!Context.ViewModelClass)
		{
			AddError(
				Context,
				TEXT("ACTION_VIEWMODEL_NOT_CONFIGURED"),
				ActionPath,
				TEXT("This action references vm.* but the UISpec has no resolved ViewModel class."),
				TEXT("Declare viewModel.class in the UISpec before using vm.Method(...) actions."));
			return false;
		}

		UFunction* TargetFunction = Context.ViewModelClass->FindFunctionByName(FName(*MethodReference.MethodName));
		if (!TargetFunction)
		{
			AddError(
				Context,
				TEXT("ACTION_VM_METHOD_NOT_FOUND"),
				ActionPath,
				FString::Printf(TEXT("ViewModel method '%s' was not found on class '%s'."), *MethodReference.MethodName, *Context.ViewModelClass->GetName()),
				TEXT("Update the UISpec method reference or the ViewModel class."));
			return false;
		}

		UEdGraphPin* ViewModelPin = nullptr;
		if (!BuildViewModelObjectPin(Context, Context.NextEventY, ViewModelPin))
		{
			return false;
		}

		UK2Node_CallFunction* CallNode = CreateCallFunctionNode(Context, *TargetFunction, 820, Context.NextEventY);
		Context.NextEventY += 240;

		if (!ApplyArgumentsToCallNode(Context, *CallNode, *TargetFunction, MethodReference.Arguments, ActionPath))
		{
			return false;
		}

		if (UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input))
		{
			if (!TryConnect(Context, ViewModelPin, SelfPin))
			{
				AddError(
					Context,
					TEXT("ACTION_VM_SELF_CONNECT_FAILED"),
					ActionPath,
					FString::Printf(TEXT("Failed to connect generated ViewModel cast to '%s'."), *TargetFunction->GetName()),
					TEXT("Check the ViewModel class and runtime getter return type."));
				return false;
			}
		}

		if (ExecInputPin && !TryConnect(Context, ExecInputPin, CallNode->GetExecPin()))
		{
			AddError(
				Context,
				TEXT("ACTION_EXEC_CONNECT_FAILED"),
				ActionPath,
				FString::Printf(TEXT("Failed to connect exec flow into '%s'."), *TargetFunction->GetName()),
				TEXT("Inspect the generated EventGraph for duplicate or incompatible pins."));
			return false;
		}

		OutThenPin = CallNode->GetThenPin();
		return true;
	}

	static bool BuildGuardCondition(FBuildContext& Context, const FString& GuardString, const FString& ActionPath, UEdGraphPin*& OutConditionPin)
	{
		OutConditionPin = nullptr;

		FVmMethodReference MethodReference;
		if (!ParseVmMethodReference(GuardString, MethodReference))
		{
			AddError(
				Context,
				TEXT("ACTION_GUARD_NOT_SUPPORTED"),
				ActionPath,
				FString::Printf(TEXT("Guard '%s' is not a supported vmMethodRef."), *GuardString),
				TEXT("Use a boolean-returning vm.Method(...) guard for v0.2."));
			return false;
		}

		if (!Context.ViewModelClass)
		{
			AddError(
				Context,
				TEXT("ACTION_VIEWMODEL_NOT_CONFIGURED"),
				ActionPath,
				TEXT("This guard references vm.* but the UISpec has no resolved ViewModel class."),
				TEXT("Declare viewModel.class in the UISpec before using vm.Method(...) guards."));
			return false;
		}

		UFunction* GuardFunction = Context.ViewModelClass->FindFunctionByName(FName(*MethodReference.MethodName));
		if (!GuardFunction)
		{
			AddError(
				Context,
				TEXT("ACTION_GUARD_METHOD_NOT_FOUND"),
				ActionPath,
				FString::Printf(TEXT("Guard method '%s' was not found on class '%s'."), *MethodReference.MethodName, *Context.ViewModelClass->GetName()),
				TEXT("Update the UISpec guard or the ViewModel class."));
			return false;
		}

		if (!CastField<FBoolProperty>(GuardFunction->GetReturnProperty()))
		{
			AddError(
				Context,
				TEXT("ACTION_GUARD_NOT_BOOLEAN"),
				ActionPath,
				FString::Printf(TEXT("Guard method '%s' must return bool."), *GuardFunction->GetName()),
				TEXT("Use a boolean-returning ViewModel method in guard."));
			return false;
		}

		UEdGraphPin* ViewModelPin = nullptr;
		if (!BuildViewModelObjectPin(Context, Context.NextEventY, ViewModelPin))
		{
			return false;
		}

		UK2Node_CallFunction* GuardNode = CreateCallFunctionNode(Context, *GuardFunction, 820, Context.NextEventY);
		Context.NextEventY += 240;

		if (!ApplyArgumentsToCallNode(Context, *GuardNode, *GuardFunction, MethodReference.Arguments, ActionPath))
		{
			return false;
		}

		if (UEdGraphPin* SelfPin = GuardNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input))
		{
			if (!TryConnect(Context, ViewModelPin, SelfPin))
			{
				AddError(
					Context,
					TEXT("ACTION_VM_SELF_CONNECT_FAILED"),
					ActionPath,
					FString::Printf(TEXT("Failed to connect generated ViewModel cast to guard '%s'."), *GuardFunction->GetName()),
					TEXT("Check the ViewModel class and runtime getter return type."));
				return false;
			}
		}

		OutConditionPin = GuardNode->GetReturnValuePin();
		return OutConditionPin != nullptr;
	}

	static bool BuildGetGameInstanceSubsystem(FBuildContext& Context, UClass& SubsystemClass, const int32 NodePosY, UEdGraphPin*& OutSubsystemPin)
	{
		OutSubsystemPin = nullptr;

		UFunction* GetSubsystemFunction = USubsystemBlueprintLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USubsystemBlueprintLibrary, GetGameInstanceSubsystem));
		if (!GetSubsystemFunction)
		{
			return false;
		}

		UK2Node_Self* SelfNode = CreateSelfNode(Context, 80, NodePosY);
		UK2Node_CallFunction* GetSubsystemNode = CreateCallFunctionNode(Context, *GetSubsystemFunction, 320, NodePosY);
		UK2Node_DynamicCast* CastNode = CreatePureCastNode(Context, SubsystemClass, 560, NodePosY);

		if (UEdGraphPin* ClassPin = GetSubsystemNode->FindPin(TEXT("Class"), EGPD_Input))
		{
			bool bMarkAsModified = false;
			Context.Schema->TrySetDefaultObject(*ClassPin, &SubsystemClass, bMarkAsModified);
		}

		if (UEdGraphPin* ContextObjectPin = GetSubsystemNode->FindPin(TEXT("ContextObject"), EGPD_Input))
		{
			TryConnect(Context, SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), ContextObjectPin);
		}

		if (!TryConnect(Context, GetSubsystemNode->GetReturnValuePin(), CastNode->GetCastSourcePin()))
		{
			return false;
		}

		OutSubsystemPin = CastNode->GetCastResultPin();
		return OutSubsystemPin != nullptr;
	}

	static bool BuildOpenLayerAction(FBuildContext& Context, UEdGraphPin* ExecInputPin, const FString& LayerName, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = nullptr;

		UEdGraphPin* SubsystemPin = nullptr;
		if (!BuildGetGameInstanceSubsystem(Context, *UUESpecLayerSubsystem::StaticClass(), Context.NextEventY, SubsystemPin))
		{
			AddError(Context, TEXT("ACTION_LAYER_SUBSYSTEM_UNAVAILABLE"), ActionPath, TEXT("Failed to build GetGameInstanceSubsystem(UUESpecLayerSubsystem)."), TEXT("Ensure UUESpecLayerSubsystem is available in runtime and editor modules."));
			return false;
		}

		UFunction* PushLayerFunction = UUESpecLayerSubsystem::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UUESpecLayerSubsystem, PushLayer));
		if (!PushLayerFunction)
		{
			AddError(Context, TEXT("ACTION_LAYER_FUNCTION_NOT_FOUND"), ActionPath, TEXT("UUESpecLayerSubsystem::PushLayer is not available."), TEXT("Implement PushLayer on the runtime subsystem before compiling openLayer actions."));
			return false;
		}

		UK2Node_CallFunction* PushLayerNode = CreateCallFunctionNode(Context, *PushLayerFunction, 620, Context.NextEventY);
		Context.NextEventY += 240;

		if (!TryConnect(Context, SubsystemPin, PushLayerNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)))
		{
			AddError(Context, TEXT("ACTION_LAYER_SELF_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect the generated layer subsystem node to PushLayer."), TEXT("Check the runtime subsystem signature."));
			return false;
		}

		if (UEdGraphPin* LayerPin = PushLayerNode->FindPin(TEXT("LayerName"), EGPD_Input))
		{
			if (!ApplyLiteralValueToPin(Context, *LayerPin, LayerName, ActionPath))
			{
				return false;
			}
		}

		if (!TryConnect(Context, ExecInputPin, PushLayerNode->GetExecPin()))
		{
			AddError(Context, TEXT("ACTION_EXEC_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect exec flow into PushLayer."), TEXT("Inspect the generated EventGraph for incompatible pins."));
			return false;
		}

		OutThenPin = PushLayerNode->GetThenPin();
		return true;
	}

	static bool BuildCloseLayerAction(FBuildContext& Context, UEdGraphPin* ExecInputPin, const FString& LayerName, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = nullptr;

		UEdGraphPin* SubsystemPin = nullptr;
		if (!BuildGetGameInstanceSubsystem(Context, *UUESpecLayerSubsystem::StaticClass(), Context.NextEventY, SubsystemPin))
		{
			AddError(Context, TEXT("ACTION_LAYER_SUBSYSTEM_UNAVAILABLE"), ActionPath, TEXT("Failed to build GetGameInstanceSubsystem(UUESpecLayerSubsystem)."), TEXT("Ensure UUESpecLayerSubsystem is available in runtime and editor modules."));
			return false;
		}

		const bool bCloseNamedLayer = !LayerName.IsEmpty();
		UFunction* CloseLayerFunction = UUESpecLayerSubsystem::StaticClass()->FindFunctionByName(
			bCloseNamedLayer
				? GET_FUNCTION_NAME_CHECKED(UUESpecLayerSubsystem, CloseLayerByName)
				: GET_FUNCTION_NAME_CHECKED(UUESpecLayerSubsystem, CloseTopLayer));
		if (!CloseLayerFunction)
		{
			AddError(Context, TEXT("ACTION_LAYER_FUNCTION_NOT_FOUND"), ActionPath, TEXT("Required UUESpecLayerSubsystem close-layer function is not available."), TEXT("Implement the layer subsystem close function before compiling closeLayer actions."));
			return false;
		}

		UK2Node_CallFunction* CloseLayerNode = CreateCallFunctionNode(Context, *CloseLayerFunction, 620, Context.NextEventY);
		Context.NextEventY += 240;

		if (!TryConnect(Context, SubsystemPin, CloseLayerNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)))
		{
			AddError(Context, TEXT("ACTION_LAYER_SELF_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect the generated layer subsystem node to closeLayer."), TEXT("Check the runtime subsystem signature."));
			return false;
		}

		if (bCloseNamedLayer)
		{
			if (UEdGraphPin* LayerPin = CloseLayerNode->FindPin(TEXT("LayerName"), EGPD_Input))
			{
				if (!ApplyLiteralValueToPin(Context, *LayerPin, LayerName, ActionPath))
				{
					return false;
				}
			}
		}

		if (!TryConnect(Context, ExecInputPin, CloseLayerNode->GetExecPin()))
		{
			AddError(Context, TEXT("ACTION_EXEC_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect exec flow into closeLayer."), TEXT("Inspect the generated EventGraph for incompatible pins."));
			return false;
		}

		OutThenPin = CloseLayerNode->GetThenPin();
		return true;
	}

	static bool BuildSetStateAction(FBuildContext& Context, UEdGraphPin* ExecInputPin, const FString& StateName, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = nullptr;

		UFunction* SetStateFunction = Context.WidgetBlueprint.ParentClass
			? Context.WidgetBlueprint.ParentClass->FindFunctionByName(TEXT("SetUIState"))
			: nullptr;
		if (!SetStateFunction)
		{
			AddError(Context, TEXT("ACTION_SET_STATE_UNAVAILABLE"), ActionPath, TEXT("The generated widget base class does not expose SetUIState(FName)."), TEXT("Compile this UISpec against a UESpec widget base that supports state transitions."));
			return false;
		}

		UK2Node_Self* SelfNode = CreateSelfNode(Context, 80, Context.NextEventY);
		UK2Node_CallFunction* SetStateNode = CreateCallFunctionNode(Context, *SetStateFunction, 360, Context.NextEventY);
		Context.NextEventY += 240;

		if (UEdGraphPin* SelfPin = SetStateNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input))
		{
			if (!TryConnect(Context, SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), SelfPin))
			{
				AddError(Context, TEXT("ACTION_SET_STATE_SELF_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect the generated self node to SetUIState."), TEXT("Check the widget base state machine function signature."));
				return false;
			}
		}

		if (UEdGraphPin* StatePin = SetStateNode->FindPin(TEXT("NewState"), EGPD_Input))
		{
			if (!ApplyLiteralValueToPin(Context, *StatePin, StateName, ActionPath))
			{
				return false;
			}
		}

		if (!TryConnect(Context, ExecInputPin, SetStateNode->GetExecPin()))
		{
			AddError(Context, TEXT("ACTION_EXEC_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect exec flow into SetUIState."), TEXT("Inspect the generated EventGraph for incompatible pins."));
			return false;
		}

		OutThenPin = SetStateNode->GetThenPin();
		return true;
	}

	static bool BuildEmitSignalAction(FBuildContext& Context, UEdGraphPin* ExecInputPin, const FString& SignalName, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = nullptr;

		UEdGraphPin* SubsystemPin = nullptr;
		if (!BuildGetGameInstanceSubsystem(Context, *UUESpecSignalBus::StaticClass(), Context.NextEventY, SubsystemPin))
		{
			AddError(Context, TEXT("ACTION_SIGNAL_BUS_UNAVAILABLE"), ActionPath, TEXT("Failed to build GetGameInstanceSubsystem(UUESpecSignalBus)."), TEXT("Ensure UUESpecSignalBus is available in runtime and editor modules."));
			return false;
		}

		UFunction* BroadcastFunction = UUESpecSignalBus::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UUESpecSignalBus, Broadcast));
		if (!BroadcastFunction)
		{
			AddError(Context, TEXT("ACTION_SIGNAL_FUNCTION_NOT_FOUND"), ActionPath, TEXT("UUESpecSignalBus::Broadcast is not available."), TEXT("Implement Broadcast on the runtime signal bus before compiling emitSignal actions."));
			return false;
		}

		UK2Node_CallFunction* BroadcastNode = CreateCallFunctionNode(Context, *BroadcastFunction, 620, Context.NextEventY);
		Context.NextEventY += 240;

		if (!TryConnect(Context, SubsystemPin, BroadcastNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)))
		{
			AddError(Context, TEXT("ACTION_SIGNAL_SELF_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect the generated signal bus node to Broadcast."), TEXT("Check the runtime subsystem signature."));
			return false;
		}

		if (UEdGraphPin* SignalPin = BroadcastNode->FindPin(TEXT("SignalName"), EGPD_Input))
		{
			if (!ApplyLiteralValueToPin(Context, *SignalPin, SignalName, ActionPath))
			{
				return false;
			}
		}

		if (!TryConnect(Context, ExecInputPin, BroadcastNode->GetExecPin()))
		{
			AddError(Context, TEXT("ACTION_EXEC_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect exec flow into emitSignal."), TEXT("Inspect the generated EventGraph for incompatible pins."));
			return false;
		}

		OutThenPin = BroadcastNode->GetThenPin();
		return true;
	}

	static bool BuildActionDo(FBuildContext& Context, UEdGraphPin* ExecInputPin, const TSharedPtr<FJsonValue>& DoValue, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = ExecInputPin;
		if (!DoValue.IsValid())
		{
			return true;
		}

		if (DoValue->Type == EJson::String)
		{
			FVmMethodReference MethodReference;
			if (!ParseVmMethodReference(DoValue->AsString(), MethodReference))
			{
				AddError(
					Context,
					TEXT("INVALID_VM_METHOD_REF"),
					ActionPath,
					FString::Printf(TEXT("Action 'do' value '%s' is not a valid vmMethodRef."), *DoValue->AsString()),
					TEXT("Use vm.MethodName(...) or a supported action block."));
				return false;
			}

			return BuildVmCall(Context, ExecInputPin, MethodReference, ActionPath, OutThenPin);
		}

		if (DoValue->Type != EJson::Object)
		{
			AddError(
				Context,
				TEXT("INVALID_VM_METHOD_REF"),
				ActionPath,
				TEXT("Action 'do' must be either a vmMethodRef string or an actionBlock object."),
				TEXT("Replace the value with vm.MethodName(...) or a supported action block."));
			return false;
		}

		const TSharedPtr<FJsonObject> ActionBlock = DoValue->AsObject();
		if (!ActionBlock.IsValid())
		{
			return false;
		}

		FString PrimitiveValue;
		if (ActionBlock->TryGetStringField(TEXT("openLayer"), PrimitiveValue))
		{
			return BuildOpenLayerAction(Context, ExecInputPin, PrimitiveValue, ActionPath, OutThenPin);
		}

		if (ActionBlock->HasField(TEXT("closeLayer")))
		{
			ActionBlock->TryGetStringField(TEXT("closeLayer"), PrimitiveValue);
			return BuildCloseLayerAction(Context, ExecInputPin, PrimitiveValue, ActionPath, OutThenPin);
		}

		if (ActionBlock->TryGetStringField(TEXT("setState"), PrimitiveValue))
		{
			return BuildSetStateAction(Context, ExecInputPin, PrimitiveValue, ActionPath, OutThenPin);
		}

		if (ActionBlock->TryGetStringField(TEXT("emitSignal"), PrimitiveValue))
		{
			return BuildEmitSignalAction(Context, ExecInputPin, PrimitiveValue, ActionPath, OutThenPin);
		}

		if (ActionBlock->TryGetStringField(TEXT("callVM"), PrimitiveValue))
		{
			FVmMethodReference MethodReference;
			if (!ParseVmMethodReference(PrimitiveValue, MethodReference))
			{
				AddError(
					Context,
					TEXT("INVALID_VM_METHOD_REF"),
					ActionPath,
					FString::Printf(TEXT("Action block callVM value '%s' is not a valid vmMethodRef."), *PrimitiveValue),
					TEXT("Use vm.MethodName(...) inside callVM."));
				return false;
			}

			return BuildVmCall(Context, ExecInputPin, MethodReference, ActionPath, OutThenPin);
		}

		if (ActionBlock->HasField(TEXT("playAnim")))
		{
			AddError(
				Context,
				TEXT("ACTION_PRIMITIVE_NOT_IMPLEMENTED"),
				ActionPath,
				TEXT("This action block primitive is declared in schema but is not implemented in the compiler yet."),
				TEXT("Continue with openLayer/closeLayer/setState/emitSignal/callVM for the current compiler batch, or implement the remaining runtime hooks before using this primitive."));
			return false;
		}

		AddError(
			Context,
			TEXT("ACTION_BLOCK_EMPTY"),
			ActionPath,
			TEXT("actionBlock must contain one supported primitive."),
			TEXT("Use one of openLayer, closeLayer, setState, emitSignal, or callVM for the current compiler batch."));
		return false;
	}

	static bool BuildSingleAction(FBuildContext& Context, UEdGraphPin* ExecInputPin, const TSharedPtr<FJsonObject>& ActionObject, const FString& ActionPath, UEdGraphPin*& OutThenPin)
	{
		OutThenPin = ExecInputPin;
		if (!ActionObject.IsValid())
		{
			return true;
		}

		UEdGraphPin* BodyExecPin = ExecInputPin;

		FString GuardString;
		if (ActionObject->TryGetStringField(TEXT("guard"), GuardString) && !GuardString.IsEmpty())
		{
			UK2Node_IfThenElse* BranchNode = CreateBranchNode(Context, 320, Context.NextEventY);
			Context.NextEventY += 220;

			if (!TryConnect(Context, ExecInputPin, BranchNode->GetExecPin()))
			{
				AddError(Context, TEXT("ACTION_EXEC_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect action guard branch."), TEXT("Inspect the generated EventGraph for incompatible pins."));
				return false;
			}

			UEdGraphPin* GuardConditionPin = nullptr;
			if (!BuildGuardCondition(Context, GuardString, MakeChildPath(ActionPath, TEXT("guard")), GuardConditionPin))
			{
				return false;
			}

			if (!TryConnect(Context, GuardConditionPin, BranchNode->GetConditionPin()))
			{
				AddError(Context, TEXT("ACTION_GUARD_CONNECT_FAILED"), ActionPath, TEXT("Failed to connect the generated guard condition to the branch node."), TEXT("Ensure the guard method returns bool."));
				return false;
			}

			BodyExecPin = BranchNode->GetThenPin();
			OutThenPin = BranchNode->GetElsePin();
		}

		const TSharedPtr<FJsonValue>* DoValue = ActionObject->Values.Find(TEXT("do"));
		if (!DoValue || !DoValue->IsValid())
		{
			return true;
		}

		UEdGraphPin* ActionThenPin = nullptr;
		if (!BuildActionDo(Context, BodyExecPin, *DoValue, MakeChildPath(ActionPath, TEXT("do")), ActionThenPin))
		{
			return false;
		}

		OutThenPin = ActionThenPin ? ActionThenPin : BodyExecPin;
		return true;
	}

	static UK2Node_ComponentBoundEvent* FindOrCreateBoundEvent(FBuildContext& Context, UWidget& Widget, const FString& EventName, const FString& ActionPath, FName& OutDelegateName)
	{
		OutDelegateName = NAME_None;

		FName DelegateName;
		if (!TryResolveDelegateProperty(Widget.GetClass(), EventName, DelegateName))
		{
			AddError(
				Context,
				TEXT("ACTION_EVENT_NOT_SUPPORTED_BY_WIDGET"),
				MakeChildPath(ActionPath, TEXT("on")),
				FString::Printf(TEXT("Event '%s' is not supported by widget class '%s'."), *EventName, *Widget.GetClass()->GetName()),
				TEXT("Use Button/CheckBox-compatible events, or register a component that exposes the expected multicast delegate."));
			return nullptr;
		}

		FObjectProperty* ComponentProperty = Context.WidgetBlueprint.SkeletonGeneratedClass
			? FindFProperty<FObjectProperty>(Context.WidgetBlueprint.SkeletonGeneratedClass, Widget.GetFName())
			: nullptr;
		if (!ComponentProperty)
		{
			AddError(
				Context,
				TEXT("WIDGET_EVENT_TARGET_IS_NOT_VARIABLE"),
				ActionPath,
				FString::Printf(TEXT("Widget '%s' is missing a generated variable property for event binding."), *Widget.GetName()),
				TEXT("Mark compiler-generated widgets as variables before creating bound events."));
			return nullptr;
		}

		if (const UK2Node_ComponentBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(&Context.WidgetBlueprint, DelegateName, ComponentProperty->GetFName()))
		{
			OutDelegateName = DelegateName;
			return const_cast<UK2Node_ComponentBoundEvent*>(ExistingNode);
		}

		FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(Widget.GetClass(), DelegateName);
		if (!DelegateProperty)
		{
			AddError(
				Context,
				TEXT("ACTION_EVENT_NOT_SUPPORTED_BY_WIDGET"),
				MakeChildPath(ActionPath, TEXT("on")),
				FString::Printf(TEXT("Delegate '%s' was not found on widget class '%s'."), *DelegateName.ToString(), *Widget.GetClass()->GetName()),
				TEXT("Use a widget class that exposes the expected multicast delegate."));
			return nullptr;
		}

		OutDelegateName = DelegateName;
		return CreateBoundEventNode(Context, *ComponentProperty, *DelegateProperty, Context.NextEventY);
	}

	static bool BuildWidgetActionsRecursive(FBuildContext& Context, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath)
	{
		if (!NodeObject.IsValid())
		{
			return true;
		}

		FString WidgetId;
		NodeObject->TryGetStringField(TEXT("id"), WidgetId);
		if (!WidgetId.IsEmpty())
		{
			if (UWidget* Widget = Context.WidgetBlueprint.WidgetTree ? Context.WidgetBlueprint.WidgetTree->FindWidget(FName(*WidgetId)) : nullptr)
			{
				const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
				if (NodeObject->TryGetArrayField(TEXT("actions"), ActionsArray) && ActionsArray)
				{
					for (int32 ActionIndex = 0; ActionIndex < ActionsArray->Num(); ++ActionIndex)
					{
						const TSharedPtr<FJsonObject>* ActionObject = nullptr;
						if (!(*ActionsArray)[ActionIndex].IsValid() || !(*ActionsArray)[ActionIndex]->TryGetObject(ActionObject) || !ActionObject || !(*ActionObject).IsValid())
						{
							continue;
						}

						FString EventName;
						(*ActionObject)->TryGetStringField(TEXT("on"), EventName);
						const FString ActionPath = MakeArrayPath(MakeChildPath(NodePath, TEXT("actions")), ActionIndex);

						FName DelegateName;
						UK2Node_ComponentBoundEvent* EventNode = FindOrCreateBoundEvent(Context, *Widget, EventName, ActionPath, DelegateName);
						if (!EventNode)
						{
							continue;
						}

						const FEventBindingKey EventKey { Widget->GetFName(), DelegateName };
						UEdGraphPin*& TailPin = Context.EventTailPins.FindOrAdd(EventKey, EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output));

						UEdGraphPin* NextTailPin = nullptr;
						if (!BuildSingleAction(Context, TailPin, *ActionObject, ActionPath, NextTailPin))
						{
							return false;
						}

						if (NextTailPin)
						{
							TailPin = NextTailPin;
						}
					}
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray)
		{
			for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
			{
				const TSharedPtr<FJsonObject>* ChildObject = nullptr;
				if ((*ChildrenArray)[ChildIndex].IsValid() && (*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid())
				{
					if (!BuildWidgetActionsRecursive(Context, *ChildObject, MakeArrayPath(MakeChildPath(NodePath, TEXT("children")), ChildIndex)))
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	static bool ResolveBuildContext(FBuildContext& Context)
	{
		Context.Graph = Context.WidgetBlueprint.GetLastEditedUberGraph();
		if (!Context.Graph)
		{
			return false;
		}

		TSharedPtr<FJsonObject> ViewModelObject;
		FString ViewModelClassName;
		if (TryGetObjectField(Context.RootObject, TEXT("viewModel"), ViewModelObject) && ViewModelObject->TryGetStringField(TEXT("class"), ViewModelClassName))
		{
			Context.ViewModelClass = UUESpecCompilerSubsystem::ResolveViewModelClass(ViewModelClassName);
		}

		Context.ViewModelGetterFunction = Context.WidgetBlueprint.ParentClass
			? Context.WidgetBlueprint.ParentClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UUESpecWidgetBase, GetUESpecViewModel))
			: nullptr;
		return true;
	}
}

bool FUESpecEventGraphBuilder::Build(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	using namespace UE::UESpec::Editor::EventGraphBuilderPrivate;

	if (!RootObject.IsValid())
	{
		return true;
	}

	FBuildContext Context { WidgetBlueprint, RootObject, OutResult };
	if (!ResolveBuildContext(Context))
	{
		return true;
	}

	FKismetEditorUtilities::CompileBlueprint(&WidgetBlueprint);

	TSharedPtr<FJsonObject> RootWidgetNode;
	if (!TryGetObjectField(RootObject, TEXT("root"), RootWidgetNode))
	{
		return true;
	}

	if (!BuildWidgetActionsRecursive(Context, RootWidgetNode, TEXT("root")))
	{
		OutResult.bOk = false;
		return false;
	}

	if (OutResult.Errors.Num() == 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
	}

	OutResult.bOk = OutResult.Errors.Num() == 0;
	return OutResult.bOk;
}
