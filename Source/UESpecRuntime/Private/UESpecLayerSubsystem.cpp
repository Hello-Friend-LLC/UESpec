#include "UESpecLayerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecLayerSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUESpecLayerSubsystem, Log, All);

namespace UE::UESpec::Private
{
	constexpr int32 LayerBaseZOrder = 100;
}

void UUESpecLayerSubsystem::RegisterLayerClass(FName LayerName, TSubclassOf<UUESpecActivatableWidgetBase> WidgetClass)
{
	if (LayerName.IsNone())
	{
		UE_LOG(LogUESpecLayerSubsystem, Warning, TEXT("Ignoring layer registration with an empty layer name."));
		return;
	}

	if (!WidgetClass)
	{
		RegisteredLayerClasses.Remove(LayerName);
		return;
	}

	RegisteredLayerClasses.Add(LayerName, WidgetClass);
}

void UUESpecLayerSubsystem::UnregisterLayerClass(FName LayerName)
{
	if (!LayerName.IsNone())
	{
		RegisteredLayerClasses.Remove(LayerName);
	}
}

void UUESpecLayerSubsystem::PushLayer(FName LayerName)
{
	CompactLayerStack();

	UUESpecActivatableWidgetBase* const Widget = CreateLayerWidget(LayerName);
	if (!Widget)
	{
		return;
	}

	const int32 ZOrder = UE::UESpec::Private::LayerBaseZOrder + LayerStack.Num();
	if (Widget->GetWorld())
	{
		Widget->AddToViewport(ZOrder);
	}
	Widget->ActivateWidget();

	FUESpecLayerStackEntry& Entry = LayerStack.AddDefaulted_GetRef();
	Entry.LayerName = LayerName;
	Entry.Widget = Widget;
}

void UUESpecLayerSubsystem::CloseTopLayer()
{
	CompactLayerStack();

	if (LayerStack.IsEmpty())
	{
		return;
	}

	CloseLayerEntryAt(LayerStack.Num() - 1);
}

void UUESpecLayerSubsystem::CloseLayerByName(FName LayerName)
{
	CompactLayerStack();

	if (LayerName.IsNone())
	{
		return;
	}

	for (int32 EntryIndex = LayerStack.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (LayerStack[EntryIndex].LayerName == LayerName)
		{
			CloseLayerEntryAt(EntryIndex);
			return;
		}
	}
}

UUESpecActivatableWidgetBase* UUESpecLayerSubsystem::GetTopLayer() const
{
	for (int32 EntryIndex = LayerStack.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (IsValid(LayerStack[EntryIndex].Widget))
		{
			return LayerStack[EntryIndex].Widget;
		}
	}

	return nullptr;
}

void UUESpecLayerSubsystem::CompactLayerStack()
{
	LayerStack.RemoveAll([](const FUESpecLayerStackEntry& Entry)
	{
		return !IsValid(Entry.Widget);
	});
}

UUESpecActivatableWidgetBase* UUESpecLayerSubsystem::CreateLayerWidget(FName LayerName) const
{
	if (LayerName.IsNone())
	{
		UE_LOG(LogUESpecLayerSubsystem, Warning, TEXT("Ignoring PushLayer with an empty layer name."));
		return nullptr;
	}

	const TSubclassOf<UUESpecActivatableWidgetBase>* const WidgetClass = RegisteredLayerClasses.Find(LayerName);
	if (!WidgetClass || !(*WidgetClass))
	{
		UE_LOG(LogUESpecLayerSubsystem, Warning, TEXT("No widget class registered for layer '%s'."), *LayerName.ToString());
		return nullptr;
	}

	UGameInstance* const GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogUESpecLayerSubsystem, Warning, TEXT("Unable to create layer '%s' because the subsystem has no game instance."), *LayerName.ToString());
		return nullptr;
	}

	return CreateWidget<UUESpecActivatableWidgetBase>(GameInstance, *WidgetClass, LayerName);
}

void UUESpecLayerSubsystem::CloseLayerEntryAt(int32 EntryIndex)
{
	if (!LayerStack.IsValidIndex(EntryIndex))
	{
		return;
	}

	UUESpecActivatableWidgetBase* const Widget = LayerStack[EntryIndex].Widget;
	LayerStack.RemoveAt(EntryIndex);

	if (!IsValid(Widget))
	{
		return;
	}

	if (Widget->IsActivated())
	{
		Widget->DeactivateWidget();
	}

	Widget->RemoveFromParent();
}
