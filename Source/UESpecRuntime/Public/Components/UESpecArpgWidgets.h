#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "UESpecArpgWidgets.generated.h"

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecSkillSlotWidget : public UCanvasPanel
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UESpec|ARPG")
	FOnButtonClickedEvent OnClicked;

	UPROPERTY(BlueprintAssignable, Category = "UESpec|ARPG")
	FOnButtonHoverEvent OnHovered;

	UPROPERTY(BlueprintAssignable, Category = "UESpec|ARPG")
	FOnButtonHoverEvent OnUnhovered;
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecRedDotWidget : public UImage
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecHealthBarWidget : public UOverlay
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecResourceBarWidget : public UOverlay
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecDamageNumberWidget : public UTextBlock
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecStatusEffectIconWidget : public UOverlay
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecQuestEntryWidget : public UHorizontalBox
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecPlayerNamePlateWidget : public UVerticalBox
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecItemSlotWidget : public UOverlay
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecItemTooltipWidget : public UVerticalBox
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecEquipmentPanelWidget : public UVerticalBox
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UESPECRUNTIME_API UUESpecChatLineWidget : public UHorizontalBox
{
	GENERATED_BODY()
};
