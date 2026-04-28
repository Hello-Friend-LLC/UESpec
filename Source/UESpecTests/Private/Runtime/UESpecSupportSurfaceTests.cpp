#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Registry/UESpecComponentRegistry.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "Widgets/UESpecWidgetBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecSupportSurfaceRegistrySettingsTest,
	"UESpec.Runtime.SupportSurface.RegistrySettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecSupportSurfaceRegistrySettingsTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is not available."));
		return false;
	}

	UUESpecSupportSurfaceSubsystem* Subsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>();
	if (!TestNotNull(TEXT("Support surface subsystem"), Subsystem))
	{
		return false;
	}

	UUESpecComponentRegistry* Registry = GetMutableDefault<UUESpecComponentRegistry>();
	const TArray<FUESpecComponentRegistration> SavedComponents = Registry->Components;
	const TArray<FUESpecConverterRegistration> SavedConverters = Registry->Converters;
	const TArray<FUESpecStyleTokenRegistration> SavedStyleTokens = Registry->StyleTokens;
	const TArray<FUESpecSignalRegistration> SavedSignals = Registry->Signals;

	ON_SCOPE_EXIT
	{
		Registry->Components = SavedComponents;
		Registry->Converters = SavedConverters;
		Registry->StyleTokens = SavedStyleTokens;
		Registry->Signals = SavedSignals;
		Subsystem->ResetProjectSurface();
	};

	FUESpecComponentRegistration Component;
	Component.Type = TEXT("SkillSlot");
	Component.WidgetClass = UUESpecWidgetBase::StaticClass();
	Component.Accepts = { TEXT("icon"), TEXT("cooldown"), TEXT("keybind") };
	Component.Keys = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };

	FUESpecConverterRegistration Converter;
	Converter.Name = TEXT("CooldownLabel");
	Converter.Signature = TEXT("CooldownLabel(seconds: number)");
	Converter.OutputType = TEXT("FText");
	Converter.Description = TEXT("Formats cooldown values for test coverage.");

	FUESpecStyleTokenRegistration StyleToken;
	StyleToken.Name = TEXT("color.primary");
	StyleToken.Value = TEXT("#FF3A7BFF");

	FUESpecSignalRegistration Signal;
	Signal.Name = TEXT("QuestCompleted");

	Registry->Components = { Component };
	Registry->Converters.Add(Converter);
	Registry->StyleTokens = { StyleToken };
	Registry->Signals = { Signal };

	Subsystem->ResetProjectSurface();
	const FUESpecSupportSurfaceSnapshot Snapshot = Subsystem->BuildSnapshot();

	TestTrue(
		TEXT("Registered component is exposed"),
		Snapshot.RegisteredComponents.ContainsByPredicate([](const FUESpecSurfaceWidgetEntry& Entry)
		{
			return Entry.Type == TEXT("SkillSlot") && Entry.ClassPath.Contains(TEXT("UESpecWidgetBase"));
		}));

	TestTrue(
		TEXT("Custom converter is exposed"),
		Snapshot.Converters.ContainsByPredicate([](const FUESpecSurfaceConverterEntry& Entry)
		{
			return Entry.Name == TEXT("CooldownLabel");
		}));

	TestTrue(
		TEXT("Project style token is exposed"),
		Snapshot.StyleTokens.ContainsByPredicate([](const FUESpecSurfaceStyleTokenEntry& Entry)
		{
			return Entry.Name == TEXT("@color.primary") && Entry.Value == TEXT("#FF3A7BFF");
		}));

	TestTrue(
		TEXT("Project signal is exposed"),
		Snapshot.Signals.ContainsByPredicate([](const FUESpecSurfaceSignalEntry& Entry)
		{
			return Entry.Name == TEXT("QuestCompleted");
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecSupportSurfaceBuiltinLibraryTest,
	"UESpec.Runtime.SupportSurface.BuiltinLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecSupportSurfaceBuiltinLibraryTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		AddError(TEXT("GEngine is not available."));
		return false;
	}

	UUESpecSupportSurfaceSubsystem* Subsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>();
	if (!TestNotNull(TEXT("Support surface subsystem"), Subsystem))
	{
		return false;
	}

	Subsystem->ResetProjectSurface();
	const FUESpecSupportSurfaceSnapshot Snapshot = Subsystem->BuildSnapshot();

	TestTrue(
		TEXT("HealthBar built-in component is exposed"),
		Snapshot.RegisteredComponents.ContainsByPredicate([](const FUESpecSurfaceWidgetEntry& Entry)
		{
			return Entry.Type == TEXT("HealthBar");
		}));

	TestTrue(
		TEXT("ResourceBar built-in component is exposed"),
		Snapshot.RegisteredComponents.ContainsByPredicate([](const FUESpecSurfaceWidgetEntry& Entry)
		{
			return Entry.Type == TEXT("ResourceBar");
		}));

	TestTrue(
		TEXT("PlayerNamePlate built-in component is exposed"),
		Snapshot.RegisteredComponents.ContainsByPredicate([](const FUESpecSurfaceWidgetEntry& Entry)
		{
			return Entry.Type == TEXT("PlayerNamePlate");
		}));

	TestTrue(
		TEXT("ItemSlot built-in component is exposed"),
		Snapshot.RegisteredComponents.ContainsByPredicate([](const FUESpecSurfaceWidgetEntry& Entry)
		{
			return Entry.Type == TEXT("ItemSlot");
		}));

	TestTrue(
		TEXT("Built-in health fill token is exposed"),
		Snapshot.StyleTokens.ContainsByPredicate([](const FUESpecSurfaceStyleTokenEntry& Entry)
		{
			return Entry.Name == TEXT("@color.health.fill");
		}));

	TestTrue(
		TEXT("Built-in damage critical token is exposed"),
		Snapshot.StyleTokens.ContainsByPredicate([](const FUESpecSurfaceStyleTokenEntry& Entry)
		{
			return Entry.Name == TEXT("@color.damage.critical");
		}));

	TestTrue(
		TEXT("Built-in item legendary token is exposed"),
		Snapshot.StyleTokens.ContainsByPredicate([](const FUESpecSurfaceStyleTokenEntry& Entry)
		{
			return Entry.Name == TEXT("@color.item.legendary");
		}));

	TestTrue(
		TEXT("Built-in onSkillUsed signal is exposed"),
		Snapshot.Signals.ContainsByPredicate([](const FUESpecSurfaceSignalEntry& Entry)
		{
			return Entry.Name == TEXT("onSkillUsed");
		}));

	TestTrue(
		TEXT("Built-in onTargetChanged signal is exposed"),
		Snapshot.Signals.ContainsByPredicate([](const FUESpecSurfaceSignalEntry& Entry)
		{
			return Entry.Name == TEXT("onTargetChanged");
		}));

	TestTrue(
		TEXT("Built-in onItemEquipped signal is exposed"),
		Snapshot.Signals.ContainsByPredicate([](const FUESpecSurfaceSignalEntry& Entry)
		{
			return Entry.Name == TEXT("onItemEquipped");
		}));

	return true;
}
