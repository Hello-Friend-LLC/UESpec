#pragma once

#include "CoreMinimal.h"

class UUESpecSupportSurfaceSubsystem;

enum class EUESpecSupportSurfaceDumpFormat : uint8
{
	Markdown,
	Json
};

class UESPECEDITOR_API FUESpecSupportSurfaceDumper
{
public:
	static bool TryParseFormat(const FString& Value, EUESpecSupportSurfaceDumpFormat& OutFormat);
	static bool ParseConsoleArguments(const TArray<FString>& Args, EUESpecSupportSurfaceDumpFormat& OutFormat, FString& OutOutputPath, FString& OutError);
	static bool ParseCommandletParams(const FString& Params, EUESpecSupportSurfaceDumpFormat& OutFormat, FString& OutOutputPath, FString& OutError);
	static bool Dump(UUESpecSupportSurfaceSubsystem& Subsystem, EUESpecSupportSurfaceDumpFormat Format, FString& OutContents, FString& OutError);
	static bool WriteOutput(const FString& OutputPath, const FString& Contents, FString& OutError);
};
