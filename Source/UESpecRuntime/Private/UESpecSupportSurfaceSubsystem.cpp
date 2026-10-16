#include "UESpecSupportSurfaceSubsystem.h"

#include "Registry/UESpecComponentRegistry.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonLazyImage.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CheckBox.h"
#include "Components/ContentWidget.h"
#include "Components/GridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Subsystems/SubsystemCollection.h"
#include "UESpecConverterLibrary.h"
#include "UObject/FieldIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecSupportSurfaceSubsystem)

namespace UE::UESpec::Private
{
	template <typename EntryType, typename PredicateType>
	void ReplaceOrAddEntry(TArray<EntryType>& Entries, const EntryType& Entry, PredicateType Predicate)
	{
		if (EntryType* Existing = Entries.FindByPredicate(Predicate))
		{
			*Existing = Entry;
			return;
		}

		Entries.Add(Entry);
	}

	void SortUniqueStrings(TArray<FString>& Values)
	{
		for (FString& Value : Values)
		{
			Value.TrimStartAndEndInline();
		}

		TSet<FString> UniqueValues;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				UniqueValues.Add(Value);
			}
		}

		Values = UniqueValues.Array();
		Values.Sort();
	}

	TArray<FString> GetCanonicalKeysForWidgetClass(UClass* WidgetClass)
	{
		TArray<FString> Keys =
		{
			TEXT("actions"),
			TEXT("enabled"),
			TEXT("navigation"),
			TEXT("slot"),
			TEXT("stateBindings"),
			TEXT("style"),
			TEXT("visibility")
		};

		if (WidgetClass->IsChildOf(UUserWidget::StaticClass()) ||
			WidgetClass->IsChildOf(UPanelWidget::StaticClass()) ||
			WidgetClass->IsChildOf(UContentWidget::StaticClass()))
		{
			Keys.Add(TEXT("children"));
		}

		if (WidgetClass->IsChildOf(UTextBlock::StaticClass()) ||
			WidgetClass->IsChildOf(URichTextBlock::StaticClass()) ||
			WidgetClass->IsChildOf(UImage::StaticClass()) ||
			WidgetClass->IsChildOf(UProgressBar::StaticClass()) ||
			WidgetClass->IsChildOf(UCommonTextBlock::StaticClass()) ||
			WidgetClass->IsChildOf(UCommonLazyImage::StaticClass()) ||
			WidgetClass->IsChildOf(UCommonRichTextBlock::StaticClass()))
		{
			Keys.Add(TEXT("content"));
		}

		SortUniqueStrings(Keys);
		return Keys;
	}

	FUESpecSurfaceWidgetEntry MakeWidgetEntry(UClass* WidgetClass)
	{
		FUESpecSurfaceWidgetEntry Entry;
		if (!WidgetClass)
		{
			return Entry;
		}

		Entry.Type = WidgetClass->GetName();
		Entry.ClassPath = WidgetClass->GetPathName();
		Entry.Keys = GetCanonicalKeysForWidgetClass(WidgetClass);
		return Entry;
	}

	FString BuildConverterSignature(const UFunction& Function, const FString& ConverterName)
	{
		TArray<FString> Parameters;
		FString ReturnType = TEXT("void");

		for (TFieldIterator<FProperty> PropertyIt(&Function); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnType = Property->GetCPPType(nullptr, CPPF_None);
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm))
			{
				continue;
			}

			Parameters.Add(FString::Printf(TEXT("%s: %s"), *Property->GetName(), *Property->GetCPPType(nullptr, CPPF_None)));
		}

		return FString::Printf(TEXT("%s(%s) -> %s"), *ConverterName, *FString::Join(Parameters, TEXT(", ")), *ReturnType);
	}

	TArray<FUESpecSurfaceWidgetEntry> GetBuiltInUMGEntries()
	{
		const TArray<UClass*> BuiltInClasses =
		{
			UCanvasPanel::StaticClass(),
			UVerticalBox::StaticClass(),
			UHorizontalBox::StaticClass(),
			UOverlay::StaticClass(),
			UScrollBox::StaticClass(),
			UWrapBox::StaticClass(),
			UGridPanel::StaticClass(),
			UUniformGridPanel::StaticClass(),
			UBorder::StaticClass(),
			USizeBox::StaticClass(),
			UScaleBox::StaticClass(),
			USpacer::StaticClass(),
			UTextBlock::StaticClass(),
			URichTextBlock::StaticClass(),
			UImage::StaticClass(),
			UProgressBar::StaticClass(),
			UButton::StaticClass(),
			UCheckBox::StaticClass(),
			USlider::StaticClass(),
			UWidgetSwitcher::StaticClass()
		};

		TArray<FUESpecSurfaceWidgetEntry> Entries;
		Entries.Reserve(BuiltInClasses.Num());

		for (UClass* Class : BuiltInClasses)
		{
			Entries.Add(MakeWidgetEntry(Class));
		}

		Entries.Sort([](const FUESpecSurfaceWidgetEntry& A, const FUESpecSurfaceWidgetEntry& B)
		{
			return A.Type < B.Type;
		});

		return Entries;
	}

	TArray<FUESpecSurfaceWidgetEntry> GetCommonUIEntries()
	{
		const TArray<UClass*> CommonUIClasses =
		{
			UCommonActivatableWidget::StaticClass(),
			UCommonButtonBase::StaticClass(),
			UCommonTextBlock::StaticClass(),
			UCommonLazyImage::StaticClass(),
			UCommonRichTextBlock::StaticClass()
		};

		TArray<FUESpecSurfaceWidgetEntry> Entries;
		Entries.Reserve(CommonUIClasses.Num());

		for (UClass* Class : CommonUIClasses)
		{
			Entries.Add(MakeWidgetEntry(Class));
		}

		Entries.Sort([](const FUESpecSurfaceWidgetEntry& A, const FUESpecSurfaceWidgetEntry& B)
		{
			return A.Type < B.Type;
		});

		return Entries;
	}

	TArray<FUESpecSurfaceKindEntry> GetKindEntries()
	{
		return
		{
			{ TEXT("HUD"), TEXT("/Script/CommonUI.CommonActivatableWidget"), TEXT("HUDLayer") },
			{ TEXT("Menu"), TEXT("/Script/CommonUI.CommonActivatableWidget"), TEXT("MenuStack with back navigation") },
			{ TEXT("Popup"), TEXT("/Script/CommonUI.CommonActivatableWidget"), TEXT("PopupLayer") },
			{ TEXT("Modal"), TEXT("/Script/CommonUI.CommonActivatableWidget"), TEXT("ModalLayer") },
			{ TEXT("Debug"), TEXT("/Script/CommonUI.CommonActivatableWidget"), TEXT("DebugLayer") },
			{ TEXT("WorldSpace"), TEXT("/Script/UMG.UserWidget"), TEXT("World-space widget component") },
			{ TEXT("Component"), TEXT("/Script/UMG.UserWidget"), TEXT("Reusable registered component") },
			{ TEXT("Test"), TEXT("N/A"), TEXT("Automation/TestSpec executor") }
		};
	}

	static FUESpecSurfaceActionEntry MakeActionEntry(const TCHAR* Name, const TCHAR* Status)
	{
		FUESpecSurfaceActionEntry Entry;
		Entry.Name = Name;
		Entry.Status = Status;
		return Entry;
	}
}

TArray<FUESpecSurfaceActionEntry> UUESpecSupportSurfaceSubsystem::GetActionEventSurface()
{
	using UE::UESpec::Private::MakeActionEntry;
	const TCHAR* const Supported = FUESpecSurfaceActionEntry::StatusSupported;
	const TCHAR* const Unimplemented = FUESpecSurfaceActionEntry::StatusUnimplemented;

	// Supported = has a delegate mapping in UESpecEventGraphBuilder TryResolveDelegateProperty.
	return
	{
		MakeActionEntry(TEXT("Clicked"), Supported),
		MakeActionEntry(TEXT("Pressed"), Supported),
		MakeActionEntry(TEXT("Released"), Supported),
		MakeActionEntry(TEXT("Hovered"), Supported),
		MakeActionEntry(TEXT("Unhovered"), Supported),
		MakeActionEntry(TEXT("FocusReceived"), Unimplemented),
		MakeActionEntry(TEXT("FocusLost"), Unimplemented),
		MakeActionEntry(TEXT("DoubleClicked"), Unimplemented),
		MakeActionEntry(TEXT("LongPressed"), Unimplemented),
		MakeActionEntry(TEXT("DragDetected"), Unimplemented),
		MakeActionEntry(TEXT("Drop"), Unimplemented)
	};
}

TArray<FUESpecSurfaceActionEntry> UUESpecSupportSurfaceSubsystem::GetActionPrimitiveSurface()
{
	using UE::UESpec::Private::MakeActionEntry;
	const TCHAR* const Supported = FUESpecSurfaceActionEntry::StatusSupported;
	const TCHAR* const Unimplemented = FUESpecSurfaceActionEntry::StatusUnimplemented;

	// Supported = built by UESpecEventGraphBuilder BuildActionBlock.
	return
	{
		MakeActionEntry(TEXT("openLayer"), Supported),
		MakeActionEntry(TEXT("closeLayer"), Supported),
		MakeActionEntry(TEXT("setState"), Supported),
		MakeActionEntry(TEXT("callVM"), Supported),
		MakeActionEntry(TEXT("playAnim"), Unimplemented),
		MakeActionEntry(TEXT("emitSignal"), Supported)
	};
}

void UUESpecSupportSurfaceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetProjectSurface();
}

void UUESpecSupportSurfaceSubsystem::Deinitialize()
{
	RegisteredComponentSurfaces.Reset();
	RegisteredConverterSurfaces.Reset();
	RegisteredStyleTokenSurfaces.Reset();
	RegisteredSignalSurfaces.Reset();

	Super::Deinitialize();
}

void UUESpecSupportSurfaceSubsystem::ResetProjectSurface()
{
	RegisteredComponentSurfaces.Reset();
	RegisteredConverterSurfaces.Reset();
	RegisteredStyleTokenSurfaces.Reset();
	RegisteredSignalSurfaces.Reset();

	RegisterCanonicalConverters();
	LoadProjectRegistrySettings();
}

void UUESpecSupportSurfaceSubsystem::RegisterComponentSurface(const FString& Type, const FString& AssetPath, const FString& ClassPath, const TArray<FString>& Accepts, const TArray<FString>& Keys)
{
	FUESpecSurfaceWidgetEntry Entry;
	Entry.Type = Type;
	Entry.AssetPath = AssetPath;
	Entry.ClassPath = ClassPath;
	Entry.Accepts = Accepts;
	Entry.Keys = Keys;

	UE::UESpec::Private::SortUniqueStrings(Entry.Accepts);
	UE::UESpec::Private::SortUniqueStrings(Entry.Keys);

	UE::UESpec::Private::ReplaceOrAddEntry(
		RegisteredComponentSurfaces,
		Entry,
		[&Entry](const FUESpecSurfaceWidgetEntry& Existing)
		{
			return Existing.Type == Entry.Type;
		});
}

void UUESpecSupportSurfaceSubsystem::LoadProjectRegistrySettings()
{
	const UUESpecComponentRegistry& Registry = UUESpecComponentRegistry::Get();
	TArray<FUESpecComponentRegistration> Components;
	TArray<FUESpecConverterRegistration> Converters;
	TArray<FUESpecStyleTokenRegistration> StyleTokens;
	TArray<FUESpecSignalRegistration> Signals;

	Registry.GetEffectiveComponents(Components);
	Registry.GetEffectiveConverters(Converters);
	Registry.GetEffectiveStyleTokens(StyleTokens);
	Registry.GetEffectiveSignals(Signals);

	for (const FUESpecComponentRegistration& Component : Components)
	{
		const FSoftObjectPath ClassObjectPath = Component.WidgetClass.ToSoftObjectPath();
		RegisterComponentSurface(
			Component.Type,
			ClassObjectPath.GetAssetPathString(),
			ClassObjectPath.ToString(),
			Component.Accepts,
			Component.Keys);
	}

	for (const FUESpecConverterRegistration& Converter : Converters)
	{
		RegisterConverterSurface(Converter.Name, Converter.Signature, Converter.OutputType, Converter.Description);
	}

	for (const FUESpecStyleTokenRegistration& StyleToken : StyleTokens)
	{
		RegisterStyleTokenSurface(StyleToken.Name, StyleToken.Value);
	}

	for (const FUESpecSignalRegistration& Signal : Signals)
	{
		RegisterSignalSurface(Signal.Name.ToString());
	}
}

void UUESpecSupportSurfaceSubsystem::RegisterConverterSurface(const FString& Name, const FString& Signature, const FString& OutputType, const FString& Description)
{
	FUESpecSurfaceConverterEntry Entry;
	Entry.Name = Name;
	Entry.Signature = Signature;
	Entry.OutputType = OutputType;
	Entry.Description = Description;

	UE::UESpec::Private::ReplaceOrAddEntry(
		RegisteredConverterSurfaces,
		Entry,
		[&Entry](const FUESpecSurfaceConverterEntry& Existing)
		{
			return Existing.Name == Entry.Name;
		});
}

void UUESpecSupportSurfaceSubsystem::RegisterStyleTokenSurface(const FString& Name, const FString& Value)
{
	FUESpecSurfaceStyleTokenEntry Entry;
	Entry.Name = Name.StartsWith(TEXT("@")) ? Name : FString::Printf(TEXT("@%s"), *Name);
	Entry.Value = Value;

	UE::UESpec::Private::ReplaceOrAddEntry(
		RegisteredStyleTokenSurfaces,
		Entry,
		[&Entry](const FUESpecSurfaceStyleTokenEntry& Existing)
		{
			return Existing.Name == Entry.Name;
		});
}

void UUESpecSupportSurfaceSubsystem::RegisterSignalSurface(const FString& Name)
{
	FUESpecSurfaceSignalEntry Entry;
	Entry.Name = Name;

	UE::UESpec::Private::ReplaceOrAddEntry(
		RegisteredSignalSurfaces,
		Entry,
		[&Entry](const FUESpecSurfaceSignalEntry& Existing)
		{
			return Existing.Name == Entry.Name;
		});
}

FUESpecSupportSurfaceSnapshot UUESpecSupportSurfaceSubsystem::BuildSnapshot() const
{
	FUESpecSupportSurfaceSnapshot Snapshot;
	Snapshot.BuiltInUMGWidgets = UE::UESpec::Private::GetBuiltInUMGEntries();
	Snapshot.CommonUIWidgets = UE::UESpec::Private::GetCommonUIEntries();
	Snapshot.RegisteredComponents = RegisteredComponentSurfaces;
	Snapshot.Converters = RegisteredConverterSurfaces;
	Snapshot.Kinds = UE::UESpec::Private::GetKindEntries();
	Snapshot.StyleTokens = RegisteredStyleTokenSurfaces;
	Snapshot.Signals = RegisteredSignalSurfaces;
	Snapshot.ActionEvents = GetActionEventSurface();
	Snapshot.ActionPrimitives = GetActionPrimitiveSurface();

	Snapshot.RegisteredComponents.Sort([](const FUESpecSurfaceWidgetEntry& A, const FUESpecSurfaceWidgetEntry& B)
	{
		return A.Type < B.Type;
	});

	Snapshot.Converters.Sort([](const FUESpecSurfaceConverterEntry& A, const FUESpecSurfaceConverterEntry& B)
	{
		return A.Name < B.Name;
	});

	Snapshot.StyleTokens.Sort([](const FUESpecSurfaceStyleTokenEntry& A, const FUESpecSurfaceStyleTokenEntry& B)
	{
		return A.Name < B.Name;
	});

	Snapshot.Signals.Sort([](const FUESpecSurfaceSignalEntry& A, const FUESpecSurfaceSignalEntry& B)
	{
		return A.Name < B.Name;
	});

	return Snapshot;
}

void UUESpecSupportSurfaceSubsystem::RegisterCanonicalConverters()
{
	RegisteredConverterSurfaces.Reset();

	for (TFieldIterator<UFunction> FunctionIt(UUESpecConverterLibrary::StaticClass(), EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if (!Function || !Function->HasAllFunctionFlags(FUNC_Static) || !Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
		{
			continue;
		}

		FString ConverterName = Function->GetName();
		ConverterName.RemoveFromStart(TEXT("Conv_"));

		FString ReturnType = TEXT("void");
		if (const FProperty* ReturnProperty = Function->GetReturnProperty())
		{
			ReturnType = ReturnProperty->GetCPPType(nullptr, CPPF_None);
		}

		RegisterConverterSurface(
			ConverterName,
			UE::UESpec::Private::BuildConverterSignature(*Function, ConverterName),
			ReturnType,
			Function->GetMetaData(TEXT("ToolTip")));
	}
}
