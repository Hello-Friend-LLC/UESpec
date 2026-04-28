#include "Compiler/UESpecBindingBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Compiler/UESpecCompilerSubsystem.h"
#include "Compiler/UESpecCompilerTypes.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintFunctionReference.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMPropertyPath.h"
#include "UESpecConverterLibrary.h"
#include "UObject/FieldIterator.h"
#include "WidgetBlueprint.h"
#include "Widgets/UESpecWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogUESpecBindingBuilder, Log, All);

namespace UE::UESpec::Editor::BindingBuilderPrivate
{
	struct FBindingTarget
	{
		FName WidgetName = NAME_None;
		FString JsonPath;
		FString BindPath;
		FString ConverterName;
		FString DestinationPropertyName;
	};

	struct FBuildContext
	{
		UWidgetBlueprint& WidgetBlueprint;
		const TSharedPtr<FJsonObject>& RootObject;
		FUESpecCompilerResult& Result;
		UMVVMEditorSubsystem* MVVMEditorSubsystem = nullptr;
		UMVVMBlueprintView* BlueprintView = nullptr;
		UClass* ViewModelClass = nullptr;
		FName ViewModelName = TEXT("ViewModel");
		FGuid ViewModelId;
	};

	static EUESpecBindingMode GetBindingMode(const TSharedPtr<FJsonObject>& ViewModelObject)
	{
		FString BindingModeValue;
		if (ViewModelObject.IsValid() &&
			ViewModelObject->TryGetStringField(TEXT("bindingMode"), BindingModeValue) &&
			BindingModeValue.Equals(TEXT("manual"), ESearchCase::IgnoreCase))
		{
			return EUESpecBindingMode::Manual;
		}

		return EUESpecBindingMode::MVVM;
	}

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

	static bool TryGetBindingExpression(const TSharedPtr<FJsonValue>* Value, FString& OutBindPath, FString& OutConverterName)
	{
		OutBindPath.Reset();
		OutConverterName.Reset();

		if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Object)
		{
			return false;
		}

		const TSharedPtr<FJsonObject> ObjectValue = (*Value)->AsObject();
		if (!ObjectValue.IsValid() || !ObjectValue->TryGetStringField(TEXT("bind"), OutBindPath) || OutBindPath.IsEmpty())
		{
			return false;
		}

		ObjectValue->TryGetStringField(TEXT("convert"), OutConverterName);
		return true;
	}

	static bool IsUnsupportedBindingExpression(const FString& BindPath)
	{
		return BindPath.Contains(TEXT("[")) || BindPath.Contains(TEXT("]")) || BindPath.Contains(TEXT("(")) || BindPath.Contains(TEXT(")"));
	}

	static bool ResolveFieldPathSegments(UStruct* RootStruct, const FString& BindPath, TArray<UE::MVVM::FMVVMConstFieldVariant>& OutFields)
	{
		OutFields.Reset();
		if (!RootStruct || BindPath.IsEmpty())
		{
			return false;
		}

		TArray<FString> Segments;
		BindPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			return false;
		}

		UStruct* CurrentStruct = RootStruct;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FString& Segment = Segments[SegmentIndex];
			FProperty* const Property = FindFProperty<FProperty>(CurrentStruct, FName(*Segment));
			if (!Property)
			{
				return false;
			}

			OutFields.Add(UE::MVVM::FMVVMConstFieldVariant(Property));

			if (SegmentIndex == Segments.Num() - 1)
			{
				return true;
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrentStruct = StructProperty->Struct;
				continue;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				CurrentStruct = ObjectProperty->PropertyClass;
				continue;
			}

			return false;
		}

		return OutFields.Num() > 0;
	}

	static bool BuildSourcePath(FBuildContext& Context, const FString& BindPath, FMVVMBlueprintPropertyPath& OutSourcePath)
	{
		TArray<UE::MVVM::FMVVMConstFieldVariant> Fields;
		if (!ResolveFieldPathSegments(Context.ViewModelClass, BindPath, Fields))
		{
			return false;
		}

		OutSourcePath.ResetPropertyPath();
		OutSourcePath.SetViewModelId(Context.ViewModelId);
		OutSourcePath.SetPropertyPath(&Context.WidgetBlueprint, Fields[0]);
		for (int32 FieldIndex = 1; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			OutSourcePath.AppendPropertyPath(&Context.WidgetBlueprint, Fields[FieldIndex]);
		}

		return true;
	}

	static FProperty* ResolveDestinationProperty(UWidget& Widget, const FString& DestinationPropertyName)
	{
		return FindFProperty<FProperty>(Widget.GetClass(), FName(*DestinationPropertyName));
	}

	static bool BuildDestinationPath(FBuildContext& Context, UWidget& Widget, const FString& DestinationPropertyName, FMVVMBlueprintPropertyPath& OutDestinationPath)
	{
		FProperty* const DestinationProperty = ResolveDestinationProperty(Widget, DestinationPropertyName);
		if (!DestinationProperty)
		{
			return false;
		}

		OutDestinationPath.ResetPropertyPath();
		if (Widget.GetFName() == Context.WidgetBlueprint.GetFName())
		{
			OutDestinationPath.SetSelfContext();
		}
		else
		{
			OutDestinationPath.SetWidgetName(Widget.GetFName());
		}
		OutDestinationPath.SetPropertyPath(&Context.WidgetBlueprint, UE::MVVM::FMVVMConstFieldVariant(DestinationProperty));
		return true;
	}

	static UFunction* ResolveConverterFunction(const FString& ConverterName)
	{
		if (ConverterName.IsEmpty())
		{
			return nullptr;
		}

		return UUESpecConverterLibrary::StaticClass()->FindFunctionByName(FName(*FString::Printf(TEXT("Conv_%s"), *ConverterName)));
	}

	static bool BindConversionSourcePath(FBuildContext& Context, FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPropertyPath& SourcePath)
	{
		UMVVMBlueprintViewConversionFunction* const ConversionFunction = Binding.Conversion.GetConversionFunction(true);
		if (!ConversionFunction)
		{
			return false;
		}

		const TArrayView<const FMVVMBlueprintPin> Pins = ConversionFunction->GetPins();
		for (const FMVVMBlueprintPin& Pin : Pins)
		{
			if (!Pin.GetId().IsValid())
			{
				continue;
			}

			const TArrayView<const FName> PinNames = Pin.GetId().GetNames();
			if (PinNames.Num() == 0 || PinNames.Last() == TEXT("ReturnValue"))
			{
				continue;
			}

			Context.MVVMEditorSubsystem->SetPathForConversionFunctionArgument(&Context.WidgetBlueprint, Binding, Pin.GetId(), SourcePath, true);
			return true;
		}

		return false;
	}

	static bool CreateBindingForTarget(FBuildContext& Context, const FBindingTarget& Target)
	{
		UWidget* const Widget = Context.WidgetBlueprint.WidgetTree ? Context.WidgetBlueprint.WidgetTree->FindWidget(Target.WidgetName) : nullptr;
		if (!Widget)
		{
			return true;
		}

		FMVVMBlueprintPropertyPath SourcePath;
		if (!BuildSourcePath(Context, Target.BindPath, SourcePath))
		{
			UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping unsupported or unresolved bind path '%s' at %s."), *Target.BindPath, *Target.JsonPath);
			return true;
		}

		FMVVMBlueprintPropertyPath DestinationPath;
		if (!BuildDestinationPath(Context, *Widget, Target.DestinationPropertyName, DestinationPath))
		{
			UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping unsupported destination property '%s' for widget '%s' at %s."), *Target.DestinationPropertyName, *Target.WidgetName.ToString(), *Target.JsonPath);
			return true;
		}

		FMVVMBlueprintViewBinding& Binding = Context.MVVMEditorSubsystem->AddBinding(&Context.WidgetBlueprint);
		Context.MVVMEditorSubsystem->SetDestinationPathForBinding(&Context.WidgetBlueprint, Binding, DestinationPath, false);
		Context.MVVMEditorSubsystem->SetBindingTypeForBinding(&Context.WidgetBlueprint, Binding, EMVVMBindingMode::OneWayToDestination);

		if (Target.ConverterName.IsEmpty())
		{
			Context.MVVMEditorSubsystem->SetSourcePathForBinding(&Context.WidgetBlueprint, Binding, SourcePath);
			return true;
		}

		UFunction* const ConverterFunction = ResolveConverterFunction(Target.ConverterName);
		if (!ConverterFunction)
		{
			AddError(Context, TEXT("CONVERTER_FUNCTION_NOT_FOUND"), Target.JsonPath, FString::Printf(TEXT("Converter '%s' is declared but no runtime conversion function was found."), *Target.ConverterName), TEXT("Implement the converter in UUESpecConverterLibrary or remove the convert field."));
			return false;
		}

		if (!Context.MVVMEditorSubsystem->IsValidConversionFunction(&Context.WidgetBlueprint, ConverterFunction, SourcePath, DestinationPath))
		{
			UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping binding at %s because converter '%s' is not valid for the source/destination types."), *Target.JsonPath, *Target.ConverterName);
			return true;
		}

		Context.MVVMEditorSubsystem->SetSourceToDestinationConversionFunction(
			&Context.WidgetBlueprint,
			Binding,
			FMVVMBlueprintFunctionReference(&Context.WidgetBlueprint, ConverterFunction));

		if (!BindConversionSourcePath(Context, Binding, SourcePath))
		{
			AddError(Context, TEXT("BINDING_CONVERSION_ARGUMENT_CONNECT_FAILED"), Target.JsonPath, FString::Printf(TEXT("Failed to wire converter '%s' to the generated source path."), *Target.ConverterName), TEXT("Check the converter function signature and MVVM wrapper pin generation."));
			return false;
		}

		return true;
	}

	static void CollectBindingTargetsFromNode(const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, TArray<FBindingTarget>& OutTargets)
	{
		if (!NodeObject.IsValid())
		{
			return;
		}

		FString WidgetId;
		NodeObject->TryGetStringField(TEXT("id"), WidgetId);

		if (!WidgetId.IsEmpty())
		{
			FString BindPath;
			FString ConverterName;

			if (const TSharedPtr<FJsonValue>* ContentValue = NodeObject->Values.Find(TEXT("content")); TryGetBindingExpression(ContentValue, BindPath, ConverterName))
			{
				FBindingTarget& BindingTarget = OutTargets.AddDefaulted_GetRef();
				BindingTarget.WidgetName = FName(*WidgetId);
				BindingTarget.JsonPath = MakeChildPath(NodePath, TEXT("content"));
				BindingTarget.BindPath = BindPath;
				BindingTarget.ConverterName = ConverterName;
				BindingTarget.DestinationPropertyName = TEXT("Text");

				FString WidgetType;
				NodeObject->TryGetStringField(TEXT("type"), WidgetType);
				if (WidgetType == TEXT("ProgressBar"))
				{
					BindingTarget.DestinationPropertyName = TEXT("Percent");
				}
				else if (WidgetType != TEXT("TextBlock"))
				{
					BindingTarget.DestinationPropertyName.Reset();
				}
			}

			if (const TSharedPtr<FJsonValue>* VisibilityValue = NodeObject->Values.Find(TEXT("visibility")); TryGetBindingExpression(VisibilityValue, BindPath, ConverterName))
			{
				FBindingTarget& BindingTarget = OutTargets.AddDefaulted_GetRef();
				BindingTarget.WidgetName = FName(*WidgetId);
				BindingTarget.JsonPath = MakeChildPath(NodePath, TEXT("visibility"));
				BindingTarget.BindPath = BindPath;
				BindingTarget.ConverterName = ConverterName;
				BindingTarget.DestinationPropertyName = TEXT("Visibility");
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
		{
			return;
		}

		for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
		{
			const TSharedPtr<FJsonObject>* ChildObject = nullptr;
			if ((*ChildrenArray)[ChildIndex].IsValid() && (*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid())
			{
				CollectBindingTargetsFromNode(*ChildObject, MakeArrayPath(MakeChildPath(NodePath, TEXT("children")), ChildIndex), OutTargets);
			}
		}
	}

	static bool EnsureViewModelContext(FBuildContext& Context)
	{
		if (!Context.BlueprintView)
		{
			return false;
		}

		if (const FMVVMBlueprintViewModelContext* ExistingContext = Context.BlueprintView->FindViewModel(Context.ViewModelName))
		{
			Context.ViewModelId = ExistingContext->GetViewModelId();
			return true;
		}

		Context.WidgetBlueprint.Modify();
		Context.BlueprintView->Modify();

		FMVVMBlueprintViewModelContext ViewModelContext(Context.ViewModelClass, Context.ViewModelName);
		ViewModelContext.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
		Context.BlueprintView->AddViewModel(ViewModelContext);
		Context.ViewModelId = ViewModelContext.GetViewModelId();

		FBlueprintEditorUtils::MarkBlueprintAsModified(&Context.WidgetBlueprint);
		return Context.ViewModelId.IsValid();
	}

	template <typename TWidgetBase>
	static void ApplyBindingDefaultsToWidget(TWidgetBase& WidgetDefaults, const EUESpecBindingMode BindingMode, const TArray<FBindingTarget>& Targets)
	{
		WidgetDefaults.BindingMode = BindingMode;
		WidgetDefaults.ManualBindings.Reset();

		if (BindingMode != EUESpecBindingMode::Manual)
		{
			return;
		}

		for (const FBindingTarget& Target : Targets)
		{
			if (Target.WidgetName.IsNone() || Target.DestinationPropertyName.IsEmpty() || IsUnsupportedBindingExpression(Target.BindPath))
			{
				continue;
			}

			FUESpecManualBinding& Binding = WidgetDefaults.ManualBindings.AddDefaulted_GetRef();
			Binding.WidgetId = Target.WidgetName;
			Binding.SourcePath = Target.BindPath;
			Binding.ConverterName = Target.ConverterName;

			if (Target.DestinationPropertyName == TEXT("Percent"))
			{
				Binding.Destination = EUESpecManualBindingDestination::Percent;
			}
			else if (Target.DestinationPropertyName == TEXT("Visibility"))
			{
				Binding.Destination = EUESpecManualBindingDestination::Visibility;
			}
			else
			{
				Binding.Destination = EUESpecManualBindingDestination::Text;
			}
		}
	}

	static void ApplyBindingDefaults(UWidgetBlueprint& WidgetBlueprint, const EUESpecBindingMode BindingMode, const TArray<FBindingTarget>& Targets)
	{
		if (!WidgetBlueprint.GeneratedClass)
		{
			return;
		}

		if (UUESpecWidgetBase* const WidgetDefaults = Cast<UUESpecWidgetBase>(WidgetBlueprint.GeneratedClass->GetDefaultObject()))
		{
			ApplyBindingDefaultsToWidget(*WidgetDefaults, BindingMode, Targets);
			return;
		}

		if (UUESpecActivatableWidgetBase* const ActivatableDefaults = Cast<UUESpecActivatableWidgetBase>(WidgetBlueprint.GeneratedClass->GetDefaultObject()))
		{
			ApplyBindingDefaultsToWidget(*ActivatableDefaults, BindingMode, Targets);
		}
	}
}

bool FUESpecBindingBuilder::Build(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	using namespace UE::UESpec::Editor::BindingBuilderPrivate;

	FBuildContext Context{ WidgetBlueprint, RootObject, OutResult };

	TSharedPtr<FJsonObject> ViewModelObject;
	FString ViewModelClassName;
	if (!TryGetObjectField(RootObject, TEXT("viewModel"), ViewModelObject) || !ViewModelObject->TryGetStringField(TEXT("class"), ViewModelClassName))
	{
		return true;
	}

	Context.ViewModelClass = UUESpecCompilerSubsystem::ResolveViewModelClass(ViewModelClassName);
	if (!Context.ViewModelClass)
	{
		UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping MVVM binding generation because viewModel.class '%s' could not be resolved."), *ViewModelClassName);
		return true;
	}

	TSharedPtr<FJsonObject> RootWidgetNode;
	if (!TryGetObjectField(RootObject, TEXT("root"), RootWidgetNode))
	{
		return true;
	}

	TArray<FBindingTarget> Targets;
	CollectBindingTargetsFromNode(RootWidgetNode, TEXT("root"), Targets);

	const EUESpecBindingMode BindingMode = GetBindingMode(ViewModelObject);
	ApplyBindingDefaults(WidgetBlueprint, BindingMode, Targets);
	if (BindingMode == EUESpecBindingMode::Manual)
	{
		return OutResult.Errors.Num() == 0;
	}

	Context.MVVMEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>() : nullptr;
	if (!Context.MVVMEditorSubsystem)
	{
		AddError(Context, TEXT("MVVM_EDITOR_SUBSYSTEM_UNAVAILABLE"), TEXT("viewModel"), TEXT("The MVVM editor subsystem is not available in this editor session."), TEXT("Enable the ModelViewViewModel editor modules before compiling bindings."));
		return false;
	}

	Context.BlueprintView = Context.MVVMEditorSubsystem->RequestView(&WidgetBlueprint);
	if (!Context.BlueprintView)
	{
		AddError(Context, TEXT("MVVM_BLUEPRINT_VIEW_UNAVAILABLE"), TEXT("viewModel"), TEXT("Failed to get or create the WidgetBlueprint MVVM view extension."), TEXT("Check the ModelViewViewModel plugin installation."));
		return false;
	}

	if (!EnsureViewModelContext(Context))
	{
		AddError(Context, TEXT("MVVM_VIEWMODEL_CONTEXT_CREATE_FAILED"), TEXT("viewModel"), TEXT("Failed to create the MVVM ViewModel context for this WidgetBlueprint."), TEXT("Check the viewModel.class declaration and MVVM editor subsystem state."));
		return false;
	}

	for (const FBindingTarget& Target : Targets)
	{
		if (Target.DestinationPropertyName.IsEmpty())
		{
			UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping unsupported binding destination at %s."), *Target.JsonPath);
			continue;
		}

		if (IsUnsupportedBindingExpression(Target.BindPath))
		{
			UE_LOG(LogUESpecBindingBuilder, Warning, TEXT("Skipping unsupported complex bind expression '%s' at %s."), *Target.BindPath, *Target.JsonPath);
			continue;
		}

		if (!CreateBindingForTarget(Context, Target))
		{
			return false;
		}
	}

	return OutResult.Errors.Num() == 0;
}
