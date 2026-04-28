#pragma once

#include "Commandlets/Commandlet.h"

#include "UESpecDumpSupportSurfaceCommandlet.generated.h"

UCLASS()
class UESPECEDITOR_API UUESpecDumpSupportSurfaceCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUESpecDumpSupportSurfaceCommandlet();

	virtual int32 Main(const FString& Params) override;
};
