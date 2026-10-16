#include "SupportSurface/UESpecSupportSurfaceDumper.h"

#include "UESpecSupportSurfaceSubsystem.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "JsonObjectConverter.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"

namespace UE::UESpec::Private
{
	static FString SerializeSnapshotToJson(const FUESpecSupportSurfaceSnapshot& Snapshot, bool bPrettyPrint)
	{
		FString JsonString;
		FJsonObjectConverter::UStructToJsonObjectString(Snapshot, JsonString, 0, 0, 0, nullptr, bPrettyPrint);
		return JsonString;
	}

	static FString BuildWidgetLine(const FUESpecSurfaceWidgetEntry& Entry)
	{
		TArray<FString> Details;
		if (!Entry.AssetPath.IsEmpty())
		{
			Details.Add(FString::Printf(TEXT("registered at %s"), *Entry.AssetPath));
		}

		if (!Entry.ClassPath.IsEmpty())
		{
			Details.Add(FString::Printf(TEXT("class: %s"), *Entry.ClassPath));
		}

		if (Entry.Accepts.Num() > 0)
		{
			Details.Add(FString::Printf(TEXT("accepts: %s"), *FString::Join(Entry.Accepts, TEXT(", "))));
		}

		if (Entry.Keys.Num() > 0)
		{
			Details.Add(FString::Printf(TEXT("keys: %s"), *FString::Join(Entry.Keys, TEXT(", "))));
		}

		if (Details.Num() == 0)
		{
			return FString::Printf(TEXT("- %s"), *Entry.Type);
		}

		return FString::Printf(TEXT("- %s (%s)"), *Entry.Type, *FString::Join(Details, TEXT("; ")));
	}

	static void AppendWidgetSection(FString& OutMarkdown, const FString& Heading, const TArray<FUESpecSurfaceWidgetEntry>& Entries)
	{
		OutMarkdown += Heading + LINE_TERMINATOR;
		if (Entries.Num() == 0)
		{
			OutMarkdown += TEXT("- (none registered)") LINE_TERMINATOR LINE_TERMINATOR;
			return;
		}

		for (const FUESpecSurfaceWidgetEntry& Entry : Entries)
		{
			OutMarkdown += BuildWidgetLine(Entry) + LINE_TERMINATOR;
		}

		OutMarkdown += LINE_TERMINATOR;
	}

	static void AppendConverterSection(FString& OutMarkdown, const TArray<FUESpecSurfaceConverterEntry>& Entries)
	{
		OutMarkdown += TEXT("## Converters") LINE_TERMINATOR;
		if (Entries.Num() == 0)
		{
			OutMarkdown += TEXT("- (none registered)") LINE_TERMINATOR LINE_TERMINATOR;
			return;
		}

		for (const FUESpecSurfaceConverterEntry& Entry : Entries)
		{
			TArray<FString> Details;
			if (!Entry.Signature.IsEmpty())
			{
				Details.Add(Entry.Signature);
			}
			if (!Entry.OutputType.IsEmpty())
			{
				Details.Add(FString::Printf(TEXT("-> %s"), *Entry.OutputType));
			}
			if (!Entry.Description.IsEmpty())
			{
				Details.Add(Entry.Description);
			}

			OutMarkdown += FString::Printf(TEXT("- %s%s"), *Entry.Name, Details.Num() > 0 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Details, TEXT("; "))) : TEXT("")) + LINE_TERMINATOR;
		}

		OutMarkdown += LINE_TERMINATOR;
	}

	static void AppendKindsSection(FString& OutMarkdown, const TArray<FUESpecSurfaceKindEntry>& Entries)
	{
		OutMarkdown += TEXT("## Kinds & Base Classes") LINE_TERMINATOR;
		for (const FUESpecSurfaceKindEntry& Entry : Entries)
		{
			OutMarkdown += FString::Printf(TEXT("- %s -> %s (%s)"), *Entry.Kind, *Entry.BaseClass, *Entry.Routing) + LINE_TERMINATOR;
		}
		OutMarkdown += LINE_TERMINATOR;
	}

	static void AppendStyleTokensSection(FString& OutMarkdown, const TArray<FUESpecSurfaceStyleTokenEntry>& Entries)
	{
		OutMarkdown += TEXT("## Style Tokens (project-level)") LINE_TERMINATOR;
		if (Entries.Num() == 0)
		{
			OutMarkdown += TEXT("- (none registered)") LINE_TERMINATOR LINE_TERMINATOR;
			return;
		}

		for (const FUESpecSurfaceStyleTokenEntry& Entry : Entries)
		{
			OutMarkdown += FString::Printf(TEXT("- %s = %s"), *Entry.Name, *Entry.Value) + LINE_TERMINATOR;
		}
		OutMarkdown += LINE_TERMINATOR;
	}

	static void AppendSignalsSection(FString& OutMarkdown, const TArray<FUESpecSurfaceSignalEntry>& Entries)
	{
		OutMarkdown += TEXT("## Signals") LINE_TERMINATOR;
		if (Entries.Num() == 0)
		{
			OutMarkdown += TEXT("- (none registered)") LINE_TERMINATOR LINE_TERMINATOR;
			return;
		}

		for (const FUESpecSurfaceSignalEntry& Entry : Entries)
		{
			OutMarkdown += FString::Printf(TEXT("- %s"), *Entry.Name) + LINE_TERMINATOR;
		}
		OutMarkdown += LINE_TERMINATOR;
	}

	static void AppendActionsSection(FString& OutMarkdown, const FUESpecSupportSurfaceSnapshot& Snapshot)
	{
		OutMarkdown += TEXT("## Actions") LINE_TERMINATOR;
		OutMarkdown += TEXT("### Events") LINE_TERMINATOR;
		for (const FUESpecSurfaceActionEntry& Entry : Snapshot.ActionEvents)
		{
			OutMarkdown += FString::Printf(TEXT("- %s%s"), *Entry.Name, Entry.IsSupported() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Entry.Status)) + LINE_TERMINATOR;
		}
		OutMarkdown += LINE_TERMINATOR;

		OutMarkdown += TEXT("### Primitives") LINE_TERMINATOR;
		for (const FUESpecSurfaceActionEntry& Entry : Snapshot.ActionPrimitives)
		{
			OutMarkdown += FString::Printf(TEXT("- %s%s"), *Entry.Name, Entry.IsSupported() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Entry.Status)) + LINE_TERMINATOR;
		}
		OutMarkdown += LINE_TERMINATOR;
	}

	static FString BuildMarkdown(const FUESpecSupportSurfaceSnapshot& Snapshot)
	{
		FString Markdown;
		Markdown += TEXT("# UESpec Support Surface") LINE_TERMINATOR LINE_TERMINATOR;
		Markdown += FString::Printf(TEXT("- supportSurfaceVersion: `%s`"), *Snapshot.SupportSurfaceVersion) + LINE_TERMINATOR;
		Markdown += FString::Printf(TEXT("- generatedAt: `%s`"), *Snapshot.GeneratedAtUtc) + LINE_TERMINATOR LINE_TERMINATOR;

		Markdown += TEXT("## Widgets") LINE_TERMINATOR;
		AppendWidgetSection(Markdown, TEXT("### Built-in UMG"), Snapshot.BuiltInUMGWidgets);
		AppendWidgetSection(Markdown, TEXT("### CommonUI"), Snapshot.CommonUIWidgets);
		AppendWidgetSection(Markdown, TEXT("### Registered Components"), Snapshot.RegisteredComponents);
		AppendConverterSection(Markdown, Snapshot.Converters);
		AppendKindsSection(Markdown, Snapshot.Kinds);
		AppendStyleTokensSection(Markdown, Snapshot.StyleTokens);
		AppendSignalsSection(Markdown, Snapshot.Signals);
		AppendActionsSection(Markdown, Snapshot);
		return Markdown;
	}
}

bool FUESpecSupportSurfaceDumper::TryParseFormat(const FString& Value, EUESpecSupportSurfaceDumpFormat& OutFormat)
{
	if (Value.Equals(TEXT("markdown"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("md"), ESearchCase::IgnoreCase))
	{
		OutFormat = EUESpecSupportSurfaceDumpFormat::Markdown;
		return true;
	}

	if (Value.Equals(TEXT("json"), ESearchCase::IgnoreCase))
	{
		OutFormat = EUESpecSupportSurfaceDumpFormat::Json;
		return true;
	}

	return false;
}

bool FUESpecSupportSurfaceDumper::ParseConsoleArguments(const TArray<FString>& Args, EUESpecSupportSurfaceDumpFormat& OutFormat, FString& OutOutputPath, FString& OutError)
{
	OutFormat = EUESpecSupportSurfaceDumpFormat::Markdown;
	OutOutputPath.Reset();

	for (const FString& RawArg : Args)
	{
		FString Arg = RawArg;
		Arg.RemoveFromStart(TEXT("--"));
		Arg.RemoveFromStart(TEXT("-"));

		FString Key;
		FString Value;
		if (Arg.Split(TEXT("="), &Key, &Value))
		{
			if (Key.Equals(TEXT("Format"), ESearchCase::IgnoreCase))
			{
				if (!TryParseFormat(Value, OutFormat))
				{
					OutError = FString::Printf(TEXT("Unsupported format '%s'. Expected markdown or json."), *Value);
					return false;
				}
			}
			else if (Key.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
			{
				OutOutputPath = Value;
				OutOutputPath.TrimQuotesInline();
			}
			continue;
		}

		if (!TryParseFormat(Arg, OutFormat))
		{
			OutError = FString::Printf(TEXT("Unrecognized argument '%s'. Use Format=<markdown|json> or Output=<path>."), *RawArg);
			return false;
		}
	}

	return true;
}

bool FUESpecSupportSurfaceDumper::ParseCommandletParams(const FString& Params, EUESpecSupportSurfaceDumpFormat& OutFormat, FString& OutOutputPath, FString& OutError)
{
	OutFormat = EUESpecSupportSurfaceDumpFormat::Markdown;
	OutOutputPath.Reset();

	FString FormatValue;
	if (FParse::Value(*Params, TEXT("Format="), FormatValue) || FParse::Value(*Params, TEXT("format="), FormatValue))
	{
		if (!TryParseFormat(FormatValue, OutFormat))
		{
			OutError = FString::Printf(TEXT("Unsupported format '%s'. Expected markdown or json."), *FormatValue);
			return false;
		}
	}

	FParse::Value(*Params, TEXT("Output="), OutOutputPath);
	if (OutOutputPath.IsEmpty())
	{
		FParse::Value(*Params, TEXT("output="), OutOutputPath);
	}
	OutOutputPath.TrimQuotesInline();
	return true;
}

bool FUESpecSupportSurfaceDumper::Dump(UUESpecSupportSurfaceSubsystem& Subsystem, EUESpecSupportSurfaceDumpFormat Format, FString& OutContents, FString& OutError)
{
	FUESpecSupportSurfaceSnapshot Snapshot = Subsystem.BuildSnapshot();
	Snapshot.GeneratedAtUtc = FDateTime::UtcNow().ToIso8601();

	FUESpecSupportSurfaceSnapshot HashSnapshot = Snapshot;
	HashSnapshot.GeneratedAtUtc.Reset();
	HashSnapshot.SupportSurfaceVersion.Reset();

	const FString HashJson = UE::UESpec::Private::SerializeSnapshotToJson(HashSnapshot, false);
	if (HashJson.IsEmpty())
	{
		OutError = TEXT("Failed to serialize support surface for version hashing.");
		return false;
	}

	Snapshot.SupportSurfaceVersion = FMD5::HashAnsiString(*HashJson);

	switch (Format)
	{
	case EUESpecSupportSurfaceDumpFormat::Json:
		OutContents = UE::UESpec::Private::SerializeSnapshotToJson(Snapshot, true);
		break;

	case EUESpecSupportSurfaceDumpFormat::Markdown:
		OutContents = UE::UESpec::Private::BuildMarkdown(Snapshot);
		break;

	default:
		OutError = TEXT("Unsupported dump format.");
		return false;
	}

	if (OutContents.IsEmpty())
	{
		OutError = TEXT("Support surface dump produced empty output.");
		return false;
	}

	return true;
}

bool FUESpecSupportSurfaceDumper::WriteOutput(const FString& OutputPath, const FString& Contents, FString& OutError)
{
	if (OutputPath.IsEmpty())
	{
		OutError = TEXT("Output path is empty.");
		return false;
	}

	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(OutputPath);
	const FString DirectoryPath = FPaths::GetPath(AbsolutePath);
	if (!DirectoryPath.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*DirectoryPath, true);
	}

	if (!FFileHelper::SaveStringToFile(Contents, *AbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write support surface to '%s'."), *AbsolutePath);
		return false;
	}

	return true;
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GUESpecDumpSupportSurfaceCommand(
	TEXT("UESpec.DumpSupportSurface"),
	TEXT("Usage: UESpec.DumpSupportSurface Format=<markdown|json> Output=<optional path>"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			EUESpecSupportSurfaceDumpFormat Format;
			FString OutputPath;
			FString Error;
			if (!FUESpecSupportSurfaceDumper::ParseConsoleArguments(Args, Format, OutputPath, Error))
			{
				Ar.Logf(ELogVerbosity::Error, TEXT("%s"), *Error);
				return;
			}

			if (!GEngine)
			{
				Ar.Logf(ELogVerbosity::Error, TEXT("GEngine is not available."));
				return;
			}

			UUESpecSupportSurfaceSubsystem* Subsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>();
			if (!Subsystem)
			{
				Ar.Logf(ELogVerbosity::Error, TEXT("UESpec support surface subsystem is not available."));
				return;
			}

			FString Contents;
			if (!FUESpecSupportSurfaceDumper::Dump(*Subsystem, Format, Contents, Error))
			{
				Ar.Logf(ELogVerbosity::Error, TEXT("%s"), *Error);
				return;
			}

			if (!OutputPath.IsEmpty())
			{
				if (!FUESpecSupportSurfaceDumper::WriteOutput(OutputPath, Contents, Error))
				{
					Ar.Logf(ELogVerbosity::Error, TEXT("%s"), *Error);
					return;
				}

				Ar.Logf(TEXT("Wrote support surface to %s"), *FPaths::ConvertRelativePathToFull(OutputPath));
				return;
			}

			Ar.Log(Contents);
		}));
