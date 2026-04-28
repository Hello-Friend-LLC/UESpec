#include "Widgets/UESpecWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "INotifyFieldValueChanged.h"
#include "Logging/LogMacros.h"
#include "UESpecConverterLibrary.h"
#include "UESpecSignalBus.h"
#include "UObject/UnrealType.h"
#include "View/MVVMView.h"
#include "ViewModel/UESpecViewModelBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecWidgetBase)

DEFINE_LOG_CATEGORY_STATIC(LogUESpecWidgetBase, Log, All);

namespace UE::UESpec::Private
{
	struct FVmMethodReference
	{
		FString MethodName;
		TArray<FString> Arguments;
	};

	enum class EResolvedBindingValueType : uint8
	{
		None,
		Bool,
		Int,
		Float,
		String,
		Name,
		Text,
		Color,
		Visibility
	};

	struct FResolvedBindingValue
	{
		EResolvedBindingValueType Type = EResolvedBindingValueType::None;
		bool BoolValue = false;
		int32 IntValue = 0;
		float FloatValue = 0.0f;
		FString StringValue;
		FName NameValue = NAME_None;
		FText TextValue;
		FLinearColor ColorValue = FLinearColor::White;
		ESlateVisibility VisibilityValue = ESlateVisibility::Visible;
	};

	static const UUESpecWidgetBase* GetWidgetBaseDefaults(const UUserWidget& Widget)
	{
		return Cast<UUESpecWidgetBase>(Widget.GetClass()->GetDefaultObject());
	}

	static const UUESpecActivatableWidgetBase* GetActivatableWidgetBaseDefaults(const UUserWidget& Widget)
	{
		return Cast<UUESpecActivatableWidgetBase>(Widget.GetClass()->GetDefaultObject());
	}

	static EUESpecBindingMode ResolveBindingMode(const UUserWidget& Widget, const EUESpecBindingMode InstanceMode, const TArray<FUESpecManualBinding>& InstanceBindings)
	{
		if (InstanceMode == EUESpecBindingMode::Manual || InstanceBindings.Num() > 0)
		{
			return InstanceMode;
		}

		if (const UUESpecWidgetBase* const WidgetDefaults = GetWidgetBaseDefaults(Widget))
		{
			return WidgetDefaults->BindingMode;
		}

		if (const UUESpecActivatableWidgetBase* const ActivatableDefaults = GetActivatableWidgetBaseDefaults(Widget))
		{
			return ActivatableDefaults->BindingMode;
		}

		return InstanceMode;
	}

	static const TArray<FUESpecManualBinding>& ResolveManualBindings(const UUserWidget& Widget, const TArray<FUESpecManualBinding>& InstanceBindings)
	{
		if (InstanceBindings.Num() > 0)
		{
			return InstanceBindings;
		}

		if (const UUESpecWidgetBase* const WidgetDefaults = GetWidgetBaseDefaults(Widget))
		{
			return WidgetDefaults->ManualBindings;
		}

		if (const UUESpecActivatableWidgetBase* const ActivatableDefaults = GetActivatableWidgetBaseDefaults(Widget))
		{
			return ActivatableDefaults->ManualBindings;
		}

		return InstanceBindings;
	}

	void InitializeViewModelInstance(UUserWidget& Widget, TSoftClassPtr<UUESpecViewModelBase> ViewModelClass, FName ViewModelContext, TObjectPtr<UUESpecViewModelBase>& ViewModelInstance)
	{
		if (ViewModelInstance)
		{
			return;
		}

		if (ViewModelClass.IsNull())
		{
			if (const UUESpecWidgetBase* const WidgetDefaults = GetWidgetBaseDefaults(Widget))
			{
				ViewModelClass = WidgetDefaults->ViewModelClass;
				ViewModelContext = WidgetDefaults->ViewModelContext;
			}
			else if (const UUESpecActivatableWidgetBase* const ActivatableDefaults = GetActivatableWidgetBaseDefaults(Widget))
			{
				ViewModelClass = ActivatableDefaults->ViewModelClass;
				ViewModelContext = ActivatableDefaults->ViewModelContext;
			}
		}

		if (ViewModelClass.IsNull())
		{
			return;
		}

		UClass* ResolvedClass = ViewModelClass.Get();
		if (!ResolvedClass)
		{
			ResolvedClass = ViewModelClass.LoadSynchronous();
		}
		if (!ResolvedClass || !ResolvedClass->IsChildOf(UUESpecViewModelBase::StaticClass()) || ResolvedClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogUESpecWidgetBase, Warning, TEXT("Unable to create a UESpec viewmodel instance for widget '%s' from class '%s'."), *Widget.GetName(), *ViewModelClass.ToSoftObjectPath().ToString());
			return;
		}

		ViewModelInstance = NewObject<UUESpecViewModelBase>(&Widget, ResolvedClass);
		if (!ViewModelInstance)
		{
			return;
		}

		if (UMVVMView* const View = Widget.GetExtension<UMVVMView>())
		{
			TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
			ViewModelInterface.SetObject(ViewModelInstance.Get());
			ViewModelInterface.SetInterface(Cast<INotifyFieldValueChanged>(ViewModelInstance.Get()));

			if (ViewModelInterface.GetInterface())
			{
				bool bAssigned = false;
				if (!ViewModelContext.IsNone())
				{
					bAssigned = View->SetViewModel(ViewModelContext, ViewModelInterface);
				}

				if (!bAssigned)
				{
					View->SetViewModelByClass(ViewModelInterface);
				}
			}
		}
	}

	static FString NormalizeBindingPath(const FString& SourcePath)
	{
		FString NormalizedPath = SourcePath.TrimStartAndEnd();
		if (NormalizedPath.StartsWith(TEXT("vm.")))
		{
			NormalizedPath.RightChopInline(3, EAllowShrinking::No);
		}

		return NormalizedPath;
	}

	static bool ResolveLeafPropertyValue(const UObject& RootObject, const FString& SourcePath, FResolvedBindingValue& OutValue)
	{
		OutValue = FResolvedBindingValue();

		const FString NormalizedPath = NormalizeBindingPath(SourcePath);
		if (NormalizedPath.IsEmpty() ||
			NormalizedPath.Contains(TEXT("[")) ||
			NormalizedPath.Contains(TEXT("]")) ||
			NormalizedPath.Contains(TEXT("(")) ||
			NormalizedPath.Contains(TEXT(")")))
		{
			return false;
		}

		TArray<FString> Segments;
		NormalizedPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			return false;
		}

		const void* CurrentContainer = &RootObject;
		const UStruct* CurrentStruct = RootObject.GetClass();

		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FName SegmentName(*Segments[SegmentIndex]);
			FProperty* const Property = FindFProperty<FProperty>(CurrentStruct, SegmentName);
			if (!Property)
			{
				return false;
			}

			const void* const PropertyAddress = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
			const bool bIsLeaf = SegmentIndex == Segments.Num() - 1;
			if (bIsLeaf)
			{
				if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Bool;
					OutValue.BoolValue = BoolProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Int;
					OutValue.IntValue = IntProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Float;
					OutValue.FloatValue = FloatProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Float;
					OutValue.FloatValue = static_cast<float>(DoubleProperty->GetPropertyValue(PropertyAddress));
					return true;
				}

				if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
				{
					if (NumericProperty->IsFloatingPoint())
					{
						OutValue.Type = EResolvedBindingValueType::Float;
						OutValue.FloatValue = static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(PropertyAddress));
						return true;
					}

					if (NumericProperty->IsInteger())
					{
						OutValue.Type = EResolvedBindingValueType::Int;
						OutValue.IntValue = static_cast<int32>(NumericProperty->GetSignedIntPropertyValue(PropertyAddress));
						return true;
					}
				}

				if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::String;
					OutValue.StringValue = StringProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Name;
					OutValue.NameValue = NameProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
				{
					OutValue.Type = EResolvedBindingValueType::Text;
					OutValue.TextValue = TextProperty->GetPropertyValue(PropertyAddress);
					return true;
				}

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
					{
						OutValue.Type = EResolvedBindingValueType::Color;
						OutValue.ColorValue = *StructProperty->ContainerPtrToValuePtr<FLinearColor>(CurrentContainer);
						return true;
					}
				}

				if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
				{
					if (ByteProperty->Enum && ByteProperty->Enum->GetName().EndsWith(TEXT("ESlateVisibility")))
					{
						OutValue.Type = EResolvedBindingValueType::Visibility;
						OutValue.VisibilityValue = static_cast<ESlateVisibility>(ByteProperty->GetPropertyValue(PropertyAddress));
						return true;
					}
				}

				return false;
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrentContainer = PropertyAddress;
				CurrentStruct = StructProperty->Struct;
				continue;
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				UObject* const NestedObject = ObjectProperty->GetObjectPropertyValue(PropertyAddress);
				if (!NestedObject)
				{
					return false;
				}

				CurrentContainer = NestedObject;
				CurrentStruct = NestedObject->GetClass();
				continue;
			}

			return false;
		}

		return false;
	}

	static bool ConvertBindingValue(const FResolvedBindingValue& InValue, const FString& ConverterName, FResolvedBindingValue& OutValue)
	{
		if (ConverterName.IsEmpty())
		{
			OutValue = InValue;
			return InValue.Type != EResolvedBindingValueType::None;
		}

		OutValue = FResolvedBindingValue();
		if (ConverterName.Equals(TEXT("Percent"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::Float)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Text;
			OutValue.TextValue = UUESpecConverterLibrary::Conv_Percent(InValue.FloatValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("LocText"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::String)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Text;
			OutValue.TextValue = UUESpecConverterLibrary::Conv_LocText(InValue.StringValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("RarityColor"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::Int)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Color;
			OutValue.ColorValue = UUESpecConverterLibrary::Conv_RarityColor(InValue.IntValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("TimeFormat"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::Float)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Text;
			OutValue.TextValue = UUESpecConverterLibrary::Conv_TimeFormat(InValue.FloatValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("BoolToVisibility"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::Bool)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Visibility;
			OutValue.VisibilityValue = UUESpecConverterLibrary::Conv_BoolToVisibility(InValue.BoolValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::Bool)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::Bool;
			OutValue.BoolValue = UUESpecConverterLibrary::Conv_Negate(InValue.BoolValue);
			return true;
		}

		if (ConverterName.Equals(TEXT("Stringify"), ESearchCase::IgnoreCase))
		{
			if (InValue.Type != EResolvedBindingValueType::String)
			{
				return false;
			}

			OutValue.Type = EResolvedBindingValueType::String;
			OutValue.StringValue = UUESpecConverterLibrary::Conv_Stringify(InValue.StringValue);
			return true;
		}

		return false;
	}

	static bool ApplyManualBindingValue(UWidget& Widget, const FUESpecManualBinding& Binding, const FResolvedBindingValue& Value)
	{
		switch (Binding.Destination)
		{
		case EUESpecManualBindingDestination::Text:
			if (UTextBlock* const TextBlock = Cast<UTextBlock>(&Widget))
			{
				FText TextValue;
				switch (Value.Type)
				{
				case EResolvedBindingValueType::Text:
					TextValue = Value.TextValue;
					break;
				case EResolvedBindingValueType::String:
					TextValue = FText::FromString(Value.StringValue);
					break;
				case EResolvedBindingValueType::Name:
					TextValue = FText::FromName(Value.NameValue);
					break;
				case EResolvedBindingValueType::Float:
					TextValue = FText::AsNumber(Value.FloatValue);
					break;
				case EResolvedBindingValueType::Int:
					TextValue = FText::AsNumber(Value.IntValue);
					break;
				case EResolvedBindingValueType::Bool:
					TextValue = Value.BoolValue ? FText::FromString(TEXT("true")) : FText::FromString(TEXT("false"));
					break;
				default:
					return false;
				}

				TextBlock->SetText(TextValue);
				return true;
			}
			return false;

		case EUESpecManualBindingDestination::Percent:
			if (UProgressBar* const ProgressBar = Cast<UProgressBar>(&Widget))
			{
				float PercentValue = 0.0f;
				if (Value.Type == EResolvedBindingValueType::Float)
				{
					PercentValue = Value.FloatValue;
				}
				else if (Value.Type == EResolvedBindingValueType::Int)
				{
					PercentValue = static_cast<float>(Value.IntValue);
				}
				else
				{
					return false;
				}

				ProgressBar->SetPercent(PercentValue);
				return true;
			}
			return false;

		case EUESpecManualBindingDestination::Visibility:
			if (Value.Type == EResolvedBindingValueType::Visibility)
			{
				Widget.SetVisibility(Value.VisibilityValue);
				return true;
			}
			if (Value.Type == EResolvedBindingValueType::Bool)
			{
				Widget.SetVisibility(Value.BoolValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
				return true;
			}
			return false;
		}

		return false;
	}

	static void RefreshManualBindings(UUserWidget& Widget, UUESpecViewModelBase* ViewModel, const TArray<FUESpecManualBinding>& ManualBindings)
	{
		if (!ViewModel || !Widget.WidgetTree)
		{
			return;
		}

		for (const FUESpecManualBinding& Binding : ManualBindings)
		{
			if (Binding.WidgetId.IsNone())
			{
				continue;
			}

			UWidget* const TargetWidget = Widget.WidgetTree->FindWidget(Binding.WidgetId);
			if (!TargetWidget)
			{
				continue;
			}

			FResolvedBindingValue SourceValue;
			if (!ResolveLeafPropertyValue(*ViewModel, Binding.SourcePath, SourceValue))
			{
				UE_LOG(LogUESpecWidgetBase, Verbose, TEXT("Manual binding could not resolve source path '%s' for widget '%s'."), *Binding.SourcePath, *Binding.WidgetId.ToString());
				continue;
			}

			FResolvedBindingValue OutputValue;
			if (!ConvertBindingValue(SourceValue, Binding.ConverterName, OutputValue))
			{
				UE_LOG(LogUESpecWidgetBase, Warning, TEXT("Manual binding converter '%s' is not compatible with source path '%s'."), *Binding.ConverterName, *Binding.SourcePath);
				continue;
			}

			if (!ApplyManualBindingValue(*TargetWidget, Binding, OutputValue))
			{
				UE_LOG(LogUESpecWidgetBase, Warning, TEXT("Manual binding could not apply destination '%d' on widget '%s'."), static_cast<int32>(Binding.Destination), *Binding.WidgetId.ToString());
			}
		}
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

	static FString UnquoteString(const FString& Value)
	{
		if (Value.Len() >= 2)
		{
			const TCHAR First = Value[0];
			const TCHAR Last = Value[Value.Len() - 1];
			if ((First == TEXT('"') && Last == TEXT('"')) || (First == TEXT('\'') && Last == TEXT('\'')))
			{
				return Value.Mid(1, Value.Len() - 2);
			}
		}

		return Value;
	}

	static bool AssignArgumentToProperty(const FString& Argument, FProperty& Property, uint8* ParameterBuffer)
	{
		const FString TrimmedArgument = Argument.TrimStartAndEnd();

		if (FIntProperty* IntProperty = CastField<FIntProperty>(&Property))
		{
			int32 ParsedValue = 0;
			if (!LexTryParseString(ParsedValue, *TrimmedArgument))
			{
				return false;
			}

			IntProperty->SetPropertyValue_InContainer(ParameterBuffer, ParsedValue);
			return true;
		}

		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(&Property))
		{
			float ParsedValue = 0.0f;
			if (!LexTryParseString(ParsedValue, *TrimmedArgument))
			{
				return false;
			}

			FloatProperty->SetPropertyValue_InContainer(ParameterBuffer, ParsedValue);
			return true;
		}

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
		{
			const bool bValue = TrimmedArgument.Equals(TEXT("true"), ESearchCase::IgnoreCase) || TrimmedArgument == TEXT("1");
			if (!bValue && !TrimmedArgument.Equals(TEXT("false"), ESearchCase::IgnoreCase) && TrimmedArgument != TEXT("0"))
			{
				return false;
			}

			BoolProperty->SetPropertyValue_InContainer(ParameterBuffer, bValue);
			return true;
		}

		if (FStrProperty* StringProperty = CastField<FStrProperty>(&Property))
		{
			StringProperty->SetPropertyValue_InContainer(ParameterBuffer, UnquoteString(TrimmedArgument));
			return true;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(&Property))
		{
			NameProperty->SetPropertyValue_InContainer(ParameterBuffer, FName(*UnquoteString(TrimmedArgument)));
			return true;
		}

		if (FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
		{
			TextProperty->SetPropertyValue_InContainer(ParameterBuffer, FText::FromString(UnquoteString(TrimmedArgument)));
			return true;
		}

		return false;
	}

	static bool InvokeViewModelMethod(UUESpecViewModelBase* ViewModel, const FString& Expression, bool* OutBoolResult)
	{
		if (!ViewModel)
		{
			return false;
		}

		FVmMethodReference MethodReference;
		if (!ParseVmMethodReference(Expression, MethodReference))
		{
			return false;
		}

		UFunction* const TargetFunction = ViewModel->FindFunction(FName(*MethodReference.MethodName));
		if (!TargetFunction)
		{
			return false;
		}

		FProperty* ReturnProperty = nullptr;
		TArray<FProperty*> InputParameters;
		for (TFieldIterator<FProperty> PropertyIt(TargetFunction); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnProperty = Property;
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm))
			{
				return false;
			}

			InputParameters.Add(Property);
		}

		if (InputParameters.Num() != MethodReference.Arguments.Num())
		{
			return false;
		}

		if (OutBoolResult && !CastField<FBoolProperty>(ReturnProperty))
		{
			return false;
		}

		uint8* const Parameters = static_cast<uint8*>(FMemory_Alloca(TargetFunction->ParmsSize));
		FMemory::Memzero(Parameters, TargetFunction->ParmsSize);
		TargetFunction->InitializeStruct(Parameters);

		bool bAssignedAllArguments = true;
		for (int32 ArgumentIndex = 0; ArgumentIndex < InputParameters.Num(); ++ArgumentIndex)
		{
			if (!AssignArgumentToProperty(MethodReference.Arguments[ArgumentIndex], *InputParameters[ArgumentIndex], Parameters))
			{
				bAssignedAllArguments = false;
				break;
			}
		}

		if (!bAssignedAllArguments)
		{
			TargetFunction->DestroyStruct(Parameters);
			return false;
		}

		ViewModel->ProcessEvent(TargetFunction, Parameters);

		if (OutBoolResult)
		{
			*OutBoolResult = CastFieldChecked<FBoolProperty>(ReturnProperty)->GetPropertyValue_InContainer(Parameters);
		}

		TargetFunction->DestroyStruct(Parameters);
		return true;
	}

	static bool TryParseVisibility(const FString& Value, ESlateVisibility& OutVisibility)
	{
		if (Value == TEXT("Visible"))
		{
			OutVisibility = ESlateVisibility::Visible;
			return true;
		}
		if (Value == TEXT("Hidden"))
		{
			OutVisibility = ESlateVisibility::Hidden;
			return true;
		}
		if (Value == TEXT("Collapsed"))
		{
			OutVisibility = ESlateVisibility::Collapsed;
			return true;
		}
		if (Value == TEXT("HitTestInvisible"))
		{
			OutVisibility = ESlateVisibility::HitTestInvisible;
			return true;
		}
		if (Value == TEXT("SelfHitTestInvisible"))
		{
			OutVisibility = ESlateVisibility::SelfHitTestInvisible;
			return true;
		}

		return false;
	}

	static bool TryParseColor(const FString& Value, FLinearColor& OutColor)
	{
		const FString TrimmedValue = Value.TrimStartAndEnd();
		if (TrimmedValue.StartsWith(TEXT("#")))
		{
			return OutColor.InitFromString(TrimmedValue);
		}

		if (TrimmedValue.Contains(TEXT("=")))
		{
			return OutColor.InitFromString(TrimmedValue);
		}

		if (TrimmedValue.Equals(TEXT("White"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::White;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Black"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Black;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Red"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Red;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Green"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Green;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Blue"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Blue;
			return true;
		}
		if (TrimmedValue.Equals(TEXT("Transparent"), ESearchCase::IgnoreCase))
		{
			OutColor = FLinearColor::Transparent;
			return true;
		}

		return false;
	}

	static bool IsOpacityProperty(const FName PropertyName)
	{
		return PropertyName == TEXT("opacity") || PropertyName == TEXT("renderOpacity");
	}

	static bool IsVisibilityProperty(const FName PropertyName)
	{
		return PropertyName == TEXT("visibility");
	}

	static bool IsColorProperty(const FName PropertyName)
	{
		return PropertyName == TEXT("color") || PropertyName == TEXT("colorAndOpacity") || PropertyName == TEXT("fontColor") || PropertyName == TEXT("fillColor");
	}

	static void CaptureWidgetDefaults(UWidget& Widget, const FName PropertyName, FUESpecCapturedWidgetState& CapturedState)
	{
		if (IsOpacityProperty(PropertyName) && !CapturedState.bHasRenderOpacity)
		{
			CapturedState.bHasRenderOpacity = true;
			CapturedState.RenderOpacity = Widget.GetRenderOpacity();
			return;
		}

		if (IsVisibilityProperty(PropertyName) && !CapturedState.bHasVisibility)
		{
			CapturedState.bHasVisibility = true;
			CapturedState.Visibility = Widget.GetVisibility();
			return;
		}

		if (IsColorProperty(PropertyName) && !CapturedState.bHasColorAndOpacity)
		{
			if (const UTextBlock* TextBlock = Cast<UTextBlock>(&Widget))
			{
				CapturedState.bHasColorAndOpacity = true;
				CapturedState.ColorAndOpacity = TextBlock->GetColorAndOpacity().GetSpecifiedColor();
				return;
			}

			if (const UImage* Image = Cast<UImage>(&Widget))
			{
				CapturedState.bHasColorAndOpacity = true;
				CapturedState.ColorAndOpacity = Image->GetColorAndOpacity();
				return;
			}

			if (const UProgressBar* ProgressBar = Cast<UProgressBar>(&Widget))
			{
				CapturedState.bHasColorAndOpacity = true;
				CapturedState.ColorAndOpacity = ProgressBar->GetFillColorAndOpacity();
			}
		}
	}

	static void RestoreCapturedProperty(UWidget& Widget, const FName PropertyName, const FUESpecCapturedWidgetState& CapturedState)
	{
		if (IsOpacityProperty(PropertyName) && CapturedState.bHasRenderOpacity)
		{
			Widget.SetRenderOpacity(CapturedState.RenderOpacity);
			return;
		}

		if (IsVisibilityProperty(PropertyName) && CapturedState.bHasVisibility)
		{
			Widget.SetVisibility(CapturedState.Visibility);
			return;
		}

		if (IsColorProperty(PropertyName) && CapturedState.bHasColorAndOpacity)
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(&Widget))
			{
				TextBlock->SetColorAndOpacity(FSlateColor(CapturedState.ColorAndOpacity));
				return;
			}

			if (UImage* Image = Cast<UImage>(&Widget))
			{
				Image->SetColorAndOpacity(CapturedState.ColorAndOpacity);
				return;
			}

			if (UProgressBar* ProgressBar = Cast<UProgressBar>(&Widget))
			{
				ProgressBar->SetFillColorAndOpacity(CapturedState.ColorAndOpacity);
			}
		}
	}

	static bool ApplyStateBindingProperty(UWidget& Widget, const FUESpecStateBindingProperty& PropertyBinding, FUESpecCapturedWidgetState& CapturedState)
	{
		CaptureWidgetDefaults(Widget, PropertyBinding.Property, CapturedState);

		if (IsOpacityProperty(PropertyBinding.Property))
		{
			float Opacity = 1.0f;
			if (!LexTryParseString(Opacity, *PropertyBinding.Value))
			{
				return false;
			}

			Widget.SetRenderOpacity(Opacity);
			return true;
		}

		if (IsVisibilityProperty(PropertyBinding.Property))
		{
			ESlateVisibility Visibility = ESlateVisibility::Visible;
			if (!TryParseVisibility(PropertyBinding.Value, Visibility))
			{
				return false;
			}

			Widget.SetVisibility(Visibility);
			return true;
		}

		if (IsColorProperty(PropertyBinding.Property))
		{
			FLinearColor Color = FLinearColor::White;
			if (!TryParseColor(PropertyBinding.Value, Color))
			{
				return false;
			}

			if (UTextBlock* TextBlock = Cast<UTextBlock>(&Widget))
			{
				TextBlock->SetColorAndOpacity(FSlateColor(Color));
				return true;
			}

			if (UImage* Image = Cast<UImage>(&Widget))
			{
				Image->SetColorAndOpacity(Color);
				return true;
			}

			if (UProgressBar* ProgressBar = Cast<UProgressBar>(&Widget))
			{
				ProgressBar->SetFillColorAndOpacity(Color);
				return true;
			}
		}

		UE_LOG(LogUESpecWidgetBase, Warning, TEXT("State binding property '%s' is not implemented for widget '%s'."), *PropertyBinding.Property.ToString(), *Widget.GetName());
		return true;
	}

	static void RestoreStateBindings(UUserWidget& Widget, const FUESpecStateMachineDefinition& StateMachine, FName PreviousState, TMap<FName, FUESpecCapturedWidgetState>& CapturedStateMap)
	{
		if (PreviousState.IsNone() || !Widget.WidgetTree)
		{
			return;
		}

		TArray<const FUESpecWidgetStateBinding*> PreviousBindings;
		StateMachine.FindBindingsForState(PreviousState, PreviousBindings);
		for (const FUESpecWidgetStateBinding* Binding : PreviousBindings)
		{
			if (!Binding || Binding->WidgetId.IsNone())
			{
				continue;
			}

			UWidget* const TargetWidget = Widget.WidgetTree->FindWidget(Binding->WidgetId);
			const FUESpecCapturedWidgetState* const CapturedState = CapturedStateMap.Find(Binding->WidgetId);
			if (!TargetWidget || !CapturedState)
			{
				continue;
			}

			for (const FUESpecStateBindingProperty& PropertyBinding : Binding->Properties)
			{
				RestoreCapturedProperty(*TargetWidget, PropertyBinding.Property, *CapturedState);
			}
		}
	}

	static void ApplyStateBindings(UUserWidget& Widget, const FUESpecStateMachineDefinition& StateMachine, FName NewState, TMap<FName, FUESpecCapturedWidgetState>& CapturedStateMap)
	{
		if (NewState.IsNone() || !Widget.WidgetTree)
		{
			return;
		}

		TArray<const FUESpecWidgetStateBinding*> StateBindings;
		StateMachine.FindBindingsForState(NewState, StateBindings);
		for (const FUESpecWidgetStateBinding* Binding : StateBindings)
		{
			if (!Binding || Binding->WidgetId.IsNone())
			{
				continue;
			}

			UWidget* const TargetWidget = Widget.WidgetTree->FindWidget(Binding->WidgetId);
			if (!TargetWidget)
			{
				UE_LOG(LogUESpecWidgetBase, Warning, TEXT("State binding target '%s' was not found under widget '%s'."), *Binding->WidgetId.ToString(), *Widget.GetName());
				continue;
			}

			FUESpecCapturedWidgetState& CapturedState = CapturedStateMap.FindOrAdd(Binding->WidgetId);
			for (const FUESpecStateBindingProperty& PropertyBinding : Binding->Properties)
			{
				if (!ApplyStateBindingProperty(*TargetWidget, PropertyBinding, CapturedState))
				{
					UE_LOG(LogUESpecWidgetBase, Warning, TEXT("Failed to apply state binding '%s'='%s' on widget '%s'."), *PropertyBinding.Property.ToString(), *PropertyBinding.Value, *Binding->WidgetId.ToString());
				}
			}
		}
	}

	static void InvokeStateHook(UUESpecViewModelBase* ViewModel, const FString& HookExpression)
	{
		if (HookExpression.StartsWith(TEXT("vm.")))
		{
			InvokeViewModelMethod(ViewModel, HookExpression, nullptr);
		}
	}

	template <typename ApplyStateFunc>
	static void EvaluateSignalTransitions(const FUESpecStateMachineDefinition& StateMachine, FName CurrentState, FName SignalName, ApplyStateFunc&& ApplyState)
	{
		const FUESpecStateDefinition* const StateDefinition = StateMachine.FindStateDefinition(CurrentState);
		if (!StateDefinition || SignalName.IsNone())
		{
			return;
		}

		for (const FUESpecStateTransition& Transition : StateDefinition->Transitions)
		{
			if (Transition.When.StartsWith(TEXT("vm.")))
			{
				continue;
			}

			if (FName(*Transition.When) == SignalName)
			{
				ApplyState(Transition.To);
				return;
			}
		}
	}

	template <typename ApplyStateFunc>
	static void EvaluatePredicateTransitions(const FUESpecStateMachineDefinition& StateMachine, FName CurrentState, UUESpecViewModelBase* ViewModel, ApplyStateFunc&& ApplyState)
	{
		const FUESpecStateDefinition* const StateDefinition = StateMachine.FindStateDefinition(CurrentState);
		if (!StateDefinition || !ViewModel)
		{
			return;
		}

		for (const FUESpecStateTransition& Transition : StateDefinition->Transitions)
		{
			if (!Transition.When.StartsWith(TEXT("vm.")))
			{
				continue;
			}

			bool bCondition = false;
			if (InvokeViewModelMethod(ViewModel, Transition.When, &bCondition) && bCondition)
			{
				ApplyState(Transition.To);
				return;
			}
		}
	}
}

void UUESpecWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UE::UESpec::Private::InitializeViewModelInstance(*this, ViewModelClass, ViewModelContext, ViewModelInstance);
}

void UUESpecWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	UE::UESpec::Private::InitializeViewModelInstance(*this, ViewModelClass, ViewModelContext, ViewModelInstance);

	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		BindSignalBus(GameInstance->GetSubsystem<UUESpecSignalBus>());
	}

	if (CurrentUIState.IsNone())
	{
		const FName InitialState = UE::UESpec::StateMachine::ResolveInitialState(StateMachine);
		if (!InitialState.IsNone())
		{
			SetUIState(InitialState);
		}
	}

	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) == EUESpecBindingMode::Manual)
	{
		RefreshUESpecBindings();
	}
}

void UUESpecWidgetBase::NativeDestruct()
{
	UnbindSignalBus();
	Super::NativeDestruct();
}

void UUESpecWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UE::UESpec::Private::EvaluatePredicateTransitions(StateMachine, CurrentUIState, GetUESpecViewModel(), [this](const FName NewState)
	{
		SetUIState(NewState);
	});

	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) == EUESpecBindingMode::Manual)
	{
		RefreshUESpecBindings();
	}
}

void UUESpecWidgetBase::SetUIState(FName NewState)
{
	const UE::UESpec::StateMachine::FStateChangeResult StateChange = UE::UESpec::StateMachine::PrepareStateChange(StateMachine, CurrentUIState, NewState);
	if (!StateChange.bShouldApply)
	{
		return;
	}

	if (const FUESpecStateDefinition* const PreviousStateDefinition = StateMachine.FindStateDefinition(StateChange.PreviousState))
	{
		UE::UESpec::Private::InvokeStateHook(GetUESpecViewModel(), PreviousStateDefinition->OnExit);
	}

	UE::UESpec::Private::RestoreStateBindings(*this, StateMachine, StateChange.PreviousState, CapturedWidgetState);

	CurrentUIState = StateChange.NextState;
	UE::UESpec::Private::ApplyStateBindings(*this, StateMachine, CurrentUIState, CapturedWidgetState);

	if (const FUESpecStateDefinition* const NewStateDefinition = StateMachine.FindStateDefinition(CurrentUIState))
	{
		UE::UESpec::Private::InvokeStateHook(GetUESpecViewModel(), NewStateDefinition->OnEnter);
	}

	OnUIStateChanged.Broadcast(StateChange.PreviousState, CurrentUIState);
}

FName UUESpecWidgetBase::GetUIState() const
{
	return CurrentUIState;
}

UUESpecViewModelBase* UUESpecWidgetBase::GetUESpecViewModel() const
{
	return ViewModelInstance;
}

void UUESpecWidgetBase::RefreshUESpecBindings()
{
	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) != EUESpecBindingMode::Manual)
	{
		return;
	}

	UE::UESpec::Private::RefreshManualBindings(*this, GetUESpecViewModel(), UE::UESpec::Private::ResolveManualBindings(*this, ManualBindings));
}

void UUESpecWidgetBase::BindSignalBus(UUESpecSignalBus* SignalBus)
{
	if (BoundSignalBus.Get() == SignalBus)
	{
		return;
	}

	UnbindSignalBus();
	if (SignalBus)
	{
		SignalBus->OnSignal.AddDynamic(this, &UUESpecWidgetBase::HandleUESpecSignal);
		BoundSignalBus = SignalBus;
	}
}

void UUESpecWidgetBase::UnbindSignalBus()
{
	if (UUESpecSignalBus* const SignalBus = BoundSignalBus.Get())
	{
		SignalBus->OnSignal.RemoveDynamic(this, &UUESpecWidgetBase::HandleUESpecSignal);
	}

	BoundSignalBus.Reset();
}

void UUESpecWidgetBase::HandleUESpecSignal(FName SignalName)
{
	UE::UESpec::Private::EvaluateSignalTransitions(StateMachine, CurrentUIState, SignalName, [this](const FName NewState)
	{
		SetUIState(NewState);
	});
}

bool UUESpecActivatableWidgetBase::SupportsBackNavigation() const
{
	return WidgetKind == EUESpecWidgetKind::Menu || WidgetKind == EUESpecWidgetKind::Popup || WidgetKind == EUESpecWidgetKind::Modal;
}

bool UUESpecActivatableWidgetBase::IsActivatableKind(EUESpecWidgetKind InKind)
{
	return InKind == EUESpecWidgetKind::HUD ||
		InKind == EUESpecWidgetKind::Menu ||
		InKind == EUESpecWidgetKind::Popup ||
		InKind == EUESpecWidgetKind::Modal ||
		InKind == EUESpecWidgetKind::Debug;
}

void UUESpecActivatableWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UE::UESpec::Private::InitializeViewModelInstance(*this, ViewModelClass, ViewModelContext, ViewModelInstance);
}

void UUESpecActivatableWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	UE::UESpec::Private::InitializeViewModelInstance(*this, ViewModelClass, ViewModelContext, ViewModelInstance);

	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		BindSignalBus(GameInstance->GetSubsystem<UUESpecSignalBus>());
	}

	if (CurrentUIState.IsNone())
	{
		const FName InitialState = UE::UESpec::StateMachine::ResolveInitialState(StateMachine);
		if (!InitialState.IsNone())
		{
			SetUIState(InitialState);
		}
	}

	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) == EUESpecBindingMode::Manual)
	{
		RefreshUESpecBindings();
	}
}

void UUESpecActivatableWidgetBase::NativeDestruct()
{
	UnbindSignalBus();
	Super::NativeDestruct();
}

void UUESpecActivatableWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UE::UESpec::Private::EvaluatePredicateTransitions(StateMachine, CurrentUIState, GetUESpecViewModel(), [this](const FName NewState)
	{
		SetUIState(NewState);
	});

	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) == EUESpecBindingMode::Manual)
	{
		RefreshUESpecBindings();
	}
}

void UUESpecActivatableWidgetBase::SetUIState(FName NewState)
{
	const UE::UESpec::StateMachine::FStateChangeResult StateChange = UE::UESpec::StateMachine::PrepareStateChange(StateMachine, CurrentUIState, NewState);
	if (!StateChange.bShouldApply)
	{
		return;
	}

	if (const FUESpecStateDefinition* const PreviousStateDefinition = StateMachine.FindStateDefinition(StateChange.PreviousState))
	{
		UE::UESpec::Private::InvokeStateHook(GetUESpecViewModel(), PreviousStateDefinition->OnExit);
	}

	UE::UESpec::Private::RestoreStateBindings(*this, StateMachine, StateChange.PreviousState, CapturedWidgetState);

	CurrentUIState = StateChange.NextState;
	UE::UESpec::Private::ApplyStateBindings(*this, StateMachine, CurrentUIState, CapturedWidgetState);

	if (const FUESpecStateDefinition* const NewStateDefinition = StateMachine.FindStateDefinition(CurrentUIState))
	{
		UE::UESpec::Private::InvokeStateHook(GetUESpecViewModel(), NewStateDefinition->OnEnter);
	}

	OnUIStateChanged.Broadcast(StateChange.PreviousState, CurrentUIState);
}

FName UUESpecActivatableWidgetBase::GetUIState() const
{
	return CurrentUIState;
}

UUESpecViewModelBase* UUESpecActivatableWidgetBase::GetUESpecViewModel() const
{
	return ViewModelInstance;
}

void UUESpecActivatableWidgetBase::RefreshUESpecBindings()
{
	if (UE::UESpec::Private::ResolveBindingMode(*this, BindingMode, ManualBindings) != EUESpecBindingMode::Manual)
	{
		return;
	}

	UE::UESpec::Private::RefreshManualBindings(*this, GetUESpecViewModel(), UE::UESpec::Private::ResolveManualBindings(*this, ManualBindings));
}

void UUESpecActivatableWidgetBase::BindSignalBus(UUESpecSignalBus* SignalBus)
{
	if (BoundSignalBus.Get() == SignalBus)
	{
		return;
	}

	UnbindSignalBus();
	if (SignalBus)
	{
		SignalBus->OnSignal.AddDynamic(this, &UUESpecActivatableWidgetBase::HandleUESpecSignal);
		BoundSignalBus = SignalBus;
	}
}

void UUESpecActivatableWidgetBase::UnbindSignalBus()
{
	if (UUESpecSignalBus* const SignalBus = BoundSignalBus.Get())
	{
		SignalBus->OnSignal.RemoveDynamic(this, &UUESpecActivatableWidgetBase::HandleUESpecSignal);
	}

	BoundSignalBus.Reset();
}

void UUESpecActivatableWidgetBase::HandleUESpecSignal(FName SignalName)
{
	UE::UESpec::Private::EvaluateSignalTransitions(StateMachine, CurrentUIState, SignalName, [this](const FName NewState)
	{
		SetUIState(NewState);
	});
}
