#pragma once

#include "CoreMinimal.h"

#include "UESpecUIStateMachine.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUESpecOnUIStateChanged, FName, PreviousState, FName, NewState);

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecStateTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName To = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FString When;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecStateBindingProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName Property = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FString Value;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecWidgetStateBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName WidgetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName State = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	TArray<FUESpecStateBindingProperty> Properties;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecStateDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FString OnEnter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FString OnExit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	TArray<FUESpecStateTransition> Transitions;
};

USTRUCT(BlueprintType)
struct UESPECRUNTIME_API FUESpecStateMachineDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	FName InitialState = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	TArray<FUESpecStateDefinition> States;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UESpec|StateMachine")
	TArray<FUESpecWidgetStateBinding> StateBindings;

	const FUESpecStateDefinition* FindStateDefinition(FName StateName) const;
	bool HasStateDefinition(FName StateName) const;
	void FindBindingsForState(FName StateName, TArray<const FUESpecWidgetStateBinding*>& OutBindings) const;
};

namespace UE::UESpec::StateMachine
{
	struct UESPECRUNTIME_API FStateChangeResult
	{
		FName PreviousState = NAME_None;
		FName NextState = NAME_None;
		bool bShouldApply = false;
	};

	UESPECRUNTIME_API FName ResolveInitialState(const FUESpecStateMachineDefinition& Definition);
	UESPECRUNTIME_API FStateChangeResult PrepareStateChange(const FUESpecStateMachineDefinition& Definition, FName CurrentState, FName RequestedState);
}
