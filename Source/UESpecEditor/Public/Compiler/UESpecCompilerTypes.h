#pragma once

#include "CoreMinimal.h"

#include "UESpecCompilerTypes.generated.h"

USTRUCT(BlueprintType)
struct UESPECEDITOR_API FUESpecCompilerError
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Path;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Hint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> NearestDeclared;
};

USTRUCT(BlueprintType)
struct UESPECEDITOR_API FUESpecCompilerResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	bool bOk = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString NormalizedJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecCompilerError> Errors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecCompilerError> Infos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FString> GeneratedAssets;

	/** Object path of the generated WidgetBlueprint (<OutputDirectory>/WBP_<name>.WBP_<name>). Set on a successful compile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString AssetPath;

	/** UBlueprint::BlueprintGuid of the generated asset; stable across in-place updates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString BlueprintGuid;

	/** "created" when a new asset was written, "updated" when an existing compiler-owned asset was rebuilt in place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString AssetAction;

	/** SHA-256 (lowercase hex) of NormalizedJson; also stored on the asset as the UESpecSpecHash registry tag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString SpecHash;

	/** Absolute filename of the saved .uasset, so callers can open it in source control (the compiler does no SCC checkout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString SavedPackageFile;
};
