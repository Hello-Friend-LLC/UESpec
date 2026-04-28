#pragma once

#include "AngelscriptManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UESpecTestHelpers.h"
#include "ViewModel/UESpecAngelscriptBridge.h"

namespace UE::UESpec::Tests
{
	class FScopedAngelscriptTestScript
	{
	public:
		~FScopedAngelscriptTestScript()
		{
			if (!StagedAbsolutePath.IsEmpty())
			{
				IFileManager::Get().Delete(*StagedAbsolutePath, false, true, true);
			}
		}

		bool StageFromTestData(FAutomationTestBase& Test, const FString& TestDataFileName, const FString& RelativeScriptPath)
		{
			const FString SourcePath = GetTestDataPath(TestDataFileName);
			Test.TestTrue(FString::Printf(TEXT("%s exists"), *TestDataFileName), FPaths::FileExists(SourcePath));
			if (!FPaths::FileExists(SourcePath))
			{
				return false;
			}

			StagedRelativePath = RelativeScriptPath.Replace(TEXT("\\"), TEXT("/"));
			StagedAbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Script"), StagedRelativePath));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(StagedAbsolutePath), true);

			const bool bCopied = IFileManager::Get().Copy(*StagedAbsolutePath, *SourcePath, true, true, false, nullptr) == COPY_OK;
			Test.TestTrue(FString::Printf(TEXT("stages %s to project Script root"), *TestDataFileName), bCopied);
			return bCopied;
		}

		bool Reload(FAutomationTestBase& Test, const FString& ExpectedClassName)
		{
			if (StagedAbsolutePath.IsEmpty() || StagedRelativePath.IsEmpty())
			{
				Test.AddError(TEXT("No staged AngelScript file is available to reload."));
				return false;
			}

			if (!FAngelscriptManager::IsInitialized())
			{
				Test.AddError(TEXT("Angelscript manager is not initialized."));
				return false;
			}

			FAngelscriptManager& Manager = FAngelscriptManager::Get();
			Test.TestTrue(TEXT("Initial AngelScript compile succeeded"), Manager.bDidInitialCompileSucceed);
			if (!Manager.bDidInitialCompileSucceed)
			{
				Test.AddError(Manager.FormatDiagnostics());
				return false;
			}

			FAngelscriptManager::FFilenamePair FilePair;
			FilePair.AbsolutePath = StagedAbsolutePath;
			FilePair.RelativePath = StagedRelativePath;
			Manager.FileChangesDetectedForReload.AddUnique(FilePair);
			Manager.LastFileChangeDetectedTime = FPlatformTime::Seconds();
			Manager.CheckForHotReload(ECompileType::FullReload);

			if (ExpectedClassName.IsEmpty())
			{
				return true;
			}

			UClass* ResolvedClass = nullptr;
			for (int32 AttemptIndex = 0; AttemptIndex < 100; ++AttemptIndex)
			{
				ResolvedClass = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(ExpectedClassName);
				if (ResolvedClass)
				{
					break;
				}

				FPlatformProcess::Sleep(0.01f);
			}

			if (!ResolvedClass)
			{
				Test.AddError(FString::Printf(TEXT("Failed to hot reload AngelScript class '%s'. Diagnostics:\n%s"), *ExpectedClassName, *Manager.FormatDiagnostics()));
				return false;
			}

			return true;
		}

		const FString& GetStagedAbsolutePath() const
		{
			return StagedAbsolutePath;
		}

	private:
		FString StagedAbsolutePath;
		FString StagedRelativePath;
	};
}
