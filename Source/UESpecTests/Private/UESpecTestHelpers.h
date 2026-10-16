#pragma once

#include "Compiler/UESpecCompilerSubsystem.h"
#include "Components/UESpecArpgWidgets.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Registry/UESpecComponentRegistry.h"
#include "Samples/UESpecSampleSkillBarViewModel.h"
#include "Samples/UESpecSampleWidgets.h"
#include "Serialization/JsonSerializer.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "Widgets/UESpecWidgetBase.h"

namespace UE::UESpec::Tests
{
	static UUESpecCompilerSubsystem* GetCompilerSubsystem(FAutomationTestBase& Test)
	{
		if (!GEditor)
		{
			Test.AddError(TEXT("GEditor is not available."));
			return nullptr;
		}

		UUESpecCompilerSubsystem* Compiler = GEditor->GetEditorSubsystem<UUESpecCompilerSubsystem>();
		if (!Compiler)
		{
			Test.AddError(TEXT("UESpec compiler subsystem is not available."));
		}

		return Compiler;
	}

	// HF port: the plugin lives under Engine/Plugins/HF_Plugins/UESpec, not Engine/Plugins/UESpec,
	// so resolve its base dir through the plugin manager instead of guessing the folder.
	static FString GetUESpecPluginBaseDir()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UESpec"));
		return Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
	}

	static FString GetTestDataPath(const FString& FileName)
	{
		const FString ProjectCandidate = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UESpec"), TEXT("TestData"), FileName);
		if (FPaths::FileExists(ProjectCandidate))
		{
			return ProjectCandidate;
		}

		const FString PluginDir = GetUESpecPluginBaseDir();
		if (!PluginDir.IsEmpty())
		{
			return FPaths::Combine(PluginDir, TEXT("TestData"), FileName);
		}

		return FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UESpec"), TEXT("TestData"), FileName);
	}

	static FString GetPluginFilePath(const FString& RelativePath)
	{
		const FString ProjectCandidate = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UESpec"), RelativePath);
		if (FPaths::FileExists(ProjectCandidate))
		{
			return ProjectCandidate;
		}

		const FString PluginDir = GetUESpecPluginBaseDir();
		if (!PluginDir.IsEmpty())
		{
			return FPaths::Combine(PluginDir, RelativePath);
		}

		return FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UESpec"), RelativePath);
	}

	static FString GetExamplePath(const FString& RelativePath)
	{
		return GetPluginFilePath(FPaths::Combine(TEXT("Examples"), RelativePath));
	}

	static bool LoadTextFile(const FString& FilePath, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *FilePath);
	}

	static bool ParseJsonObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	class FScopedRegistryOverride
	{
	public:
		FScopedRegistryOverride()
		{
			Registry = GetMutableDefault<UUESpecComponentRegistry>();
			check(Registry);

			SavedComponents = Registry->Components;
			SavedConverters = Registry->Converters;
			SavedStyleTokens = Registry->StyleTokens;
			SavedSignals = Registry->Signals;

			ApplyRound2Defaults();
		}

		~FScopedRegistryOverride()
		{
			Registry->Components = SavedComponents;
			Registry->Converters = SavedConverters;
			Registry->StyleTokens = SavedStyleTokens;
			Registry->Signals = SavedSignals;
			RefreshSupportSurface();
		}

		void SetComponents(const TArray<FUESpecComponentRegistration>& NewComponents)
		{
			Registry->Components = NewComponents;
			RefreshSupportSurface();
		}

		void SetConverters(const TArray<FUESpecConverterRegistration>& NewConverters)
		{
			Registry->Converters = NewConverters;
			RefreshSupportSurface();
		}

		void SetStyleTokens(const TArray<FUESpecStyleTokenRegistration>& NewStyleTokens)
		{
			Registry->StyleTokens = NewStyleTokens;
			RefreshSupportSurface();
		}

		void SetSignals(const TArray<FUESpecSignalRegistration>& NewSignals)
		{
			Registry->Signals = NewSignals;
			RefreshSupportSurface();
		}

	private:
		void ApplyRound2Defaults()
		{
			Registry->Components.Reset();
			Registry->Converters.Reset();
			Registry->StyleTokens.Reset();
			Registry->Signals.Reset();

			RefreshSupportSurface();
		}

		void RefreshSupportSurface() const
		{
			if (GEngine)
			{
				if (UUESpecSupportSurfaceSubsystem* SurfaceSubsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
				{
					SurfaceSubsystem->ResetProjectSurface();
				}
			}
		}

		UUESpecComponentRegistry* Registry = nullptr;
		TArray<FUESpecComponentRegistration> SavedComponents;
		TArray<FUESpecConverterRegistration> SavedConverters;
		TArray<FUESpecStyleTokenRegistration> SavedStyleTokens;
		TArray<FUESpecSignalRegistration> SavedSignals;
	};
}
