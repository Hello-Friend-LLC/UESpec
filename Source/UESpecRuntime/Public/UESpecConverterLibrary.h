#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UESpecConverterLibrary.generated.h"

UCLASS()
class UESPECRUNTIME_API UUESpecConverterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "Percent", ToolTip = "Formats a normalized value as an NN% label."))
	static FText Conv_Percent(float Value);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "LocText", ToolTip = "Converts a string localization key into FText."))
	static FText Conv_LocText(const FString& Key);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "RarityColor", ToolTip = "Maps rarity tiers to canonical UI colors."))
	static FLinearColor Conv_RarityColor(int32 Rarity);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "TimeFormat", ToolTip = "Formats seconds into a cooldown-style M:SS label."))
	static FText Conv_TimeFormat(float Seconds);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "BoolToVisibility", ToolTip = "Maps booleans to Visible or Collapsed visibility."))
	static ESlateVisibility Conv_BoolToVisibility(bool bValue);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "Negate", ToolTip = "Negates a boolean input."))
	static bool Conv_Negate(bool bValue);

	UFUNCTION(BlueprintPure, Category = "UESpec|Converter", meta = (DisplayName = "Stringify", ToolTip = "Returns the original string value unchanged."))
	static FString Conv_Stringify(const FString& Value);
};
