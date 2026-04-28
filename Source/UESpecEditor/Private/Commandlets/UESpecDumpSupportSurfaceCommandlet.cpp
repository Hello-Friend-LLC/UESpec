#include "Commandlets/UESpecDumpSupportSurfaceCommandlet.h"

#include "SupportSurface/UESpecSupportSurfaceDumper.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecDumpSupportSurfaceCommandlet)

UUESpecDumpSupportSurfaceCommandlet::UUESpecDumpSupportSurfaceCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UUESpecDumpSupportSurfaceCommandlet::Main(const FString& Params)
{
	EUESpecSupportSurfaceDumpFormat Format;
	FString OutputPath;
	FString Error;
	if (!FUESpecSupportSurfaceDumper::ParseCommandletParams(Params, Format, OutputPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 1;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Error, TEXT("GEngine is not available."));
		return 2;
	}

	UUESpecSupportSurfaceSubsystem* Subsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UESpec support surface subsystem is not available."));
		return 3;
	}

	FString Contents;
	if (!FUESpecSupportSurfaceDumper::Dump(*Subsystem, Format, Contents, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 4;
	}

	if (!OutputPath.IsEmpty())
	{
		if (!FUESpecSupportSurfaceDumper::WriteOutput(OutputPath, Contents, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
			return 5;
		}

		UE_LOG(LogTemp, Display, TEXT("Wrote support surface to %s"), *FPaths::ConvertRelativePathToFull(OutputPath));
		return 0;
	}

	GLog->Log(Contents);
	return 0;
}
