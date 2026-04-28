#include "Registry/UESpecBuiltinComponentLibrary.h"

#include "Components/UESpecArpgWidgets.h"

namespace UE::UESpec::ComponentLibrary
{
	void GetBuiltinComponents(TArray<FUESpecComponentRegistration>& OutComponents)
	{
		OutComponents.Reset();

		FUESpecComponentRegistration SkillSlot;
		SkillSlot.Type = TEXT("SkillSlot");
		SkillSlot.WidgetClass = UUESpecSkillSlotWidget::StaticClass();
		SkillSlot.Accepts = { TEXT("children"), TEXT("actions"), TEXT("navigation"), TEXT("style"), TEXT("stateBindings") };
		SkillSlot.Keys = { TEXT("children"), TEXT("actions"), TEXT("navigation"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(SkillSlot);

		FUESpecComponentRegistration RedDot;
		RedDot.Type = TEXT("RedDot");
		RedDot.WidgetClass = UUESpecRedDotWidget::StaticClass();
		RedDot.Accepts = { TEXT("style"), TEXT("visibility") };
		RedDot.Keys = { TEXT("slot"), TEXT("style"), TEXT("visibility") };
		OutComponents.Add(RedDot);

		FUESpecComponentRegistration HealthBar;
		HealthBar.Type = TEXT("HealthBar");
		HealthBar.WidgetClass = UUESpecHealthBarWidget::StaticClass();
		HealthBar.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		HealthBar.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(HealthBar);

		FUESpecComponentRegistration ResourceBar;
		ResourceBar.Type = TEXT("ResourceBar");
		ResourceBar.WidgetClass = UUESpecResourceBarWidget::StaticClass();
		ResourceBar.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		ResourceBar.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(ResourceBar);

		FUESpecComponentRegistration DamageNumber;
		DamageNumber.Type = TEXT("DamageNumber");
		DamageNumber.WidgetClass = UUESpecDamageNumberWidget::StaticClass();
		DamageNumber.Accepts = { TEXT("style"), TEXT("stateBindings") };
		DamageNumber.Keys = { TEXT("content"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(DamageNumber);

		FUESpecComponentRegistration StatusEffectIcon;
		StatusEffectIcon.Type = TEXT("StatusEffectIcon");
		StatusEffectIcon.WidgetClass = UUESpecStatusEffectIconWidget::StaticClass();
		StatusEffectIcon.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		StatusEffectIcon.Keys = { TEXT("children"), TEXT("content"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(StatusEffectIcon);

		FUESpecComponentRegistration QuestEntry;
		QuestEntry.Type = TEXT("QuestEntry");
		QuestEntry.WidgetClass = UUESpecQuestEntryWidget::StaticClass();
		QuestEntry.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		QuestEntry.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(QuestEntry);

		FUESpecComponentRegistration PlayerNamePlate;
		PlayerNamePlate.Type = TEXT("PlayerNamePlate");
		PlayerNamePlate.WidgetClass = UUESpecPlayerNamePlateWidget::StaticClass();
		PlayerNamePlate.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		PlayerNamePlate.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(PlayerNamePlate);

		FUESpecComponentRegistration ItemSlot;
		ItemSlot.Type = TEXT("ItemSlot");
		ItemSlot.WidgetClass = UUESpecItemSlotWidget::StaticClass();
		ItemSlot.Accepts = { TEXT("children"), TEXT("actions"), TEXT("style"), TEXT("stateBindings") };
		ItemSlot.Keys = { TEXT("children"), TEXT("actions"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(ItemSlot);

		FUESpecComponentRegistration ItemTooltip;
		ItemTooltip.Type = TEXT("ItemTooltip");
		ItemTooltip.WidgetClass = UUESpecItemTooltipWidget::StaticClass();
		ItemTooltip.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		ItemTooltip.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(ItemTooltip);

		FUESpecComponentRegistration EquipmentPanel;
		EquipmentPanel.Type = TEXT("EquipmentPanel");
		EquipmentPanel.WidgetClass = UUESpecEquipmentPanelWidget::StaticClass();
		EquipmentPanel.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		EquipmentPanel.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(EquipmentPanel);

		FUESpecComponentRegistration ChatLine;
		ChatLine.Type = TEXT("ChatLine");
		ChatLine.WidgetClass = UUESpecChatLineWidget::StaticClass();
		ChatLine.Accepts = { TEXT("children"), TEXT("style"), TEXT("stateBindings") };
		ChatLine.Keys = { TEXT("children"), TEXT("slot"), TEXT("style"), TEXT("stateBindings"), TEXT("visibility") };
		OutComponents.Add(ChatLine);
	}

	void GetBuiltinConverters(TArray<FUESpecConverterRegistration>& OutConverters)
	{
		OutConverters.Reset();

		auto AddConverter = [&OutConverters](const TCHAR* Name, const TCHAR* Signature, const TCHAR* OutputType, const TCHAR* Description)
		{
			FUESpecConverterRegistration Entry;
			Entry.Name = Name;
			Entry.Signature = Signature;
			Entry.OutputType = OutputType;
			Entry.Description = Description;
			OutConverters.Add(Entry);
		};

		AddConverter(TEXT("Percent"), TEXT("Percent(Value: float) -> FText"), TEXT("FText"), TEXT("Formats a normalized value as an NN% label."));
		AddConverter(TEXT("LocText"), TEXT("LocText(Key: FString) -> FText"), TEXT("FText"), TEXT("Converts a string localization key into FText."));
		AddConverter(TEXT("RarityColor"), TEXT("RarityColor(Rarity: int32) -> FLinearColor"), TEXT("FLinearColor"), TEXT("Maps rarity tiers to canonical UI colors."));
		AddConverter(TEXT("TimeFormat"), TEXT("TimeFormat(Seconds: float) -> FText"), TEXT("FText"), TEXT("Formats seconds into a cooldown-style M:SS label."));
		AddConverter(TEXT("BoolToVisibility"), TEXT("BoolToVisibility(bValue: bool) -> ESlateVisibility"), TEXT("ESlateVisibility"), TEXT("Maps booleans to Visible or Collapsed visibility."));
		AddConverter(TEXT("Negate"), TEXT("Negate(bValue: bool) -> bool"), TEXT("bool"), TEXT("Negates a boolean input."));
		AddConverter(TEXT("Stringify"), TEXT("Stringify(Value: FString) -> FString"), TEXT("FString"), TEXT("Returns the original string value unchanged."));
		AddConverter(TEXT("SignedDeltaText"), TEXT("SignedDeltaText(Value: float) -> FText"), TEXT("FText"), TEXT("Formats combat deltas like -248 or +520 with a leading sign."));
		AddConverter(TEXT("QuestProgressText"), TEXT("QuestProgressText(Current: int32, Required: int32) -> FText"), TEXT("FText"), TEXT("Formats quest tracker progress as X / Y."));
		AddConverter(TEXT("ItemPowerText"), TEXT("ItemPowerText(Value: int32) -> FText"), TEXT("FText"), TEXT("Formats an item power number like +412."));
		AddConverter(TEXT("ChatChannelPrefix"), TEXT("ChatChannelPrefix(Channel: FString, Speaker: FString) -> FText"), TEXT("FText"), TEXT("Formats a chat speaker label with its channel prefix."));
	}

	void GetBuiltinStyleTokens(TArray<FUESpecStyleTokenRegistration>& OutStyleTokens)
	{
		OutStyleTokens.Reset();

		auto AddToken = [&OutStyleTokens](const TCHAR* Name, const TCHAR* Value)
		{
			FUESpecStyleTokenRegistration Entry;
			Entry.Name = Name;
			Entry.Value = Value;
			OutStyleTokens.Add(Entry);
		};

		AddToken(TEXT("color.ui.border"), TEXT("#FF3A7BFF"));
		AddToken(TEXT("color.cooldownOverlay"), TEXT("#AA000000"));
		AddToken(TEXT("color.white"), TEXT("#FFFFFFFF"));
		AddToken(TEXT("color.ui.hint"), TEXT("#FFB8C3D1"));
		AddToken(TEXT("color.text.primary"), TEXT("#FFF5F7FA"));
		AddToken(TEXT("color.health.fill"), TEXT("#FFE24A4A"));
		AddToken(TEXT("color.health.background"), TEXT("#66110B0B"));
		AddToken(TEXT("color.resource.fill"), TEXT("#FF3A7BFF"));
		AddToken(TEXT("color.resource.background"), TEXT("#66111A33"));
		AddToken(TEXT("color.damage.normal"), TEXT("#FFF8D66B"));
		AddToken(TEXT("color.damage.critical"), TEXT("#FFFF9A4A"));
		AddToken(TEXT("color.status.buff"), TEXT("#FF59D18C"));
		AddToken(TEXT("color.quest.title"), TEXT("#FFF0E4B8"));
		AddToken(TEXT("color.quest.progress"), TEXT("#FFB8C3D1"));
		AddToken(TEXT("color.nameplate.enemy"), TEXT("#FFFFE7C2"));
		AddToken(TEXT("color.item.legendary"), TEXT("#FFFFB347"));
		AddToken(TEXT("color.item.tooltipTitle"), TEXT("#FFFFF1C7"));
		AddToken(TEXT("color.chat.guild"), TEXT("#FF7ED7A8"));
		AddToken(TEXT("color.chat.message"), TEXT("#FFF5F7FA"));
		AddToken(TEXT("size.statusBar.width"), TEXT("320"));
		AddToken(TEXT("size.statusBar.height"), TEXT("22"));
		AddToken(TEXT("size.statusBar.label"), TEXT("14"));
		AddToken(TEXT("size.damage.large"), TEXT("26"));
		AddToken(TEXT("size.nameplate.title"), TEXT("18"));
		AddToken(TEXT("size.status.icon"), TEXT("18"));
		AddToken(TEXT("size.item.slot"), TEXT("48"));
		AddToken(TEXT("size.tooltip.body"), TEXT("14"));
	}

	void GetBuiltinSignals(TArray<FUESpecSignalRegistration>& OutSignals)
	{
		OutSignals.Reset();

		auto AddSignal = [&OutSignals](const TCHAR* Name)
		{
			FUESpecSignalRegistration Entry;
			Entry.Name = FName(Name);
			OutSignals.Add(Entry);
		};

		AddSignal(TEXT("onSkillUsed"));
		AddSignal(TEXT("onHealthChanged"));
		AddSignal(TEXT("onResourceChanged"));
		AddSignal(TEXT("onStatusEffectAdded"));
		AddSignal(TEXT("onQuestTracked"));
		AddSignal(TEXT("onTargetChanged"));
		AddSignal(TEXT("onDamageNumberSpawned"));
		AddSignal(TEXT("onItemEquipped"));
		AddSignal(TEXT("onItemHovered"));
		AddSignal(TEXT("onTooltipShown"));
		AddSignal(TEXT("onChatMessageReceived"));
	}
}
