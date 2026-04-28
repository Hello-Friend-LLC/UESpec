#pragma once

#include "CoreMinimal.h"
#include "Registry/UESpecComponentRegistry.h"

namespace UE::UESpec::ComponentLibrary
{
	UESPECRUNTIME_API void GetBuiltinComponents(TArray<FUESpecComponentRegistration>& OutComponents);
	UESPECRUNTIME_API void GetBuiltinConverters(TArray<FUESpecConverterRegistration>& OutConverters);
	UESPECRUNTIME_API void GetBuiltinStyleTokens(TArray<FUESpecStyleTokenRegistration>& OutStyleTokens);
	UESPECRUNTIME_API void GetBuiltinSignals(TArray<FUESpecSignalRegistration>& OutSignals);
}
