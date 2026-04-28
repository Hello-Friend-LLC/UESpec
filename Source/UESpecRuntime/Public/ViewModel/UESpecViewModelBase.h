#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "UESpecViewModelBase.generated.h"

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecViewModelMethodParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Type;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecViewModelMethodDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	bool bPure = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	TArray<FUESpecViewModelMethodParameter> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESpec")
	FString ReturnType;
};

UCLASS(Abstract, Blueprintable)
class UESPECRUNTIME_API UUESpecViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|ViewModel")
	static void DescribeViewModelClass(TSubclassOf<UUESpecViewModelBase> ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods);

	static void DescribeViewModelClass(const UClass* ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods);
	static bool IsUESpecPureFunction(const UFunction* Function);
};
