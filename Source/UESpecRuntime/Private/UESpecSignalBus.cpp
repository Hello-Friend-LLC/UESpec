#include "UESpecSignalBus.h"

#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecSignalBus)

DEFINE_LOG_CATEGORY_STATIC(LogUESpecSignalBus, Log, All);

void UUESpecSignalBus::Broadcast(FName SignalName)
{
	if (SignalName.IsNone())
	{
		UE_LOG(LogUESpecSignalBus, Warning, TEXT("Ignoring empty signal broadcast."));
		return;
	}

	OnSignal.Broadcast(SignalName);
}
