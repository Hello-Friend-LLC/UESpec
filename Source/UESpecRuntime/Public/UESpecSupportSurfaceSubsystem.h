#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"

#include "UESpecSupportSurfaceSubsystem.generated.h"

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceWidgetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString ClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> Keys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> Accepts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString AssetPath;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceConverterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Signature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString OutputType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Description;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceKindEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString BaseClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Routing;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceStyleTokenEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Value;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceSignalEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSurfaceActionEntry
{
	GENERATED_BODY()

	static constexpr const TCHAR* StatusSupported = TEXT("supported");
	static constexpr const TCHAR* StatusUnimplemented = TEXT("unimplemented");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	/** "supported" or "unimplemented" (declared in the schema, rejected by validation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Status;

	bool IsSupported() const { return Status == StatusSupported; }
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecSupportSurfaceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString SupportSurfaceVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString GeneratedAtUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceWidgetEntry> BuiltInUMGWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceWidgetEntry> CommonUIWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceWidgetEntry> RegisteredComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceConverterEntry> Converters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceKindEntry> Kinds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceStyleTokenEntry> StyleTokens;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceSignalEntry> Signals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceActionEntry> ActionEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecSurfaceActionEntry> ActionPrimitives;
};

UCLASS()
class UESPECRUNTIME_API UUESpecSupportSurfaceSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "UESpec|SupportSurface")
	void ResetProjectSurface();

	UFUNCTION(BlueprintCallable, Category = "UESpec|SupportSurface")
	void RegisterComponentSurface(const FString& Type, const FString& AssetPath, const FString& ClassPath, const TArray<FString>& Accepts, const TArray<FString>& Keys);

	UFUNCTION(BlueprintCallable, Category = "UESpec|SupportSurface")
	void RegisterConverterSurface(const FString& Name, const FString& Signature, const FString& OutputType, const FString& Description);

	UFUNCTION(BlueprintCallable, Category = "UESpec|SupportSurface")
	void RegisterStyleTokenSurface(const FString& Name, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "UESpec|SupportSurface")
	void RegisterSignalSurface(const FString& Name);

	FUESpecSupportSurfaceSnapshot BuildSnapshot() const;

	/** Action "on" events and actionBlock primitives with their implementation status; validation rejects unimplemented ones. */
	static TArray<FUESpecSurfaceActionEntry> GetActionEventSurface();
	static TArray<FUESpecSurfaceActionEntry> GetActionPrimitiveSurface();

private:
	UPROPERTY(Transient)
	TArray<FUESpecSurfaceWidgetEntry> RegisteredComponentSurfaces;

	UPROPERTY(Transient)
	TArray<FUESpecSurfaceConverterEntry> RegisteredConverterSurfaces;

	UPROPERTY(Transient)
	TArray<FUESpecSurfaceStyleTokenEntry> RegisteredStyleTokenSurfaces;

	UPROPERTY(Transient)
	TArray<FUESpecSurfaceSignalEntry> RegisteredSignalSurfaces;

	void LoadProjectRegistrySettings();
	void RegisterCanonicalConverters();
};
