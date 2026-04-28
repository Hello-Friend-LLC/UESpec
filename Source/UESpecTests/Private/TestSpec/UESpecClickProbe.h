#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "UESpecClickProbe.generated.h"

UCLASS()
class UUESpecClickProbe : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 ClickCount = 0;

	UFUNCTION()
	void HandleClicked()
	{
		++ClickCount;
	}
};
