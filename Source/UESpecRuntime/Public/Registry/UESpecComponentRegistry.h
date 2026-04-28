#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "UESpecComponentRegistry.generated.h"

class UWidget;

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecComponentRegistration
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Type;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	TSoftClassPtr<UWidget> WidgetClass;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	TArray<FString> Accepts;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	TArray<FString> Keys;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecConverterRegistration
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Name;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Signature;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString OutputType;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Description;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecStyleTokenRegistration
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Name;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FString Value;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSignalRegistration
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UESpec")
	FName Name = NAME_None;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "UESpec Component Registry"))
class UESPECRUNTIME_API UUESpecComponentRegistry : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UUESpecComponentRegistry& Get();

	void GetEffectiveComponents(TArray<FUESpecComponentRegistration>& OutComponents) const;
	void GetEffectiveConverters(TArray<FUESpecConverterRegistration>& OutConverters) const;
	void GetEffectiveStyleTokens(TArray<FUESpecStyleTokenRegistration>& OutStyleTokens) const;
	void GetEffectiveSignals(TArray<FUESpecSignalRegistration>& OutSignals) const;

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<FUESpecComponentRegistration> Components;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<FUESpecConverterRegistration> Converters;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<FUESpecStyleTokenRegistration> StyleTokens;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<FUESpecSignalRegistration> Signals;
};
