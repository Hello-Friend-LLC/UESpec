#include "Samples/UESpecSampleWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Samples/UESpecAdventureOverlayViewModel.h"
#include "UESpecSignalBus.h"
#include "Samples/UESpecCombatStatusViewModel.h"
#include "Samples/UESpecInventorySocialViewModel.h"
#include "Samples/UESpecRound6ShowcaseViewModel.h"
#include "Samples/UESpecSampleSkillBarViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecSampleWidgets)

namespace
{
	static FUESpecStateBindingProperty MakePropertyBinding(const FName PropertyName, const FString& Value)
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
}

void UUESpecStateMachineTestWidget::InitializeTestTree()
{
	WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);

	UHorizontalBox* const RootPanel = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("skillRow"));
	UButton* const SlotQButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("slotQ"));
	UProgressBar* const CooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("slotQ_cooldownBar"));
	UTextBlock* const KeybindText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("slotQ_keybind"));
	UButton* const SlotWButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("slotW"));
	UTextBlock* const SlotQButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("slotQ_buttonLabel"));
	UTextBlock* const SlotWButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("slotW_buttonLabel"));
	if (RootPanel && SlotQButton && CooldownBar && KeybindText && SlotWButton && SlotQButtonLabel && SlotWButtonLabel)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SlotQButton->IsFocusable = true;
		SlotWButton->IsFocusable = true;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		SlotQButton->SetNavigationRuleExplicit(EUINavigation::Right, SlotWButton);
		SlotWButton->SetNavigationRuleExplicit(EUINavigation::Left, SlotQButton);

		CooldownBar->SetVisibility(ESlateVisibility::Collapsed);
		CooldownBar->SetPercent(0.5f);
		KeybindText->SetText(FText::FromString(TEXT("Q")));
		SlotQButtonLabel->SetText(FText::FromString(TEXT("Q")));
		SlotWButtonLabel->SetText(FText::FromString(TEXT("W")));
		SlotQButton->AddChild(SlotQButtonLabel);
		SlotWButton->AddChild(SlotWButtonLabel);

		SlotQButton->OnClicked.AddDynamic(this, &UUESpecStateMachineTestWidget::HandleSlotQClicked);
		SlotQButton->OnHovered.AddDynamic(this, &UUESpecStateMachineTestWidget::HandleSlotQHovered);
		SlotQButton->OnUnhovered.AddDynamic(this, &UUESpecStateMachineTestWidget::HandleSlotQUnhovered);

		RootPanel->AddChild(SlotQButton);
		RootPanel->AddChild(CooldownBar);
		RootPanel->AddChild(KeybindText);
		RootPanel->AddChild(SlotWButton);
		WidgetTree->RootWidget = RootPanel;
	}

	if (ViewModelClass.IsNull())
	{
		ViewModelClass = UVM_SkillBar::StaticClass();
	}

	if (StateMachine.States.IsEmpty())
	{
		StateMachine.InitialState = TEXT("Normal");

		FUESpecStateDefinition& NormalState = StateMachine.States.AddDefaulted_GetRef();
		NormalState.Name = TEXT("Normal");

		FUESpecStateDefinition& CooldownState = StateMachine.States.AddDefaulted_GetRef();
		CooldownState.Name = TEXT("Cooldown");

		AddStateBinding(StateMachine, TEXT("slotQ"), TEXT("Normal"), { MakePropertyBinding(TEXT("opacity"), TEXT("1.0")) });
		AddStateBinding(StateMachine, TEXT("slotQ"), TEXT("Cooldown"), { MakePropertyBinding(TEXT("opacity"), TEXT("0.6")) });
		AddStateBinding(StateMachine, TEXT("slotQ_cooldownBar"), TEXT("Normal"), { MakePropertyBinding(TEXT("visibility"), TEXT("Collapsed")) });
		AddStateBinding(StateMachine, TEXT("slotQ_cooldownBar"), TEXT("Cooldown"), { MakePropertyBinding(TEXT("visibility"), TEXT("Visible")) });
	}

	bSlotQHovered = false;
}

void UUESpecStateMachineTestWidget::RunTestLifecycle()
{
	NativeOnInitialized();
	NativeConstruct();
}

void UUESpecStateMachineTestWidget::RunTestTick(const float DeltaSeconds)
{
	NativeTick(FGeometry(), DeltaSeconds);
}

void UUESpecStateMachineTestWidget::AttachSignalBus(UUESpecSignalBus* SignalBus)
{
	BindSignalBus(SignalBus);
}

UWidget* UUESpecStateMachineTestWidget::FindTestWidgetByName(const FName WidgetName) const
{
	return WidgetTree ? WidgetTree->FindWidget(WidgetName) : nullptr;
}

UVM_SkillBar* UUESpecStateMachineTestWidget::GetSkillBarViewModel() const
{
	return Cast<UVM_SkillBar>(GetUESpecViewModel());
}

bool UUESpecStateMachineTestWidget::IsSlotQHovered() const
{
	return bSlotQHovered;
}

void UUESpecStateMachineTestWidget::RefreshStateFromViewModel()
{
	if (UVM_SkillBar* const ViewModel = GetSkillBarViewModel())
	{
		ViewModel->SyncPrimarySkillState();
		SetUIState(ViewModel->bShowCooldown ? TEXT("Cooldown") : TEXT("Normal"));
	}
}

void UUESpecStateMachineTestWidget::HandleSlotQClicked()
{
	UVM_SkillBar* const ViewModel = GetSkillBarViewModel();
	if (!ViewModel || ViewModel->IsSkillLocked(0))
	{
		return;
	}

	if (ViewModel->IsSkillReady(0))
	{
		ViewModel->UseSkill(0);
		ViewModel->SyncPrimarySkillState();
	}

	if (!ViewModel->bShowCooldown && !ViewModel->IsSkillReady(0))
	{
		return;
	}

	SetUIState(TEXT("Cooldown"));
}

void UUESpecStateMachineTestWidget::HandleSlotQHovered()
{
	bSlotQHovered = true;
}

void UUESpecStateMachineTestWidget::HandleSlotQUnhovered()
{
	bSlotQHovered = false;
}

void UUESpecSignalProbe::HandleSignal(const FName SignalName)
{
	LastSignal = SignalName;
	++BroadcastCount;
}

void UUESpecCombatStatusTestWidget::InitializeCombatStatusTree()
{
	WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);

	UCanvasPanel* const RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("combatHudRoot"));
	UTextBlock* const PlayerName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerName"));
	UTextBlock* const PlayerLevel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerLevel"));
	UProgressBar* const PlayerHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("playerHealthBar"));
	UTextBlock* const PlayerHealthValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerHealthValue"));
	UProgressBar* const PlayerResourceBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("playerResourceBar"));
	UTextBlock* const PlayerResourceValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerResourceValue"));

	if (RootPanel && PlayerName && PlayerLevel && PlayerHealthBar && PlayerHealthValue && PlayerResourceBar && PlayerResourceValue)
	{
		RootPanel->AddChild(PlayerName);
		RootPanel->AddChild(PlayerLevel);
		RootPanel->AddChild(PlayerHealthBar);
		RootPanel->AddChild(PlayerHealthValue);
		RootPanel->AddChild(PlayerResourceBar);
		RootPanel->AddChild(PlayerResourceValue);
		WidgetTree->RootWidget = RootPanel;
	}

	if (ViewModelClass.IsNull())
	{
		ViewModelClass = UVM_CombatStatus::StaticClass();
	}
}

void UUESpecCombatStatusTestWidget::RunCombatStatusLifecycle()
{
	NativeOnInitialized();
	NativeConstruct();
	RefreshCombatStatusFromViewModel();
}

void UUESpecCombatStatusTestWidget::RefreshCombatStatusFromViewModel()
{
	UVM_CombatStatus* const ViewModel = GetCombatStatusViewModel();
	if (!WidgetTree || !ViewModel)
	{
		return;
	}

	ViewModel->RefreshPresentation();

	if (UTextBlock* const PlayerName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerName"))))
	{
		PlayerName->SetText(ViewModel->DisplayName);
	}

	if (UTextBlock* const PlayerLevel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerLevel"))))
	{
		PlayerLevel->SetText(ViewModel->LevelLabel);
	}

	if (UProgressBar* const PlayerHealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("playerHealthBar"))))
	{
		PlayerHealthBar->SetPercent(ViewModel->HealthRatio);
	}

	if (UTextBlock* const PlayerHealthValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerHealthValue"))))
	{
		PlayerHealthValue->SetText(ViewModel->HealthLabel);
	}

	if (UProgressBar* const PlayerResourceBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("playerResourceBar"))))
	{
		PlayerResourceBar->SetPercent(ViewModel->ResourceRatio);
		PlayerResourceBar->SetVisibility(ViewModel->bShowResourceBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const PlayerResourceValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerResourceValue"))))
	{
		PlayerResourceValue->SetText(ViewModel->ResourceLabel);
		PlayerResourceValue->SetVisibility(ViewModel->bShowResourceBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UVM_CombatStatus* UUESpecCombatStatusTestWidget::GetCombatStatusViewModel() const
{
	return Cast<UVM_CombatStatus>(GetUESpecViewModel());
}

void UUESpecAdventureOverlayTestWidget::InitializeAdventureOverlayTree()
{
	WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);

	UCanvasPanel* const RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("adventureOverlayRoot"));
	UTextBlock* const TargetName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetName"));
	UTextBlock* const TargetLevel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetLevel"));
	UProgressBar* const TargetHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("targetHealthBar"));
	UTextBlock* const TargetHealthValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetHealthValue"));
	UImage* const TargetBuffIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("targetBuffIcon"));
	UTextBlock* const TargetBuffTimer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetBuffTimer"));
	UTextBlock* const RecentDamage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("recentDamage"));
	UTextBlock* const TrackedQuestTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("trackedQuestTitle"));
	UTextBlock* const TrackedQuestProgress = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("trackedQuestProgress"));

	if (RootPanel && TargetName && TargetLevel && TargetHealthBar && TargetHealthValue && TargetBuffIcon && TargetBuffTimer && RecentDamage && TrackedQuestTitle && TrackedQuestProgress)
	{
		RootPanel->AddChild(TargetName);
		RootPanel->AddChild(TargetLevel);
		RootPanel->AddChild(TargetHealthBar);
		RootPanel->AddChild(TargetHealthValue);
		RootPanel->AddChild(TargetBuffIcon);
		RootPanel->AddChild(TargetBuffTimer);
		RootPanel->AddChild(RecentDamage);
		RootPanel->AddChild(TrackedQuestTitle);
		RootPanel->AddChild(TrackedQuestProgress);
		WidgetTree->RootWidget = RootPanel;
	}

	if (ViewModelClass.IsNull())
	{
		ViewModelClass = UVM_AdventureOverlay::StaticClass();
	}
}

void UUESpecAdventureOverlayTestWidget::RunAdventureOverlayLifecycle()
{
	NativeOnInitialized();
	NativeConstruct();
	RefreshAdventureOverlayFromViewModel();
}

void UUESpecAdventureOverlayTestWidget::RefreshAdventureOverlayFromViewModel()
{
	UVM_AdventureOverlay* const ViewModel = GetAdventureOverlayViewModel();
	if (!WidgetTree || !ViewModel)
	{
		return;
	}

	ViewModel->RefreshPresentation();

	if (UTextBlock* const TargetName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetName"))))
	{
		TargetName->SetText(ViewModel->TargetName);
	}

	if (UTextBlock* const TargetLevel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetLevel"))))
	{
		TargetLevel->SetText(ViewModel->TargetLevelLabel);
	}

	if (UProgressBar* const TargetHealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("targetHealthBar"))))
	{
		TargetHealthBar->SetPercent(ViewModel->TargetHealthRatio);
	}

	if (UTextBlock* const TargetHealthValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetHealthValue"))))
	{
		TargetHealthValue->SetText(ViewModel->TargetHealthLabel);
	}

	if (UImage* const TargetBuffIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("targetBuffIcon"))))
	{
		TargetBuffIcon->SetVisibility(ViewModel->bShowPrimaryBuff ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TargetBuffTimer = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetBuffTimer"))))
	{
		TargetBuffTimer->SetText(ViewModel->PrimaryBuffTimerLabel);
		TargetBuffTimer->SetVisibility(ViewModel->bShowPrimaryBuff ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const RecentDamage = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("recentDamage"))))
	{
		RecentDamage->SetText(ViewModel->DamageNumberLabel);
		RecentDamage->SetVisibility(ViewModel->bShowDamageNumber ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TrackedQuestTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("trackedQuestTitle"))))
	{
		TrackedQuestTitle->SetText(ViewModel->TrackedQuestTitle);
	}

	if (UTextBlock* const TrackedQuestProgress = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("trackedQuestProgress"))))
	{
		TrackedQuestProgress->SetText(ViewModel->TrackedQuestProgress);
	}
}

UVM_AdventureOverlay* UUESpecAdventureOverlayTestWidget::GetAdventureOverlayViewModel() const
{
	return Cast<UVM_AdventureOverlay>(GetUESpecViewModel());
}

void UUESpecInventorySocialOverlayTestWidget::InitializeInventorySocialTree()
{
	WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);

	UCanvasPanel* const RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("inventorySocialRoot"));
	UTextBlock* const EquipmentPanelTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equipmentPanelTitle"));
	UImage* const EquippedHelmetIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("equippedHelmetIcon"));
	UTextBlock* const EquippedHelmetName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equippedHelmetName"));
	UTextBlock* const EquippedHelmetPower = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equippedHelmetPower"));
	UTextBlock* const TooltipTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("itemTooltipTitle"));
	UTextBlock* const TooltipSummary = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("itemTooltipSummary"));
	UTextBlock* const ChatSpeaker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("chatSpeaker"));
	UTextBlock* const ChatMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("chatMessage"));

	if (RootPanel && EquipmentPanelTitle && EquippedHelmetIcon && EquippedHelmetName && EquippedHelmetPower && TooltipTitle && TooltipSummary && ChatSpeaker && ChatMessage)
	{
		RootPanel->AddChild(EquipmentPanelTitle);
		RootPanel->AddChild(EquippedHelmetIcon);
		RootPanel->AddChild(EquippedHelmetName);
		RootPanel->AddChild(EquippedHelmetPower);
		RootPanel->AddChild(TooltipTitle);
		RootPanel->AddChild(TooltipSummary);
		RootPanel->AddChild(ChatSpeaker);
		RootPanel->AddChild(ChatMessage);
		WidgetTree->RootWidget = RootPanel;
	}

	if (ViewModelClass.IsNull())
	{
		ViewModelClass = UVM_InventorySocialOverlay::StaticClass();
	}
}

void UUESpecInventorySocialOverlayTestWidget::RunInventorySocialLifecycle()
{
	NativeOnInitialized();
	NativeConstruct();
	RefreshInventorySocialFromViewModel();
}

void UUESpecInventorySocialOverlayTestWidget::RefreshInventorySocialFromViewModel()
{
	UVM_InventorySocialOverlay* const ViewModel = GetInventorySocialViewModel();
	if (!WidgetTree || !ViewModel)
	{
		return;
	}

	if (UTextBlock* const EquipmentPanelTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equipmentPanelTitle"))))
	{
		EquipmentPanelTitle->SetText(ViewModel->EquipmentPanelTitle);
	}

	if (UImage* const EquippedHelmetIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("equippedHelmetIcon"))))
	{
		EquippedHelmetIcon->SetVisibility(ESlateVisibility::Visible);
	}

	if (UTextBlock* const EquippedHelmetName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equippedHelmetName"))))
	{
		EquippedHelmetName->SetText(ViewModel->EquippedHelmetName);
	}

	if (UTextBlock* const EquippedHelmetPower = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equippedHelmetPower"))))
	{
		EquippedHelmetPower->SetText(ViewModel->EquippedHelmetPower);
	}

	if (UTextBlock* const TooltipTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("itemTooltipTitle"))))
	{
		TooltipTitle->SetText(ViewModel->TooltipTitle);
		TooltipTitle->SetVisibility(ViewModel->bShowTooltip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TooltipSummary = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("itemTooltipSummary"))))
	{
		TooltipSummary->SetText(ViewModel->TooltipSummary);
		TooltipSummary->SetVisibility(ViewModel->bShowTooltip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const ChatSpeaker = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("chatSpeaker"))))
	{
		ChatSpeaker->SetText(ViewModel->ChatSpeaker);
		ChatSpeaker->SetVisibility(ViewModel->bShowChatLine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const ChatMessage = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("chatMessage"))))
	{
		ChatMessage->SetText(ViewModel->ChatMessage);
		ChatMessage->SetVisibility(ViewModel->bShowChatLine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UVM_InventorySocialOverlay* UUESpecInventorySocialOverlayTestWidget::GetInventorySocialViewModel() const
{
	return Cast<UVM_InventorySocialOverlay>(GetUESpecViewModel());
}

void UUESpecRound6ShowcaseTestWidget::InitializeRound6ShowcaseTree()
{
	WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);

	UCanvasPanel* const RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("round6ShowcaseRoot"));
	UTextBlock* const PlayerName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerName"));
	UTextBlock* const PlayerLevel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerLevel"));
	UProgressBar* const PlayerHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("playerHealthBar"));
	UTextBlock* const PlayerHealthValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerHealthValue"));
	UProgressBar* const PlayerResourceBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("playerResourceBar"));
	UTextBlock* const PlayerResourceValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("playerResourceValue"));
	UTextBlock* const SkillKeybind = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("skillKeybind"));
	UImage* const SkillAlertDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("skillAlertDot"));
	UTextBlock* const TargetName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetName"));
	UTextBlock* const TargetLevel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetLevel"));
	UProgressBar* const TargetHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("targetHealthBar"));
	UTextBlock* const TargetHealthValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetHealthValue"));
	UImage* const TargetBuffIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("targetBuffIcon"));
	UTextBlock* const TargetBuffTimer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("targetBuffTimer"));
	UTextBlock* const RecentDamage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("recentDamage"));
	UTextBlock* const TrackedQuestTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("trackedQuestTitle"));
	UTextBlock* const TrackedQuestProgress = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("trackedQuestProgress"));
	UTextBlock* const EquipmentPanelTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equipmentPanelTitle"));
	UImage* const EquippedItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("equippedItemIcon"));
	UTextBlock* const EquippedItemName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equippedItemName"));
	UTextBlock* const EquippedItemPower = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("equippedItemPower"));
	UTextBlock* const TooltipTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("itemTooltipTitle"));
	UTextBlock* const TooltipSummary = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("itemTooltipSummary"));
	UTextBlock* const ChatSpeaker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("chatSpeaker"));
	UTextBlock* const ChatMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("chatMessage"));

	if (RootPanel && PlayerName && PlayerLevel && PlayerHealthBar && PlayerHealthValue && PlayerResourceBar && PlayerResourceValue
		&& SkillKeybind && SkillAlertDot && TargetName && TargetLevel && TargetHealthBar && TargetHealthValue
		&& TargetBuffIcon && TargetBuffTimer && RecentDamage && TrackedQuestTitle && TrackedQuestProgress
		&& EquipmentPanelTitle && EquippedItemIcon && EquippedItemName && EquippedItemPower
		&& TooltipTitle && TooltipSummary && ChatSpeaker && ChatMessage)
	{
		RootPanel->AddChild(PlayerName);
		RootPanel->AddChild(PlayerLevel);
		RootPanel->AddChild(PlayerHealthBar);
		RootPanel->AddChild(PlayerHealthValue);
		RootPanel->AddChild(PlayerResourceBar);
		RootPanel->AddChild(PlayerResourceValue);
		RootPanel->AddChild(SkillKeybind);
		RootPanel->AddChild(SkillAlertDot);
		RootPanel->AddChild(TargetName);
		RootPanel->AddChild(TargetLevel);
		RootPanel->AddChild(TargetHealthBar);
		RootPanel->AddChild(TargetHealthValue);
		RootPanel->AddChild(TargetBuffIcon);
		RootPanel->AddChild(TargetBuffTimer);
		RootPanel->AddChild(RecentDamage);
		RootPanel->AddChild(TrackedQuestTitle);
		RootPanel->AddChild(TrackedQuestProgress);
		RootPanel->AddChild(EquipmentPanelTitle);
		RootPanel->AddChild(EquippedItemIcon);
		RootPanel->AddChild(EquippedItemName);
		RootPanel->AddChild(EquippedItemPower);
		RootPanel->AddChild(TooltipTitle);
		RootPanel->AddChild(TooltipSummary);
		RootPanel->AddChild(ChatSpeaker);
		RootPanel->AddChild(ChatMessage);
		WidgetTree->RootWidget = RootPanel;
	}

	if (ViewModelClass.IsNull())
	{
		ViewModelClass = UVM_Round6Showcase::StaticClass();
	}
}

void UUESpecRound6ShowcaseTestWidget::RunRound6ShowcaseLifecycle()
{
	NativeOnInitialized();
	NativeConstruct();
	RefreshRound6ShowcaseFromViewModel();
}

void UUESpecRound6ShowcaseTestWidget::RefreshRound6ShowcaseFromViewModel()
{
	UVM_Round6Showcase* const ViewModel = GetRound6ShowcaseViewModel();
	if (!WidgetTree || !ViewModel)
	{
		return;
	}

	if (UTextBlock* const PlayerName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerName"))))
	{
		PlayerName->SetText(ViewModel->PlayerName);
	}

	if (UTextBlock* const PlayerLevel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerLevel"))))
	{
		PlayerLevel->SetText(ViewModel->PlayerLevel);
	}

	if (UProgressBar* const PlayerHealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("playerHealthBar"))))
	{
		PlayerHealthBar->SetPercent(ViewModel->PlayerHealthRatio);
	}

	if (UTextBlock* const PlayerHealthValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerHealthValue"))))
	{
		PlayerHealthValue->SetText(ViewModel->PlayerHealthLabel);
	}

	if (UProgressBar* const PlayerResourceBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("playerResourceBar"))))
	{
		PlayerResourceBar->SetPercent(ViewModel->PlayerResourceRatio);
	}

	if (UTextBlock* const PlayerResourceValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("playerResourceValue"))))
	{
		PlayerResourceValue->SetText(ViewModel->PlayerResourceLabel);
	}

	if (UTextBlock* const SkillKeybind = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("skillKeybind"))))
	{
		SkillKeybind->SetText(ViewModel->SkillKeybind);
	}

	if (UImage* const SkillAlertDot = Cast<UImage>(WidgetTree->FindWidget(TEXT("skillAlertDot"))))
	{
		SkillAlertDot->SetVisibility(ViewModel->bShowSkillAlert ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TargetName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetName"))))
	{
		TargetName->SetText(ViewModel->TargetName);
	}

	if (UTextBlock* const TargetLevel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetLevel"))))
	{
		TargetLevel->SetText(ViewModel->TargetLevel);
	}

	if (UProgressBar* const TargetHealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("targetHealthBar"))))
	{
		TargetHealthBar->SetPercent(ViewModel->TargetHealthRatio);
	}

	if (UTextBlock* const TargetHealthValue = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetHealthValue"))))
	{
		TargetHealthValue->SetText(ViewModel->TargetHealthLabel);
	}

	if (UImage* const TargetBuffIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("targetBuffIcon"))))
	{
		TargetBuffIcon->SetVisibility(ViewModel->bShowPrimaryBuff ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TargetBuffTimer = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("targetBuffTimer"))))
	{
		TargetBuffTimer->SetText(ViewModel->BuffTimer);
		TargetBuffTimer->SetVisibility(ViewModel->bShowPrimaryBuff ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const RecentDamage = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("recentDamage"))))
	{
		RecentDamage->SetText(ViewModel->DamageNumberLabel);
		RecentDamage->SetVisibility(ViewModel->bShowDamageNumber ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TrackedQuestTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("trackedQuestTitle"))))
	{
		TrackedQuestTitle->SetText(ViewModel->QuestTitle);
	}

	if (UTextBlock* const TrackedQuestProgress = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("trackedQuestProgress"))))
	{
		TrackedQuestProgress->SetText(ViewModel->QuestProgress);
	}

	if (UTextBlock* const EquipmentPanelTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equipmentPanelTitle"))))
	{
		EquipmentPanelTitle->SetText(ViewModel->EquipmentPanelTitle);
	}

	if (UImage* const EquippedItemIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("equippedItemIcon"))))
	{
		EquippedItemIcon->SetVisibility(ESlateVisibility::Visible);
	}

	if (UTextBlock* const EquippedItemName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equippedItemName"))))
	{
		EquippedItemName->SetText(ViewModel->EquippedItemName);
	}

	if (UTextBlock* const EquippedItemPower = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("equippedItemPower"))))
	{
		EquippedItemPower->SetText(ViewModel->EquippedItemPower);
	}

	if (UTextBlock* const TooltipTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("itemTooltipTitle"))))
	{
		TooltipTitle->SetText(ViewModel->TooltipTitle);
		TooltipTitle->SetVisibility(ViewModel->bShowTooltip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const TooltipSummary = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("itemTooltipSummary"))))
	{
		TooltipSummary->SetText(ViewModel->TooltipSummary);
		TooltipSummary->SetVisibility(ViewModel->bShowTooltip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const ChatSpeaker = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("chatSpeaker"))))
	{
		ChatSpeaker->SetText(ViewModel->ChatSpeaker);
		ChatSpeaker->SetVisibility(ViewModel->bShowChatLine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UTextBlock* const ChatMessage = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("chatMessage"))))
	{
		ChatMessage->SetText(ViewModel->ChatMessage);
		ChatMessage->SetVisibility(ViewModel->bShowChatLine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UVM_Round6Showcase* UUESpecRound6ShowcaseTestWidget::GetRound6ShowcaseViewModel() const
{
	return Cast<UVM_Round6Showcase>(GetUESpecViewModel());
}
