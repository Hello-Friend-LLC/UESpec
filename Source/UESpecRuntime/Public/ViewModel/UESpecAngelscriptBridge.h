#pragma once

#include "CoreMinimal.h"

class UClass;

namespace UE::UESpec::Angelscript
{
	UESPECRUNTIME_API bool IsAngelscriptGeneratedClass(const UClass* Class);
	UESPECRUNTIME_API FString GetAngelscriptSourcePath(const UClass* Class);
	UESPECRUNTIME_API UClass* ResolveClassByAngelscriptName(const FString& AsClassName);
}
