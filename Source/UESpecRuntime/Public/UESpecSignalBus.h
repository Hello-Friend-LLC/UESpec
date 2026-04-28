#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "UESpecSignalBus.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUESpecOnSignal, FName, SignalName);

UCLASS()
class UESPECRUNTIME_API UUESpecSignalBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UESpec|Signals")
	FUESpecOnSignal OnSignal;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Signals")
	void Broadcast(FName SignalName);
};
