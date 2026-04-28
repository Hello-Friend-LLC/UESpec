#pragma once

#include "Commandlets/Commandlet.h"

#include "UESpecRunTestsCommandlet.generated.h"

UCLASS()
class UESPECEDITOR_API UUESpecRunTestsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUESpecRunTestsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
