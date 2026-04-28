#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Samples/UESpecSampleSkillBarViewModel.h"
#include "Samples/UESpecSampleWidgets.h"
#include "StateMachine/UESpecUIStateMachine.h"
#include "UESpecLayerSubsystem.h"
#include "UESpecSignalBus.h"

namespace
{
	static FUESpecStateBindingProperty MakeStateBindingProperty(const FName PropertyName, const FString& Value)
	{
		FUESpecStateBindingProperty PropertyBinding;
		PropertyBinding.Property = PropertyName;
		PropertyBinding.Value = Value;
		return PropertyBinding;
	}

	static void AddStateBinding(FUESpecStateMachineDefinition& Definition, const FName WidgetId, const FName StateName, std::initializer_list<FUESpecStateBindingProperty> Properties)
	{
		FUESpecWidgetStateBinding& Binding = Definition.StateBindings.AddDefaulted_GetRef();
		Binding.WidgetId = WidgetId;
		Binding.State = StateName;
		Binding.Properties.Append(Properties);
	}

	static UUESpecStateMachineTestWidget* CreateStateMachineWidget(FAutomationTestBase& Test, const FUESpecStateMachineDefinition& Definition, const bool bUseSkillBarViewModel)
	{
		UUESpecStateMachineTestWidget* const Widget = NewObject<UUESpecStateMachineTestWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		Test.TestNotNull(TEXT("State machine test widget allocated"), Widget);
		if (!Widget)
		{
			return nullptr;
		}

		Widget->StateMachine = Definition;
		if (bUseSkillBarViewModel)
		{
			Widget->ViewModelClass = UVM_SkillBar::StaticClass();
		}

		Widget->InitializeTestTree();

		UWidget* const SlotWidget = Widget->FindTestWidgetByName(TEXT("slotQ"));
		Test.TestNotNull(TEXT("slotQ widget exists"), SlotWidget);
		if (SlotWidget)
		{
			SlotWidget->SetRenderOpacity(0.2f);
		}

		return Widget;
	}

	static UWidget* GetSlotWidget(FAutomationTestBase& Test, UUESpecStateMachineTestWidget* Widget)
	{
		UWidget* const SlotWidget = Widget ? Widget->FindTestWidgetByName(TEXT("slotQ")) : nullptr;
		Test.TestNotNull(TEXT("slotQ widget is available"), SlotWidget);
		return SlotWidget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeStateMachineInitialStateAppliedTest,
	"UESpec.Runtime.StateMachine.InitialStateApplied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeStateMachineInitialStateAppliedTest::RunTest(const FString& Parameters)
{
	FUESpecStateMachineDefinition Definition;
	Definition.InitialState = TEXT("Normal");

	FUESpecStateDefinition& NormalState = Definition.States.AddDefaulted_GetRef();
	NormalState.Name = TEXT("Normal");

	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Normal"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("1.0")) });

	UUESpecStateMachineTestWidget* const Widget = CreateStateMachineWidget(*this, Definition, false);
	if (!Widget)
	{
		return false;
	}

	Widget->RunTestLifecycle();

	UWidget* const SlotWidget = GetSlotWidget(*this, Widget);
	if (!SlotWidget)
	{
		return false;
	}

	TestEqual(TEXT("Initial UI state is Normal"), Widget->GetUIState(), FName(TEXT("Normal")));
	TestTrue(TEXT("Normal state binding restores slot opacity"), FMath::IsNearlyEqual(SlotWidget->GetRenderOpacity(), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeStateMachineSetUIStateChangesStyleTest,
	"UESpec.Runtime.StateMachine.SetUIStateChangesStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeStateMachineSetUIStateChangesStyleTest::RunTest(const FString& Parameters)
{
	FUESpecStateMachineDefinition Definition;
	Definition.InitialState = TEXT("Normal");

	FUESpecStateDefinition& NormalState = Definition.States.AddDefaulted_GetRef();
	NormalState.Name = TEXT("Normal");

	FUESpecStateDefinition& CooldownState = Definition.States.AddDefaulted_GetRef();
	CooldownState.Name = TEXT("Cooldown");

	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Normal"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("1.0")) });
	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Cooldown"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("0.6")) });

	UUESpecStateMachineTestWidget* const Widget = CreateStateMachineWidget(*this, Definition, false);
	if (!Widget)
	{
		return false;
	}

	Widget->RunTestLifecycle();
	Widget->SetUIState(TEXT("Cooldown"));

	UWidget* const SlotWidget = GetSlotWidget(*this, Widget);
	if (!SlotWidget)
	{
		return false;
	}

	TestEqual(TEXT("Current UI state becomes Cooldown"), Widget->GetUIState(), FName(TEXT("Cooldown")));
	TestTrue(TEXT("Cooldown state binding applies opacity"), FMath::IsNearlyEqual(SlotWidget->GetRenderOpacity(), 0.6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeStateMachineTransitionOnSignalTest,
	"UESpec.Runtime.StateMachine.TransitionOnSignal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeStateMachineTransitionOnSignalTest::RunTest(const FString& Parameters)
{
	FUESpecStateMachineDefinition Definition;
	Definition.InitialState = TEXT("Normal");

	FUESpecStateDefinition& NormalState = Definition.States.AddDefaulted_GetRef();
	NormalState.Name = TEXT("Normal");
	NormalState.Transitions.Add({ TEXT("Cooldown"), TEXT("onSkillUsed") });

	FUESpecStateDefinition& CooldownState = Definition.States.AddDefaulted_GetRef();
	CooldownState.Name = TEXT("Cooldown");

	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Normal"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("1.0")) });
	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Cooldown"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("0.6")) });

	UUESpecStateMachineTestWidget* const Widget = CreateStateMachineWidget(*this, Definition, false);
	UGameInstance* const GameInstance = NewObject<UGameInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	UUESpecSignalBus* const SignalBus = NewObject<UUESpecSignalBus>(GameInstance, NAME_None, RF_Transient);
	TestNotNull(TEXT("Signal bus allocated"), SignalBus);
	if (!Widget || !SignalBus)
	{
		return false;
	}

	Widget->RunTestLifecycle();
	Widget->AttachSignalBus(SignalBus);
	SignalBus->Broadcast(TEXT("onSkillUsed"));

	UWidget* const SlotWidget = GetSlotWidget(*this, Widget);
	if (!SlotWidget)
	{
		return false;
	}

	TestEqual(TEXT("Signal transition moves to Cooldown"), Widget->GetUIState(), FName(TEXT("Cooldown")));
	TestTrue(TEXT("Signal transition applies cooldown opacity"), FMath::IsNearlyEqual(SlotWidget->GetRenderOpacity(), 0.6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeStateMachineTransitionOnVmPredicateTest,
	"UESpec.Runtime.StateMachine.TransitionOnVmPredicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeStateMachineTransitionOnVmPredicateTest::RunTest(const FString& Parameters)
{
	FUESpecStateMachineDefinition Definition;
	Definition.InitialState = TEXT("Cooldown");

	FUESpecStateDefinition& CooldownState = Definition.States.AddDefaulted_GetRef();
	CooldownState.Name = TEXT("Cooldown");
	CooldownState.Transitions.Add({ TEXT("Normal"), TEXT("vm.IsSkillReady(0)") });

	FUESpecStateDefinition& NormalState = Definition.States.AddDefaulted_GetRef();
	NormalState.Name = TEXT("Normal");

	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Normal"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("1.0")) });
	AddStateBinding(Definition, TEXT("slotQ"), TEXT("Cooldown"), { MakeStateBindingProperty(TEXT("opacity"), TEXT("0.6")) });

	UUESpecStateMachineTestWidget* const Widget = CreateStateMachineWidget(*this, Definition, true);
	if (!Widget)
	{
		return false;
	}

	Widget->RunTestLifecycle();

	UVM_SkillBar* const ViewModel = Widget->GetSkillBarViewModel();
	TestNotNull(TEXT("Skill bar viewmodel is created"), ViewModel);
	if (!ViewModel)
	{
		return false;
	}

	ViewModel->SetSkillReady(false);
	Widget->RunTestTick(0.016f);
	TestEqual(TEXT("Widget remains in Cooldown while predicate is false"), Widget->GetUIState(), FName(TEXT("Cooldown")));

	ViewModel->SetSkillReady(true);
	Widget->RunTestTick(0.016f);

	UWidget* const SlotWidget = GetSlotWidget(*this, Widget);
	if (!SlotWidget)
	{
		return false;
	}

	TestEqual(TEXT("Predicate transition moves to Normal"), Widget->GetUIState(), FName(TEXT("Normal")));
	TestTrue(TEXT("Predicate transition restores normal opacity"), FMath::IsNearlyEqual(SlotWidget->GetRenderOpacity(), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeLayerSubsystemPushPopBasicTest,
	"UESpec.Runtime.LayerSubsystem.PushPopBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeLayerSubsystemPushPopBasicTest::RunTest(const FString& Parameters)
{
	UGameInstance* const GameInstance = NewObject<UGameInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	UUESpecLayerSubsystem* const LayerSubsystem = NewObject<UUESpecLayerSubsystem>(GameInstance, NAME_None, RF_Transient);
	TestNotNull(TEXT("Layer subsystem allocated"), LayerSubsystem);
	if (!LayerSubsystem)
	{
		return false;
	}

	LayerSubsystem->RegisterLayerClass(TEXT("InventoryPanel"), UUESpecLayerTestWidget::StaticClass());
	LayerSubsystem->PushLayer(TEXT("InventoryPanel"));

	UUESpecActivatableWidgetBase* const TopLayer = LayerSubsystem->GetTopLayer();
	TestNotNull(TEXT("Top layer exists after push"), TopLayer);
	TestTrue(TEXT("Top layer uses registered widget class"), TopLayer && TopLayer->GetClass()->IsChildOf(UUESpecLayerTestWidget::StaticClass()));

	LayerSubsystem->CloseTopLayer();
	TestNull(TEXT("Top layer clears after close"), LayerSubsystem->GetTopLayer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecRuntimeSignalBusBroadcastReceiveTest,
	"UESpec.Runtime.SignalBus.BroadcastReceive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecRuntimeSignalBusBroadcastReceiveTest::RunTest(const FString& Parameters)
{
	UGameInstance* const GameInstance = NewObject<UGameInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	UUESpecSignalBus* const SignalBus = NewObject<UUESpecSignalBus>(GameInstance, NAME_None, RF_Transient);
	UUESpecSignalProbe* const Probe = NewObject<UUESpecSignalProbe>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Signal bus allocated"), SignalBus);
	TestNotNull(TEXT("Signal probe allocated"), Probe);
	if (!SignalBus || !Probe)
	{
		return false;
	}

	SignalBus->OnSignal.AddDynamic(Probe, &UUESpecSignalProbe::HandleSignal);
	SignalBus->Broadcast(TEXT("onSkillUsed"));

	TestEqual(TEXT("Probe records the broadcast signal"), Probe->LastSignal, FName(TEXT("onSkillUsed")));
	TestEqual(TEXT("Probe records one broadcast"), Probe->BroadcastCount, 1);
	return true;
}
