#include "UESpecConverterLibrary.h"

#include "Internationalization/Text.h"
#include "Components/SlateWrapperTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecConverterLibrary)

FText UUESpecConverterLibrary::Conv_Percent(const float Value)
{
	return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f)));
}

FText UUESpecConverterLibrary::Conv_LocText(const FString& Key)
{
	return FText::FromString(Key);
}

FLinearColor UUESpecConverterLibrary::Conv_RarityColor(const int32 Rarity)
{
	switch (Rarity)
	{
	case 0:
		return FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
	case 1:
		return FLinearColor(0.18f, 0.72f, 0.28f, 1.0f);
	case 2:
		return FLinearColor(0.12f, 0.48f, 0.95f, 1.0f);
	case 3:
		return FLinearColor(0.62f, 0.28f, 0.88f, 1.0f);
	case 4:
		return FLinearColor(0.96f, 0.58f, 0.12f, 1.0f);
	default:
		return FLinearColor(0.90f, 0.82f, 0.30f, 1.0f);
	}
}

FText UUESpecConverterLibrary::Conv_TimeFormat(const float Seconds)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(Seconds));
	const int32 Minutes = TotalSeconds / 60;
	const int32 RemainingSeconds = TotalSeconds % 60;
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, RemainingSeconds));
}

ESlateVisibility UUESpecConverterLibrary::Conv_BoolToVisibility(const bool bValue)
{
	return bValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

bool UUESpecConverterLibrary::Conv_Negate(const bool bValue)
{
	return !bValue;
}

FString UUESpecConverterLibrary::Conv_Stringify(const FString& Value)
{
	return Value;
}
