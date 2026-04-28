#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
class FJsonObject;
struct FUESpecCompilerResult;

class FUESpecBindingBuilder
{
public:
	static bool Build(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
};
