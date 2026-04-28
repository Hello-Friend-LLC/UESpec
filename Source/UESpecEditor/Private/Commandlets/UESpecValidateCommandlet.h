#pragma once

#include "Commandlets/Commandlet.h"

#include "UESpecValidateCommandlet.generated.h"

UCLASS()
class UESPECEDITOR_API UUESpecValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUESpecValidateCommandlet();

	virtual int32 Main(const FString& Params) override;
};
