#pragma once

#include "Commandlets/Commandlet.h"

#include "UESpecCompileCommandlet.generated.h"

UCLASS()
class UESPECEDITOR_API UUESpecCompileCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUESpecCompileCommandlet();

	virtual int32 Main(const FString& Params) override;
};
