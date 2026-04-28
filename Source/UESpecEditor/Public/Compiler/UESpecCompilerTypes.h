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
};
