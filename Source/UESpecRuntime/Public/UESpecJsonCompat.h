#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * FJsonObject key-type compatibility.
 *
 * From UE 5.8, FJsonObject::Values is keyed by UE::FSharedString (UE_JSONOBJECT_LEGACY_STRING_KEYS defaults to 0);
 * earlier engines key it by FString. The macro changes FJsonObject's layout, so it cannot be overridden per module.
 * These helpers give callers FString keys on either engine.
 */
namespace UE::UESpec::Json
{
	/** Copies a FJsonObject key (FString or UE::FSharedString) into an FString. */
	template <typename KeyType>
	FORCEINLINE FString KeyToString(const KeyType& Key)
	{
		return FString(MakeStringView(Key));
	}

	/** Snapshot of an object's fields with FString keys, in the object's iteration order. */
	inline TArray<TPair<FString, TSharedPtr<FJsonValue>>> GetFields(const FJsonObject& Object)
	{
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Fields;
		Fields.Reserve(Object.Values.Num());
		for (const auto& Pair : Object.Values)
		{
			Fields.Emplace(KeyToString(Pair.Key), Pair.Value);
		}
		return Fields;
	}
}
