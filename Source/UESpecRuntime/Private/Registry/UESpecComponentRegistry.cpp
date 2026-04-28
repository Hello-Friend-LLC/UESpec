#include "Registry/UESpecComponentRegistry.h"

#include "Registry/UESpecBuiltinComponentLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecComponentRegistry)

namespace
{
	template <typename EntryType, typename KeyType, typename KeyFunc>
	void MergeOverrideEntries(const TArray<EntryType>& Overrides, TArray<EntryType>& InOutEntries, KeyFunc&& GetKey)
	{
		for (const EntryType& OverrideEntry : Overrides)
		{
			const KeyType OverrideKey = GetKey(OverrideEntry);
			const int32 ExistingIndex = InOutEntries.IndexOfByPredicate(
				[&GetKey, &OverrideKey](const EntryType& ExistingEntry)
				{
					return GetKey(ExistingEntry) == OverrideKey;
				});

			if (ExistingIndex != INDEX_NONE)
			{
				InOutEntries[ExistingIndex] = OverrideEntry;
			}
			else
			{
				InOutEntries.Add(OverrideEntry);
			}
		}
	}
}

const UUESpecComponentRegistry& UUESpecComponentRegistry::Get()
{
	return *GetDefault<UUESpecComponentRegistry>();
}

void UUESpecComponentRegistry::GetEffectiveComponents(TArray<FUESpecComponentRegistration>& OutComponents) const
{
	UE::UESpec::ComponentLibrary::GetBuiltinComponents(OutComponents);
	MergeOverrideEntries<FUESpecComponentRegistration, FString>(Components, OutComponents, [](const FUESpecComponentRegistration& Entry)
	{
		return Entry.Type;
	});
}

void UUESpecComponentRegistry::GetEffectiveConverters(TArray<FUESpecConverterRegistration>& OutConverters) const
{
	UE::UESpec::ComponentLibrary::GetBuiltinConverters(OutConverters);
	MergeOverrideEntries<FUESpecConverterRegistration, FString>(Converters, OutConverters, [](const FUESpecConverterRegistration& Entry)
	{
		return Entry.Name;
	});
}

void UUESpecComponentRegistry::GetEffectiveStyleTokens(TArray<FUESpecStyleTokenRegistration>& OutStyleTokens) const
{
	UE::UESpec::ComponentLibrary::GetBuiltinStyleTokens(OutStyleTokens);
	MergeOverrideEntries<FUESpecStyleTokenRegistration, FString>(StyleTokens, OutStyleTokens, [](const FUESpecStyleTokenRegistration& Entry)
	{
		return Entry.Name;
	});
}

void UUESpecComponentRegistry::GetEffectiveSignals(TArray<FUESpecSignalRegistration>& OutSignals) const
{
	UE::UESpec::ComponentLibrary::GetBuiltinSignals(OutSignals);
	MergeOverrideEntries<FUESpecSignalRegistration, FName>(Signals, OutSignals, [](const FUESpecSignalRegistration& Entry)
	{
		return Entry.Name;
	});
}

FName UUESpecComponentRegistry::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UUESpecComponentRegistry::GetSectionName() const
{
	return TEXT("UESpec");
}
