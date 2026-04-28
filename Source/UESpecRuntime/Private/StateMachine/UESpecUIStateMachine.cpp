#include "StateMachine/UESpecUIStateMachine.h"

#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecUIStateMachine)

DEFINE_LOG_CATEGORY_STATIC(LogUESpecUIStateMachine, Log, All);

const FUESpecStateDefinition* FUESpecStateMachineDefinition::FindStateDefinition(FName StateName) const
{
	return States.FindByPredicate([StateName](const FUESpecStateDefinition& StateDefinition)
	{
		return StateDefinition.Name == StateName;
	});
}

bool FUESpecStateMachineDefinition::HasStateDefinition(FName StateName) const
{
	return FindStateDefinition(StateName) != nullptr;
}

void FUESpecStateMachineDefinition::FindBindingsForState(FName StateName, TArray<const FUESpecWidgetStateBinding*>& OutBindings) const
{
	OutBindings.Reset();
	for (const FUESpecWidgetStateBinding& Binding : StateBindings)
	{
		if (Binding.State == StateName)
		{
			OutBindings.Add(&Binding);
		}
	}
}

FName UE::UESpec::StateMachine::ResolveInitialState(const FUESpecStateMachineDefinition& Definition)
{
	if (!Definition.InitialState.IsNone())
	{
		return Definition.InitialState;
	}

	const FUESpecStateDefinition* const FirstNamedState = Definition.States.FindByPredicate([](const FUESpecStateDefinition& StateDefinition)
	{
		return !StateDefinition.Name.IsNone();
	});

	return FirstNamedState ? FirstNamedState->Name : NAME_None;
}

UE::UESpec::StateMachine::FStateChangeResult UE::UESpec::StateMachine::PrepareStateChange(const FUESpecStateMachineDefinition& Definition, FName CurrentState, FName RequestedState)
{
	FStateChangeResult Result;
	Result.PreviousState = CurrentState;

	const FName TargetState = RequestedState.IsNone() ? ResolveInitialState(Definition) : RequestedState;
	if (TargetState.IsNone())
	{
		return Result;
	}

	if (Definition.States.Num() > 0 && !Definition.HasStateDefinition(TargetState))
	{
		UE_LOG(LogUESpecUIStateMachine, Warning, TEXT("Ignoring undefined UI state '%s'."), *TargetState.ToString());
		return Result;
	}

	if (CurrentState == TargetState)
	{
		return Result;
	}

	Result.NextState = TargetState;
	Result.bShouldApply = true;
	return Result;
}
