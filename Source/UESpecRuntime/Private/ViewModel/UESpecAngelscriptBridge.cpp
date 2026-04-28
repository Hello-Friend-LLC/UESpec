#include "ViewModel/UESpecAngelscriptBridge.h"

#include "AngelscriptManager.h"
#include "ClassGenerator/ASClass.h"
#include "UObject/UObjectIterator.h"

namespace UE::UESpec::Angelscript
{
	namespace
	{
		static void AddLookupCandidate(TArray<FString>& OutCandidates, FString Candidate)
		{
			Candidate.TrimStartAndEndInline();
			if (!Candidate.IsEmpty())
			{
				OutCandidates.AddUnique(MoveTemp(Candidate));
			}
		}

		static void AddNormalizedLookupVariants(TArray<FString>& OutCandidates, const FString& InName)
		{
			FString Candidate = InName.TrimStartAndEnd();
			Candidate.ReplaceInline(TEXT("\\"), TEXT("/"));

			int32 SeparatorIndex = INDEX_NONE;
			if (Candidate.FindLastChar(TEXT('/'), SeparatorIndex))
			{
				Candidate = Candidate.Mid(SeparatorIndex + 1);
			}

			if (Candidate.Contains(TEXT(".")))
			{
				FString Left;
				FString Right;
				if (Candidate.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					Candidate = Right;
				}
			}

			AddLookupCandidate(OutCandidates, Candidate);
			if ((Candidate.StartsWith(TEXT("U")) || Candidate.StartsWith(TEXT("A")))
				&& Candidate.Len() > 1
				&& FChar::IsUpper(Candidate[1]))
			{
				FString StrippedCandidate = Candidate.RightChop(1);
				AddLookupCandidate(OutCandidates, MoveTemp(StrippedCandidate));
			}
		}

		static UASClass* ResolveFirstASClass(const UClass* Class)
		{
			return Class ? UASClass::GetFirstASClass(const_cast<UClass*>(Class)) : nullptr;
		}
	}

	bool IsAngelscriptGeneratedClass(const UClass* Class)
	{
		return ResolveFirstASClass(Class) != nullptr;
	}

	FString GetAngelscriptSourcePath(const UClass* Class)
	{
		if (const UASClass* const ASClass = ResolveFirstASClass(Class))
		{
			return ASClass->GetSourceFilePath();
		}

		return FString();
	}

	UClass* ResolveClassByAngelscriptName(const FString& AsClassName)
	{
		TArray<FString> LookupCandidates;
		AddNormalizedLookupVariants(LookupCandidates, AsClassName);
		if (LookupCandidates.IsEmpty() || !FAngelscriptManager::IsInitialized())
		{
			return nullptr;
		}

		for (const FString& LookupName : LookupCandidates)
		{
			if (const TSharedPtr<FAngelscriptClassDesc> ClassDesc = FAngelscriptManager::Get().GetClass(LookupName))
			{
				if (ClassDesc->Class)
				{
					return ClassDesc->Class;
				}
			}
		}

		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* const Candidate = *ClassIt;
			if (!IsAngelscriptGeneratedClass(Candidate))
			{
				continue;
			}

			for (const FString& LookupName : LookupCandidates)
			{
				if (Candidate->GetName().Equals(LookupName, ESearchCase::CaseSensitive))
				{
					return Candidate;
				}
			}
		}

		return nullptr;
	}
}
