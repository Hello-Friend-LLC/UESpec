#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWidgetBlueprint;

struct FUESpecCompilerResult;

class FUESpecEventGraphBuilder
{
public:
	static bool Build(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
};
