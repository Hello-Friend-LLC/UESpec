#include "Compiler/UESpecCompilerSubsystem.h"

#include "Algo/LevenshteinDistance.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Compiler/UESpecBindingBuilder.h"
#include "Compiler/UESpecEventGraphBuilder.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UESpecJsonCompat.h"
#include "WidgetBlueprintFactory.h"
#include "Animation/WidgetAnimation.h"
#include "Compiler/UESpecSha256.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "PackageTools.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectHash.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Regex.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Registry/UESpecComponentRegistry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UESpecSupportSurfaceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "ViewModel/UESpecAngelscriptBridge.h"
#include "WidgetBlueprint.h"
#include "Widgets/UESpecWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecCompilerSubsystem)

namespace UE::UESpec::Editor::Private
{
	static constexpr TCHAR SupportedApiVersion[] = TEXT("0.2");
	static constexpr TCHAR DefaultOutputDirectory[] = TEXT("/Game/UESpec/Generated");
	static constexpr TCHAR DefaultViewModelContext[] = TEXT("ViewModel");

	static const TMap<FString, FString> UECasingMappings =
	{
		{ TEXT("hAlign"), TEXT("HorizontalAlignment") },
		{ TEXT("vAlign"), TEXT("VerticalAlignment") }
	};

	static const TSet<FString> ActionPrimitiveKeys =
	{
		TEXT("openLayer"),
		TEXT("closeLayer"),
		TEXT("setState"),
		TEXT("callVM"),
		TEXT("playAnim"),
		TEXT("emitSignal")
	};

	static const TArray<FString> TestVerbKeys =
	{
		TEXT("click"),
		TEXT("emitSignal"),
		TEXT("hover"),
		TEXT("focus"),
		TEXT("navigateKey"),
		TEXT("setVMField"),
		TEXT("waitForState"),
		TEXT("waitForVisible"),
		TEXT("assertVisible"),
		TEXT("assertText"),
		TEXT("assertStyle"),
		TEXT("assertState"),
		TEXT("assertFocus"),
		TEXT("screenshot")
	};

	static const TSet<FString> ProtocolObjectParents =
	{
		TEXT("content"),
		TEXT("visibility"),
		TEXT("navigation"),
		TEXT("assertVisible"),
		TEXT("assertText"),
		TEXT("assertStyle"),
		TEXT("assertState"),
		TEXT("assertFocus"),
		TEXT("screenshot"),
		TEXT("waitForState"),
		TEXT("waitForVisible"),
		TEXT("emitSignal"),
		TEXT("focus"),
		TEXT("hover"),
		TEXT("click"),
		TEXT("navigateKey"),
		TEXT("setVMField")
	};

	static bool IsProtocolObjectParent(const FString& ParentKey)
	{
		for (const FString& Candidate : ProtocolObjectParents)
		{
			if (ParentKey.Equals(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static const TArray<FString> FallbackKindNames =
	{
		TEXT("HUD"),
		TEXT("Menu"),
		TEXT("Popup"),
		TEXT("Modal"),
		TEXT("Debug"),
		TEXT("WorldSpace"),
		TEXT("Component"),
		TEXT("Test")
	};

	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return JsonString;
	}

	static FString SerializeJsonValue(const TSharedPtr<FJsonValue>& JsonValue)
	{
		if (!JsonValue.IsValid())
		{
			return FString();
		}

		switch (JsonValue->Type)
		{
		case EJson::String:
			return JsonValue->AsString();
		case EJson::Number:
			return FString::SanitizeFloat(JsonValue->AsNumber());
		case EJson::Boolean:
			return JsonValue->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Null:
			return TEXT("null");
		case EJson::Array:
		case EJson::Object:
			{
				FString JsonText;
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
				FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);
				return JsonText;
			}
		default:
			return FString();
		}
	}

	static FString MakeChildPath(const FString& ParentPath, const FString& Key)
	{
		return ParentPath.IsEmpty() ? Key : ParentPath + TEXT(".") + Key;
	}

	static FString MakeArrayPath(const FString& ParentPath, const int32 Index)
	{
		return FString::Printf(TEXT("%s[%d]"), *ParentPath, Index);
	}

	static bool TryGetObjectField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedPtr<FJsonObject>* ObjectField = nullptr;
		if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(FieldName, ObjectField) || !ObjectField || !(*ObjectField).IsValid())
		{
			return false;
		}

		OutObject = *ObjectField;
		return true;
	}

	static bool TryGetArrayField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
	{
		OutArray = nullptr;
		return JsonObject.IsValid() && JsonObject->TryGetArrayField(FieldName, OutArray) && OutArray != nullptr;
	}

	static bool ParseNumberArray(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutValues)
	{
		OutValues.Reset();

		const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
		if (!TryGetArrayField(JsonObject, FieldName, JsonArray))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
		{
			if (!Value.IsValid())
			{
				return false;
			}

			if (Value->Type == EJson::Number)
			{
				OutValues.Add(static_cast<float>(Value->AsNumber()));
				continue;
			}

			if (Value->Type == EJson::String)
			{
				float ParsedValue = 0.0f;
				if (LexTryParseString(ParsedValue, *Value->AsString()))
				{
					OutValues.Add(ParsedValue);
					continue;
				}
			}

			return false;
		}

		return true;
	}

	static bool ParseMargin(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FMargin& OutMargin)
	{
		TArray<float> Values;
		if (!ParseNumberArray(JsonObject, FieldName, Values))
		{
			return false;
		}

		if (Values.Num() == 1)
		{
			OutMargin = FMargin(Values[0]);
			return true;
		}

		if (Values.Num() == 2)
		{
			OutMargin = FMargin(Values[0], Values[1], Values[0], Values[1]);
			return true;
		}

		if (Values.Num() == 4)
		{
			OutMargin = FMargin(Values[0], Values[1], Values[2], Values[3]);
			return true;
		}

		return false;
	}

	static bool ParseVector2(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector2D& OutVector)
	{
		TArray<float> Values;
		if (!ParseNumberArray(JsonObject, FieldName, Values) || Values.Num() != 2)
		{
			return false;
		}

		OutVector = FVector2D(Values[0], Values[1]);
		return true;
	}

	static bool ParseAnchors(const FString& AnchorsName, FAnchors& OutAnchors)
	{
		if (AnchorsName == TEXT("TopLeft"))
		{
			OutAnchors = FAnchors(0.0f, 0.0f, 0.0f, 0.0f);
			return true;
		}
		if (AnchorsName == TEXT("TopCenter"))
		{
			OutAnchors = FAnchors(0.5f, 0.0f, 0.5f, 0.0f);
			return true;
		}
		if (AnchorsName == TEXT("TopRight"))
		{
			OutAnchors = FAnchors(1.0f, 0.0f, 1.0f, 0.0f);
			return true;
		}
		if (AnchorsName == TEXT("CenterLeft"))
		{
			OutAnchors = FAnchors(0.0f, 0.5f, 0.0f, 0.5f);
			return true;
		}
		if (AnchorsName == TEXT("Center"))
		{
			OutAnchors = FAnchors(0.5f, 0.5f, 0.5f, 0.5f);
			return true;
		}
		if (AnchorsName == TEXT("CenterRight"))
		{
			OutAnchors = FAnchors(1.0f, 0.5f, 1.0f, 0.5f);
			return true;
		}
		if (AnchorsName == TEXT("BottomLeft"))
		{
			OutAnchors = FAnchors(0.0f, 1.0f, 0.0f, 1.0f);
			return true;
		}
		if (AnchorsName == TEXT("BottomCenter"))
		{
			OutAnchors = FAnchors(0.5f, 1.0f, 0.5f, 1.0f);
			return true;
		}
		if (AnchorsName == TEXT("BottomRight"))
		{
			OutAnchors = FAnchors(1.0f, 1.0f, 1.0f, 1.0f);
			return true;
		}
		if (AnchorsName == TEXT("Fill"))
		{
			OutAnchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			return true;
		}
		if (AnchorsName == TEXT("FillHorizontal"))
		{
			OutAnchors = FAnchors(0.0f, 0.0f, 1.0f, 0.0f);
			return true;
		}
		if (AnchorsName == TEXT("FillVertical"))
		{
			OutAnchors = FAnchors(0.0f, 0.0f, 0.0f, 1.0f);
			return true;
		}

		return false;
	}

	static bool ParseHorizontalAlignment(const FString& Value, EHorizontalAlignment& OutAlignment)
	{
		if (Value == TEXT("Left"))
		{
			OutAlignment = HAlign_Left;
			return true;
		}
		if (Value == TEXT("Center"))
		{
			OutAlignment = HAlign_Center;
			return true;
		}
		if (Value == TEXT("Right"))
		{
			OutAlignment = HAlign_Right;
			return true;
		}
		if (Value == TEXT("Fill"))
		{
			OutAlignment = HAlign_Fill;
			return true;
		}

		return false;
	}

	static bool ParseVerticalAlignment(const FString& Value, EVerticalAlignment& OutAlignment)
	{
		if (Value == TEXT("Top"))
		{
			OutAlignment = VAlign_Top;
			return true;
		}
		if (Value == TEXT("Center"))
		{
			OutAlignment = VAlign_Center;
			return true;
		}
		if (Value == TEXT("Bottom"))
		{
			OutAlignment = VAlign_Bottom;
			return true;
		}
		if (Value == TEXT("Fill"))
		{
			OutAlignment = VAlign_Fill;
			return true;
		}

		return false;
	}

	static bool ParseVisibility(const FString& Value, ESlateVisibility& OutVisibility)
	{
		if (Value == TEXT("Visible"))
		{
			OutVisibility = ESlateVisibility::Visible;
			return true;
		}
		if (Value == TEXT("Hidden"))
		{
			OutVisibility = ESlateVisibility::Hidden;
			return true;
		}
		if (Value == TEXT("Collapsed"))
		{
			OutVisibility = ESlateVisibility::Collapsed;
			return true;
		}
		if (Value == TEXT("HitTestInvisible"))
		{
			OutVisibility = ESlateVisibility::HitTestInvisible;
			return true;
		}
		if (Value == TEXT("SelfHitTestInvisible"))
		{
			OutVisibility = ESlateVisibility::SelfHitTestInvisible;
			return true;
		}

		return false;
	}

	static FString MakePackagePath(const FString& Directory, const FString& AssetName)
	{
		return Directory.EndsWith(TEXT("/")) ? Directory + AssetName : Directory + TEXT("/") + AssetName;
	}

	static int32 ParseApiVersionMinor(const FString& ApiVersion)
	{
		const FRegexPattern Pattern(TEXT("^0\\.([0-9]+)$"));
		FRegexMatcher Matcher(Pattern, ApiVersion);
		if (!Matcher.FindNext())
		{
			return INDEX_NONE;
		}

		int32 MinorVersion = INDEX_NONE;
		LexTryParseString(MinorVersion, *Matcher.GetCaptureGroup(1));
		return MinorVersion;
	}

	static bool IsApiVersionString(const FString& ApiVersion)
	{
		return ParseApiVersionMinor(ApiVersion) != INDEX_NONE;
	}

	static bool IsNumericVersionString(const FString& ApiVersion)
	{
		const FRegexPattern Pattern(TEXT("^[0-9]+\\.[0-9]+$"));
		FRegexMatcher Matcher(Pattern, ApiVersion);
		return Matcher.FindNext();
	}

	static bool IsValidSpecName(const FString& Name)
	{
		const FRegexPattern Pattern(TEXT("^[A-Z][A-Za-z0-9_]*$"));
		FRegexMatcher Matcher(Pattern, Name);
		return Matcher.FindNext();
	}

	static FString FirstCharUpper(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return Value;
		}

		FString Result = Value;
		Result[0] = FChar::ToUpper(Result[0]);
		return Result;
	}

	static FString ApplyUEHardcodedMapping(const FString& OriginalKey, const FString& CandidateKey)
	{
		if (const FString* MappedValue = UECasingMappings.Find(OriginalKey))
		{
			return *MappedValue;
		}

		return CandidateKey;
	}

	static bool TryBuildSupportSurfaceSnapshot(FUESpecSupportSurfaceSnapshot& OutSnapshot)
	{
		if (!GEngine)
		{
			return false;
		}

		if (UUESpecSupportSurfaceSubsystem* SurfaceSubsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
		{
			OutSnapshot = SurfaceSubsystem->BuildSnapshot();
			return true;
		}

		return false;
	}

	static void BuildSupportedKindNames(TArray<FString>& OutKindNames)
	{
		OutKindNames.Reset();

		FUESpecSupportSurfaceSnapshot Snapshot;
		if (TryBuildSupportSurfaceSnapshot(Snapshot))
		{
			for (const FUESpecSurfaceKindEntry& Entry : Snapshot.Kinds)
			{
				OutKindNames.AddUnique(Entry.Kind);
			}
		}

		if (OutKindNames.Num() == 0)
		{
			OutKindNames = FallbackKindNames;
		}
	}

	static void BuildConverterNames(TSet<FString>& OutConverterNames, TArray<FString>& OutConverterCandidates)
	{
		OutConverterNames.Reset();
		OutConverterCandidates.Reset();

		FUESpecSupportSurfaceSnapshot Snapshot;
		if (!TryBuildSupportSurfaceSnapshot(Snapshot))
		{
			return;
		}

		for (const FUESpecSurfaceConverterEntry& Entry : Snapshot.Converters)
		{
			if (!Entry.Name.IsEmpty())
			{
				OutConverterNames.Add(Entry.Name);
				OutConverterCandidates.AddUnique(Entry.Name);
			}
		}
	}

	static void BuildGlobalStyleTokenNames(TSet<FString>& OutTokenNames, TArray<FString>& OutCandidates)
	{
		OutTokenNames.Reset();
		OutCandidates.Reset();

		FUESpecSupportSurfaceSnapshot Snapshot;
		if (!TryBuildSupportSurfaceSnapshot(Snapshot))
		{
			return;
		}

		for (const FUESpecSurfaceStyleTokenEntry& Entry : Snapshot.StyleTokens)
		{
			if (Entry.Name.IsEmpty())
			{
				continue;
			}

			OutTokenNames.Add(Entry.Name);
			OutCandidates.AddUnique(Entry.Name);
			if (Entry.Name.StartsWith(TEXT("@")))
			{
				OutTokenNames.Add(Entry.Name.RightChop(1));
			}
		}
	}

	static void BuildLocalStyleTokenNames(const TSharedPtr<FJsonObject>& RootObject, TSet<FString>& OutTokenNames, TArray<FString>& OutCandidates)
	{
		OutTokenNames.Reset();
		OutCandidates.Reset();

		TSharedPtr<FJsonObject> StyleTokensObject;
		if (!TryGetObjectField(RootObject, TEXT("styleTokens"), StyleTokensObject))
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*StyleTokensObject))
		{
			if (Pair.Key.IsEmpty())
			{
				continue;
			}

			OutTokenNames.Add(Pair.Key);
			OutTokenNames.Add(FString::Printf(TEXT("@%s"), *Pair.Key));
			OutCandidates.AddUnique(FString::Printf(TEXT("@%s"), *Pair.Key));
		}
	}

	static void CollectStateNames(const TSharedPtr<FJsonObject>& RootObject, TSet<FString>& OutStateNames)
	{
		OutStateNames.Reset();

		TSharedPtr<FJsonObject> StateMachineObject;
		if (!TryGetObjectField(RootObject, TEXT("stateMachine"), StateMachineObject))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
		if (!StateMachineObject->TryGetArrayField(TEXT("states"), StatesArray) || !StatesArray)
		{
			return;
		}

		for (int32 StateIndex = 0; StateIndex < StatesArray->Num(); ++StateIndex)
		{
			const TSharedPtr<FJsonObject>* StateObject = nullptr;
			if (!(*StatesArray)[StateIndex].IsValid() || !(*StatesArray)[StateIndex]->TryGetObject(StateObject) || !StateObject || !(*StateObject).IsValid())
			{
				continue;
			}

			FString StateName;
			if ((*StateObject)->TryGetStringField(TEXT("name"), StateName) && !StateName.IsEmpty())
			{
				OutStateNames.Add(StateName);
			}
		}
	}

	static void ValidateStyleTokenValue(
		const TSharedPtr<FJsonValue>& Value,
		const FString& Path,
		const TSet<FString>& KnownTokenNames,
		const TArray<FString>& CandidateTokens,
		FUESpecCompilerResult& OutResult)
	{
		if (!Value.IsValid())
		{
			return;
		}

		if (Value->Type == EJson::String)
		{
			const FString RawString = Value->AsString();
			if (RawString.StartsWith(TEXT("@")) && !KnownTokenNames.Contains(RawString) && !KnownTokenNames.Contains(RawString.RightChop(1)))
			{
				UUESpecCompilerSubsystem::AddError(
					OutResult,
					TEXT("UNKNOWN_STYLE_TOKEN"),
					Path,
					FString::Printf(TEXT("Style token '%s' is not available in the local or project support surface."), *RawString),
					TEXT("Register the token in styleTokens or ComponentRegistry.StyleTokens, or replace it with a known token."),
					UUESpecCompilerSubsystem::BuildNearestSuggestions(RawString, CandidateTokens));
			}

			return;
		}

		if (Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*ObjectValue))
			{
				ValidateStyleTokenValue(Pair.Value, MakeChildPath(Path, Pair.Key), KnownTokenNames, CandidateTokens, OutResult);
			}

			return;
		}

		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayValue = Value->AsArray();
			for (int32 Index = 0; Index < ArrayValue.Num(); ++Index)
			{
				ValidateStyleTokenValue(ArrayValue[Index], MakeArrayPath(Path, Index), KnownTokenNames, CandidateTokens, OutResult);
			}
		}
	}

	static void ValidateConverterReferences(
		const TSharedPtr<FJsonValue>& Value,
		const FString& Path,
		const TSet<FString>& ConverterNames,
		const TArray<FString>& CandidateConverters,
		FUESpecCompilerResult& OutResult)
	{
		if (!Value.IsValid())
		{
			return;
		}

		if (Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
			if (const TSharedPtr<FJsonValue>* ConvertValue = ObjectValue->Values.Find(TEXT("convert")))
			{
				if (ConvertValue->IsValid() && (*ConvertValue)->Type == EJson::String)
				{
					const FString ConverterName = (*ConvertValue)->AsString();
					if (!ConverterNames.Contains(ConverterName))
					{
						UUESpecCompilerSubsystem::AddError(
							OutResult,
							TEXT("UNKNOWN_CONVERTER"),
							MakeChildPath(Path, TEXT("convert")),
							FString::Printf(TEXT("Converter '%s' is not registered in the support surface."), *ConverterName),
							TEXT("Register the converter in ComponentRegistry.Converters or use one of the known converter names."),
							UUESpecCompilerSubsystem::BuildNearestSuggestions(ConverterName, CandidateConverters));
					}
				}
			}

			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*ObjectValue))
			{
				ValidateConverterReferences(Pair.Value, MakeChildPath(Path, Pair.Key), ConverterNames, CandidateConverters, OutResult);
			}

			return;
		}

		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayValue = Value->AsArray();
			for (int32 Index = 0; Index < ArrayValue.Num(); ++Index)
			{
				ValidateConverterReferences(ArrayValue[Index], MakeArrayPath(Path, Index), ConverterNames, CandidateConverters, OutResult);
			}
		}
	}

	static void CollectWidgetIds(
		const TSharedPtr<FJsonObject>& NodeObject,
		const FString& NodePath,
		const FString& TopLevelApiVersion,
		TMap<FString, FString>& OutWidgetIdPaths,
		FUESpecCompilerResult& OutResult)
	{
		if (!NodeObject.IsValid())
		{
			return;
		}

		FString WidgetId;
		if (NodeObject->TryGetStringField(TEXT("id"), WidgetId) && !WidgetId.IsEmpty())
		{
			const FString IdPath = MakeChildPath(NodePath, TEXT("id"));
			if (!UUESpecCompilerSubsystem::IsCamelCaseIdentifier(WidgetId))
			{
				UUESpecCompilerSubsystem::AddError(
					OutResult,
					TEXT("INVALID_WIDGET_ID"),
					IdPath,
					FString::Printf(TEXT("Widget id '%s' must match ^[a-z][a-zA-Z0-9_]*$."), *WidgetId),
					TEXT("Rename the widget id to camelCase, for example 'slotQ' or 'cooldownBar'."));
			}

			if (const FString* ExistingPath = OutWidgetIdPaths.Find(WidgetId))
			{
				UUESpecCompilerSubsystem::AddError(
					OutResult,
					TEXT("DUPLICATE_WIDGET_ID"),
					IdPath,
					FString::Printf(TEXT("Widget id '%s' is already declared at %s."), *WidgetId, *(*ExistingPath)),
					TEXT("Rename one of the widgets so every id is unique inside the spec."));
			}
			else
			{
				OutWidgetIdPaths.Add(WidgetId, IdPath);
			}
		}

		FString NodeApiVersion;
		if (NodeObject->TryGetStringField(TEXT("apiVersion"), NodeApiVersion) && !NodeApiVersion.IsEmpty())
		{
			if (!IsNumericVersionString(NodeApiVersion))
			{
				UUESpecCompilerSubsystem::AddError(
					OutResult,
					TEXT("INVALID_API_VERSION"),
					MakeChildPath(NodePath, TEXT("apiVersion")),
					FString::Printf(TEXT("apiVersion '%s' must match ^0\\.[0-9]+$."), *NodeApiVersion),
					TEXT("Use a numeric minor version such as \"0.1\" or \"0.2\"."));
			}
			else if (!IsApiVersionString(NodeApiVersion) || UUESpecCompilerSubsystem::IsApiVersionGreaterThan(NodeApiVersion, TopLevelApiVersion))
			{
				UUESpecCompilerSubsystem::AddError(
					OutResult,
					TEXT("API_VERSION_TOO_HIGH"),
					MakeChildPath(NodePath, TEXT("apiVersion")),
					FString::Printf(TEXT("Subtree apiVersion '%s' cannot exceed root apiVersion '%s'."), *NodeApiVersion, *TopLevelApiVersion),
					TEXT("Lower the subtree apiVersion, or raise the root apiVersion within compiler support."));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
		{
			return;
		}

		for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
		{
			const TSharedPtr<FJsonObject>* ChildObject = nullptr;
			if ((*ChildrenArray)[ChildIndex].IsValid() && (*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid())
			{
				CollectWidgetIds(*ChildObject, MakeArrayPath(MakeChildPath(NodePath, TEXT("children")), ChildIndex), TopLevelApiVersion, OutWidgetIdPaths, OutResult);
			}
		}
	}

	static void ValidateWidgetNodeDetails(
		const TSharedPtr<FJsonObject>& NodeObject,
		const FString& NodePath,
		const TMap<FString, FString>& WidgetIdPaths,
		const TSet<FString>& StateNames,
		const TSet<FString>& KnownStyleTokenNames,
		const TArray<FString>& StyleTokenCandidates,
		FUESpecCompilerResult& OutResult)
	{
		if (!NodeObject.IsValid())
		{
			return;
		}

		TSharedPtr<FJsonObject> StyleObject;
		if (TryGetObjectField(NodeObject, TEXT("style"), StyleObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*StyleObject))
			{
				ValidateStyleTokenValue(Pair.Value, MakeChildPath(MakeChildPath(NodePath, TEXT("style")), Pair.Key), KnownStyleTokenNames, StyleTokenCandidates, OutResult);
			}
		}

		TSharedPtr<FJsonObject> SlotObject;
		if (TryGetObjectField(NodeObject, TEXT("slot"), SlotObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*SlotObject))
			{
				ValidateStyleTokenValue(Pair.Value, MakeChildPath(MakeChildPath(NodePath, TEXT("slot")), Pair.Key), KnownStyleTokenNames, StyleTokenCandidates, OutResult);
			}
		}

		TSharedPtr<FJsonObject> NavigationObject;
		if (TryGetObjectField(NodeObject, TEXT("navigation"), NavigationObject))
		{
			for (const FString& DirectionKey : { TEXT("up"), TEXT("down"), TEXT("left"), TEXT("right") })
			{
				FString TargetId;
				if (NavigationObject->TryGetStringField(DirectionKey, TargetId) && !TargetId.IsEmpty() && !WidgetIdPaths.Contains(TargetId))
				{
					TArray<FString> KnownIds;
					WidgetIdPaths.GetKeys(KnownIds);
					UUESpecCompilerSubsystem::AddError(
						OutResult,
						TEXT("UNKNOWN_NAV_TARGET"),
						MakeChildPath(MakeChildPath(NodePath, TEXT("navigation")), DirectionKey),
						FString::Printf(TEXT("navigation.%s targets unknown widget id '%s'."), *DirectionKey, *TargetId),
						TEXT("Point navigation to an existing widget id in the same spec."),
						UUESpecCompilerSubsystem::BuildNearestSuggestions(TargetId, KnownIds));
				}
			}
		}

		TSharedPtr<FJsonObject> StateBindingsObject;
		if (TryGetObjectField(NodeObject, TEXT("stateBindings"), StateBindingsObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*StateBindingsObject))
			{
				if (!StateNames.Contains(Pair.Key))
				{
					TArray<FString> KnownStates = StateNames.Array();
					KnownStates.Sort();
					UUESpecCompilerSubsystem::AddError(
						OutResult,
						TEXT("UNKNOWN_STATE"),
						MakeChildPath(MakeChildPath(NodePath, TEXT("stateBindings")), Pair.Key),
						FString::Printf(TEXT("stateBindings references unknown state '%s'."), *Pair.Key),
						TEXT("Add the state to stateMachine.states, or rename the binding to a declared state."),
						UUESpecCompilerSubsystem::BuildNearestSuggestions(Pair.Key, KnownStates));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
		if (NodeObject->TryGetArrayField(TEXT("actions"), ActionsArray) && ActionsArray)
		{
			UUESpecCompilerSubsystem::ValidateActions(*ActionsArray, MakeChildPath(NodePath, TEXT("actions")), OutResult);
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
		{
			return;
		}

		for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
		{
			const TSharedPtr<FJsonObject>* ChildObject = nullptr;
			if ((*ChildrenArray)[ChildIndex].IsValid() && (*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid())
			{
				ValidateWidgetNodeDetails(*ChildObject, MakeArrayPath(MakeChildPath(NodePath, TEXT("children")), ChildIndex), WidgetIdPaths, StateNames, KnownStyleTokenNames, StyleTokenCandidates, OutResult);
			}
		}
	}

	static void CollectStateBindingsFromNode(const TSharedPtr<FJsonObject>& NodeObject, FUESpecStateMachineDefinition& OutStateMachineDefinition)
	{
		if (!NodeObject.IsValid())
		{
			return;
		}

		FString WidgetId;
		NodeObject->TryGetStringField(TEXT("id"), WidgetId);

		TSharedPtr<FJsonObject> StateBindingsObject;
		if (!WidgetId.IsEmpty() && TryGetObjectField(NodeObject, TEXT("stateBindings"), StateBindingsObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& StatePair : UE::UESpec::Json::GetFields(*StateBindingsObject))
			{
				const TSharedPtr<FJsonObject>* PropertyObject = nullptr;
				if (!StatePair.Value.IsValid() || !StatePair.Value->TryGetObject(PropertyObject) || !PropertyObject || !(*PropertyObject).IsValid())
				{
					continue;
				}

				FUESpecWidgetStateBinding& Binding = OutStateMachineDefinition.StateBindings.AddDefaulted_GetRef();
				Binding.WidgetId = FName(*WidgetId);
				Binding.State = FName(*StatePair.Key);

				for (const TPair<FString, TSharedPtr<FJsonValue>>& PropertyPair : UE::UESpec::Json::GetFields(*(*PropertyObject)))
				{
					FUESpecStateBindingProperty& PropertyBinding = Binding.Properties.AddDefaulted_GetRef();
					PropertyBinding.Property = FName(*PropertyPair.Key);
					PropertyBinding.Value = SerializeJsonValue(PropertyPair.Value);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray)
		{
			const TSharedPtr<FJsonObject>* ChildObject = nullptr;
			if (ChildValue.IsValid() && ChildValue->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid())
			{
				CollectStateBindingsFromNode(*ChildObject, OutStateMachineDefinition);
			}
		}
	}

	static void BuildStateMachineDefinitionFromSpec(const TSharedPtr<FJsonObject>& RootObject, FUESpecStateMachineDefinition& OutStateMachineDefinition)
	{
		OutStateMachineDefinition = FUESpecStateMachineDefinition();

		TSharedPtr<FJsonObject> StateMachineObject;
		if (!TryGetObjectField(RootObject, TEXT("stateMachine"), StateMachineObject))
		{
			return;
		}

		FString InitialStateName;
		if (!StateMachineObject->TryGetStringField(TEXT("initial"), InitialStateName))
		{
			StateMachineObject->TryGetStringField(TEXT("initialState"), InitialStateName);
		}

		if (!InitialStateName.IsEmpty())
		{
			OutStateMachineDefinition.InitialState = FName(*InitialStateName);
		}

		const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
		if (StateMachineObject->TryGetArrayField(TEXT("states"), StatesArray) && StatesArray)
		{
			for (const TSharedPtr<FJsonValue>& StateValue : *StatesArray)
			{
				const TSharedPtr<FJsonObject>* StateObject = nullptr;
				if (!StateValue.IsValid() || !StateValue->TryGetObject(StateObject) || !StateObject || !(*StateObject).IsValid())
				{
					continue;
				}

				FUESpecStateDefinition& StateDefinition = OutStateMachineDefinition.States.AddDefaulted_GetRef();

				FString StateName;
				if ((*StateObject)->TryGetStringField(TEXT("name"), StateName))
				{
					StateDefinition.Name = FName(*StateName);
				}

				(*StateObject)->TryGetStringField(TEXT("onEnter"), StateDefinition.OnEnter);
				(*StateObject)->TryGetStringField(TEXT("onExit"), StateDefinition.OnExit);

				const TArray<TSharedPtr<FJsonValue>>* TransitionsArray = nullptr;
				if ((*StateObject)->TryGetArrayField(TEXT("transitions"), TransitionsArray) && TransitionsArray)
				{
					for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionsArray)
					{
						const TSharedPtr<FJsonObject>* TransitionObject = nullptr;
						if (!TransitionValue.IsValid() || !TransitionValue->TryGetObject(TransitionObject) || !TransitionObject || !(*TransitionObject).IsValid())
						{
							continue;
						}

						FUESpecStateTransition& Transition = StateDefinition.Transitions.AddDefaulted_GetRef();

						FString ToState;
						if ((*TransitionObject)->TryGetStringField(TEXT("to"), ToState))
						{
							Transition.To = FName(*ToState);
						}

						(*TransitionObject)->TryGetStringField(TEXT("when"), Transition.When);
					}
				}
			}
		}

		TSharedPtr<FJsonObject> RootWidgetObject;
		if (TryGetObjectField(RootObject, TEXT("root"), RootWidgetObject))
		{
			CollectStateBindingsFromNode(RootWidgetObject, OutStateMachineDefinition);
		}
	}

	static void ApplyBlueprintDefaultsFromSpec(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject)
	{
		FKismetEditorUtilities::CompileBlueprint(&WidgetBlueprint);

		const UClass* GeneratedClass = WidgetBlueprint.GeneratedClass;
		if (!GeneratedClass)
		{
			return;
		}

		if (UWidgetBlueprintGeneratedClass* const WidgetBlueprintClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint.GeneratedClass))
		{
			WidgetBlueprintClass->SetClassRequiresNativeTick(true);
		}

		FString Kind;
		RootObject->TryGetStringField(TEXT("kind"), Kind);

		TSharedPtr<FJsonObject> ViewModelObject;
		FString ViewModelClassName;
		if (TryGetObjectField(RootObject, TEXT("viewModel"), ViewModelObject))
		{
			ViewModelObject->TryGetStringField(TEXT("class"), ViewModelClassName);
		}

		UClass* ViewModelClass = UUESpecCompilerSubsystem::ResolveViewModelClass(ViewModelClassName);
		FUESpecStateMachineDefinition StateMachineDefinition;
		BuildStateMachineDefinitionFromSpec(RootObject, StateMachineDefinition);

		if (UUESpecWidgetBase* WidgetDefaults = Cast<UUESpecWidgetBase>(GeneratedClass->GetDefaultObject()))
		{
			if (Kind == TEXT("Component"))
			{
				WidgetDefaults->WidgetKind = EUESpecWidgetKind::Component;
			}
			else if (Kind == TEXT("WorldSpace"))
			{
				WidgetDefaults->WidgetKind = EUESpecWidgetKind::WorldSpace;
			}

			if (ViewModelClass)
			{
				WidgetDefaults->ViewModelClass = ViewModelClass;
			}

			WidgetDefaults->ViewModelContext = DefaultViewModelContext;
			WidgetDefaults->StateMachine = StateMachineDefinition;
		}
		else if (UUESpecActivatableWidgetBase* ActivatableDefaults = Cast<UUESpecActivatableWidgetBase>(GeneratedClass->GetDefaultObject()))
		{
			if (Kind == TEXT("HUD"))
			{
				ActivatableDefaults->WidgetKind = EUESpecWidgetKind::HUD;
				ActivatableDefaults->LayerRoute = TEXT("HUDLayer");
			}
			else if (Kind == TEXT("Menu"))
			{
				ActivatableDefaults->WidgetKind = EUESpecWidgetKind::Menu;
				ActivatableDefaults->LayerRoute = TEXT("MenuStack");
			}
			else if (Kind == TEXT("Popup"))
			{
				ActivatableDefaults->WidgetKind = EUESpecWidgetKind::Popup;
				ActivatableDefaults->LayerRoute = TEXT("PopupLayer");
			}
			else if (Kind == TEXT("Modal"))
			{
				ActivatableDefaults->WidgetKind = EUESpecWidgetKind::Modal;
				ActivatableDefaults->LayerRoute = TEXT("ModalLayer");
			}
			else if (Kind == TEXT("Debug"))
			{
				ActivatableDefaults->WidgetKind = EUESpecWidgetKind::Debug;
				ActivatableDefaults->LayerRoute = TEXT("DebugLayer");
			}

			if (ViewModelClass)
			{
				ActivatableDefaults->ViewModelClass = ViewModelClass;
			}

			ActivatableDefaults->ViewModelContext = DefaultViewModelContext;
			ActivatableDefaults->StateMachine = StateMachineDefinition;
		}
	}

	static bool IsConstructibleWidgetClass(UClass* WidgetClass)
	{
		return WidgetClass &&
			WidgetClass->IsChildOf(UWidget::StaticClass()) &&
			!WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	}

	static void CleanupFailedWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
	{
		if (!WidgetBlueprint)
		{
			return;
		}

		ObjectTools::DeleteSingleObject(WidgetBlueprint, false);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	// ---- Asset identity: deterministic WBP_<name>, ownership tags, in-place update (HF) ----

	static FString MakeGeneratedAssetName(const FString& SpecName)
	{
		return FString::Printf(TEXT("WBP_%s"), *SpecName);
	}

	static FString GetGeneratorName()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UESpec"));
		const FString Version = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("unknown"));
		return FString::Printf(TEXT("UESpecCompilerSubsystem/%s"), *Version);
	}

	static FString GetOwnershipSpecName(const UWidgetBlueprint& WidgetBlueprint)
	{
		const UObject* const Defaults = WidgetBlueprint.GeneratedClass ? WidgetBlueprint.GeneratedClass->GetDefaultObject() : nullptr;
		if (const UUESpecWidgetBase* const WidgetDefaults = Cast<UUESpecWidgetBase>(Defaults))
		{
			return WidgetDefaults->UESpecSpecName;
		}
		if (const UUESpecActivatableWidgetBase* const ActivatableDefaults = Cast<UUESpecActivatableWidgetBase>(Defaults))
		{
			return ActivatableDefaults->UESpecSpecName;
		}
		return FString();
	}

	template <typename TWidgetBase>
	static void SetOwnershipTags(TWidgetBase& Defaults, const FString& SpecName, const FString& SpecHash, const FString& Generator)
	{
		Defaults.UESpecSpecName = SpecName;
		Defaults.UESpecSpecHash = SpecHash;
		Defaults.UESpecGenerator = Generator;
	}

	// Written on the generated class CDO so UBlueprint::GetAssetRegistryTags publishes them (queryable without loading).
	static void ApplyOwnershipTags(UWidgetBlueprint& WidgetBlueprint, const FString& SpecName, const FString& SpecHash)
	{
		UObject* const Defaults = WidgetBlueprint.GeneratedClass ? WidgetBlueprint.GeneratedClass->GetDefaultObject() : nullptr;
		const FString Generator = GetGeneratorName();
		if (UUESpecWidgetBase* const WidgetDefaults = Cast<UUESpecWidgetBase>(Defaults))
		{
			SetOwnershipTags(*WidgetDefaults, SpecName, SpecHash, Generator);
		}
		else if (UUESpecActivatableWidgetBase* const ActivatableDefaults = Cast<UUESpecActivatableWidgetBase>(Defaults))
		{
			SetOwnershipTags(*ActivatableDefaults, SpecName, SpecHash, Generator);
		}
	}

	// Compile-time defaults the compiler owns go back to the native parent's values, so a spec that drops
	// e.g. its viewModel does not inherit the previous build's ViewModelClass/StateMachine/ManualBindings.
	template <typename TWidgetBase>
	static void ResetCompilerOwnedDefaults(TWidgetBase& Defaults)
	{
		const TWidgetBase* const NativeDefaults = GetDefault<TWidgetBase>();
		Defaults.WidgetKind = NativeDefaults->WidgetKind;
		Defaults.ViewModelClass = NativeDefaults->ViewModelClass;
		Defaults.ViewModelContext = NativeDefaults->ViewModelContext;
		Defaults.BindingMode = NativeDefaults->BindingMode;
		Defaults.ManualBindings = NativeDefaults->ManualBindings;
		Defaults.StateMachine = NativeDefaults->StateMachine;
	}

	static void RenameToTransientPackage(UObject& Object)
	{
		const FName TrashName = MakeUniqueObjectName(GetTransientPackage(), Object.GetClass(), *FString::Printf(TEXT("TRASH_%s"), *Object.GetName()));
		Object.Rename(*TrashName.ToString(), GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	}

	static void CloseAssetEditorsFor(UWidgetBlueprint& WidgetBlueprint)
	{
		if (GEditor)
		{
			if (UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSubsystem->CloseAllEditorsForAsset(&WidgetBlueprint);
			}
		}
	}

	/**
	 * Strips everything a previous compile generated from a compiler-owned WidgetBlueprint before it is rebuilt in place:
	 * (1) widgets move to the transient package (ConstructWidget(..., FName(WidgetId)) would collide with same-named ones),
	 * (2) MVVM bindings and ViewModel contexts are removed (AddBinding only appends; EnsureViewModelContext reuses by name),
	 * (3) every event-graph node is removed (FindOrCreateBoundEvent would reuse stale bound events and their chains),
	 * (4) compiler-owned CDO defaults are reset. The asset object, its package and BlueprintGuid are kept.
	 */
	static void ResetGeneratedWidgetBlueprintContent(UWidgetBlueprint& WidgetBlueprint)
	{
		WidgetBlueprint.Modify();

		// RemoveViewModels/MarkBlueprintAsStructurallyModified below compile the blueprint against the emptied tree; with
		// the old GUID map in place ValidateAndFixUpVariableGuids would ensure for every trashed widget. Park the map and
		// restore it at the end so ReconcileWidgetVariableGuids can keep same-named widgets' GUIDs after the rebuild.
		TMap<FName, FGuid> ParkedVariableGuids = MoveTemp(WidgetBlueprint.WidgetVariableNameToGuidMap);
		WidgetBlueprint.WidgetVariableNameToGuidMap.Reset();

		if (UWidgetTree* const WidgetTree = WidgetBlueprint.WidgetTree)
		{
			TArray<UWidget*> OldWidgets;
			WidgetTree->GetAllWidgets(OldWidgets);

			// Also catch widgets that are outered to the tree but no longer reachable from its root.
			TArray<UObject*> TreeSubobjects;
			GetObjectsWithOuter(WidgetTree, TreeSubobjects, EGetObjectsFlags::None);
			for (UObject* const Subobject : TreeSubobjects)
			{
				if (UWidget* const Widget = Cast<UWidget>(Subobject))
				{
					OldWidgets.AddUnique(Widget);
				}
			}

			WidgetTree->Modify();
			WidgetTree->RootWidget = nullptr;
			for (UWidget* const OldWidget : OldWidgets)
			{
				if (OldWidget && OldWidget->GetOuter() == WidgetTree)
				{
					RenameToTransientPackage(*OldWidget);
				}
			}
		}

		if (UMVVMEditorSubsystem* const MVVMEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>() : nullptr)
		{
			if (UMVVMBlueprintView* const BlueprintView = MVVMEditorSubsystem->GetView(&WidgetBlueprint))
			{
				BlueprintView->Modify();
				for (int32 BindingIndex = BlueprintView->GetNumBindings() - 1; BindingIndex >= 0; --BindingIndex)
				{
					BlueprintView->RemoveBindingAt(BindingIndex);
				}

				TArray<FGuid> ViewModelIds;
				for (const FMVVMBlueprintViewModelContext& ViewModelContext : BlueprintView->GetViewModels())
				{
					ViewModelIds.Add(ViewModelContext.GetViewModelId());
				}
				if (ViewModelIds.Num() > 0)
				{
					BlueprintView->RemoveViewModels(MakeArrayView(ViewModelIds));
				}
			}
		}

		for (UEdGraph* const Graph : WidgetBlueprint.UbergraphPages)
		{
			if (!Graph)
			{
				continue;
			}

			const TArray<UEdGraphNode*> GraphNodes = Graph->Nodes;
			for (UEdGraphNode* const Node : GraphNodes)
			{
				if (Node)
				{
					FBlueprintEditorUtils::RemoveNode(&WidgetBlueprint, Node, /*bDontRecompile*/ true);
				}
			}
		}

		UObject* const Defaults = WidgetBlueprint.GeneratedClass ? WidgetBlueprint.GeneratedClass->GetDefaultObject() : nullptr;
		if (UUESpecWidgetBase* const WidgetDefaults = Cast<UUESpecWidgetBase>(Defaults))
		{
			ResetCompilerOwnedDefaults(*WidgetDefaults);
		}
		else if (UUESpecActivatableWidgetBase* const ActivatableDefaults = Cast<UUESpecActivatableWidgetBase>(Defaults))
		{
			ResetCompilerOwnedDefaults(*ActivatableDefaults);
			ActivatableDefaults->LayerRoute = GetDefault<UUESpecActivatableWidgetBase>()->LayerRoute;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);

		WidgetBlueprint.WidgetVariableNameToGuidMap = MoveTemp(ParkedVariableGuids);
	}

	// Keeps WidgetVariableNameToGuidMap in step with the rebuilt tree: same-named widgets keep their GUID, new ones get a
	// deterministic one, removed ones are dropped. Without this the widget compiler's ValidateAndFixUpVariableGuids raises
	// ensureAlways for every added/removed widget on an in-place update. An empty map is left for the compiler to populate.
	static void ReconcileWidgetVariableGuids(UWidgetBlueprint& WidgetBlueprint)
	{
		TMap<FName, FGuid>& GuidMap = WidgetBlueprint.WidgetVariableNameToGuidMap;
		if (GuidMap.IsEmpty())
		{
			return;
		}

		TSet<FName> LiveVariableNames;
		WidgetBlueprint.ForEachSourceWidget([&GuidMap, &LiveVariableNames](UWidget* Widget)
		{
			LiveVariableNames.Add(Widget->GetFName());
			if (!GuidMap.Contains(Widget->GetFName()))
			{
				GuidMap.Add(Widget->GetFName(), FGuid::NewDeterministicGuid(Widget->GetPathName()));
			}
		});

		for (const UWidgetAnimation* const Animation : WidgetBlueprint.Animations)
		{
			if (Animation)
			{
				LiveVariableNames.Add(Animation->GetFName());
			}
		}

		for (auto It = GuidMap.CreateIterator(); It; ++It)
		{
			if (!LiveVariableNames.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}
	}

	// Failed in-place rebuild: never save; drop the in-memory edits by reloading the last saved package.
	static void RestoreWidgetBlueprintFromDisk(UWidgetBlueprint& WidgetBlueprint, FUESpecCompilerResult& OutResult)
	{
		CloseAssetEditorsFor(WidgetBlueprint);

		TArray<UPackage*> PackagesToReload;
		PackagesToReload.Add(WidgetBlueprint.GetPackage());

		FText ReloadError;
		if (!UPackageTools::ReloadPackages(PackagesToReload, ReloadError, EReloadPackagesInteractionMode::AssumePositive))
		{
			UUESpecCompilerSubsystem::AddError(
				OutResult,
				TEXT("ASSET_RESTORE_FAILED"),
				TEXT("$"),
				FString::Printf(TEXT("Failed to restore '%s' to its last saved state: %s"), *WidgetBlueprint.GetPathName(), *ReloadError.ToString()),
				TEXT("Reload the asset manually (or revert it in source control) before compiling again."));
		}
	}
}

bool UUESpecCompilerSubsystem::ValidateJsonText(const FString& JsonText, FUESpecCompilerResult& OutResult) const
{
	OutResult = FUESpecCompilerResult();

	TSharedPtr<FJsonObject> RootObject;
	if (!ParseJsonTextToObject(JsonText, RootObject))
	{
		AddError(
			OutResult,
			TEXT("INVALID_JSON"),
			TEXT("$"),
			TEXT("Input is not valid JSON."),
			TEXT("Fix JSON syntax before compiling."));
		return false;
	}

	FUESpecCompilerError NormalizeError;
	if (!NormalizeJsonObject(RootObject, OutResult.NormalizedJson, NormalizeError))
	{
		OutResult.Errors.Add(MoveTemp(NormalizeError));
		return false;
	}

	ValidateTopLevelDocument(RootObject, OutResult);
	ValidateWidgetTree(RootObject, OutResult);
	ValidateTestSpec(RootObject, OutResult);
	ValidateViewModelMethods(RootObject, OutResult);

	OutResult.bOk = OutResult.Errors.Num() == 0;
	return OutResult.bOk;
}

bool UUESpecCompilerSubsystem::ValidateJsonFile(const FString& SpecFilePath, FUESpecCompilerResult& OutResult) const
{
	FString JsonText;
	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(SpecFilePath);
	if (!FFileHelper::LoadFileToString(JsonText, *AbsolutePath))
	{
		OutResult = FUESpecCompilerResult();
		AddError(
			OutResult,
			TEXT("SPEC_NOT_FOUND"),
			TEXT("$"),
			FString::Printf(TEXT("Unable to read UISpec file '%s'."), *AbsolutePath),
			TEXT("Check the file path and try again."));
		return false;
	}

	return ValidateJsonText(JsonText, OutResult);
}

bool UUESpecCompilerSubsystem::CompileJsonFileToWidgetBlueprint(const FString& SpecFilePath, const FString& OutputDirectory, FUESpecCompilerResult& OutResult) const
{
	if (!ValidateJsonFile(SpecFilePath, OutResult))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	if (!ParseJsonTextToObject(OutResult.NormalizedJson, RootObject))
	{
		AddError(
			OutResult,
			TEXT("INVALID_NORMALIZED_JSON"),
			TEXT("$"),
			TEXT("Compiler failed to parse its normalized UISpec."),
			TEXT("Fix the compiler normalization logic before continuing."));
		OutResult.bOk = false;
		return false;
	}

	FString Kind;
	RootObject->TryGetStringField(TEXT("kind"), Kind);
	if (Kind == TEXT("Test"))
	{
		AddError(
			OutResult,
			TEXT("TEST_SPEC_CANNOT_COMPILE_TO_WIDGET"),
			TEXT("kind"),
			TEXT("kind='Test' routes to the testSpec executor, not the WidgetBlueprint compiler."),
			TEXT("Compile the target UISpec instead, or run the testSpec executor."));
		OutResult.bOk = false;
		return false;
	}

	FString SpecName;
	RootObject->TryGetStringField(TEXT("name"), SpecName);
	OutResult.SpecHash = ComputeSpecHash(OutResult.NormalizedJson);

	// Semantic validation has passed; only now is the target asset resolved (and possibly loaded).
	FString AssetPath;
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	if (!ResolveTargetWidgetBlueprint(SpecName, Kind, OutputDirectory, AssetPath, WidgetBlueprint, OutResult))
	{
		OutResult.bOk = false;
		return false;
	}

	const bool bUpdatingExisting = WidgetBlueprint != nullptr;
	if (bUpdatingExisting)
	{
		UE::UESpec::Editor::Private::CloseAssetEditorsFor(*WidgetBlueprint);
		UE::UESpec::Editor::Private::ResetGeneratedWidgetBlueprintContent(*WidgetBlueprint);
	}
	else
	{
		WidgetBlueprint = CreateWidgetBlueprintAsset(SpecName, Kind, OutputDirectory, AssetPath, OutResult);
		if (!WidgetBlueprint)
		{
			OutResult.bOk = false;
			return false;
		}
	}

	// A new asset is deleted on failure; an updated one is never saved and is reloaded from its last saved state.
	const auto FailCompile = [&OutResult, WidgetBlueprint, bUpdatingExisting]()
	{
		if (bUpdatingExisting)
		{
			UE::UESpec::Editor::Private::RestoreWidgetBlueprintFromDisk(*WidgetBlueprint, OutResult);
		}
		else
		{
			UE::UESpec::Editor::Private::CleanupFailedWidgetBlueprint(WidgetBlueprint);
		}
		OutResult.bOk = false;
		return false;
	};

	if (!BuildWidgetBlueprintTree(*WidgetBlueprint, RootObject, OutResult))
	{
		return FailCompile();
	}

	UE::UESpec::Editor::Private::ReconcileWidgetVariableGuids(*WidgetBlueprint);
	UE::UESpec::Editor::Private::ApplyBlueprintDefaultsFromSpec(*WidgetBlueprint, RootObject);
	UE::UESpec::Editor::Private::ApplyOwnershipTags(*WidgetBlueprint, SpecName, OutResult.SpecHash);

	if (!FUESpecBindingBuilder::Build(*WidgetBlueprint, RootObject, OutResult))
	{
		return FailCompile();
	}

	if (!FUESpecEventGraphBuilder::Build(*WidgetBlueprint, RootObject, OutResult))
	{
		return FailCompile();
	}

	if (!SaveWidgetBlueprintAsset(*WidgetBlueprint, AssetPath, OutResult))
	{
		return FailCompile();
	}

	OutResult.GeneratedAssets.Reset();
	OutResult.GeneratedAssets.Add(AssetPath);
	OutResult.AssetPath = AssetPath;
	OutResult.BlueprintGuid = WidgetBlueprint->GetBlueprintGuid().ToString(EGuidFormats::DigitsWithHyphens);
	OutResult.AssetAction = bUpdatingExisting ? TEXT("updated") : TEXT("created");
	OutResult.SavedPackageFile = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(
		FPackageName::ObjectPathToPackageName(AssetPath), FPackageName::GetAssetPackageExtension()));
	OutResult.bOk = true;
	return true;
}

FString UUESpecCompilerSubsystem::ComputeSpecHash(const FString& NormalizedJson)
{
	return UE::UESpec::Editor::Sha256::HashUtf8ToHex(NormalizedJson);
}

bool UUESpecCompilerSubsystem::ResolveTargetWidgetBlueprint(const FString& SpecName, const FString& Kind, const FString& OutputDirectory, FString& OutAssetPath, UWidgetBlueprint*& OutExistingWidgetBlueprint, FUESpecCompilerResult& OutResult)
{
	OutExistingWidgetBlueprint = nullptr;

	const FString PackageDirectory = OutputDirectory.IsEmpty() ? FString(UE::UESpec::Editor::Private::DefaultOutputDirectory) : OutputDirectory;
	if (!FPackageName::IsValidLongPackageName(PackageDirectory))
	{
		AddError(
			OutResult,
			TEXT("INVALID_OUTPUT_DIRECTORY"),
			TEXT("$"),
			FString::Printf(TEXT("Output directory '%s' is not a valid long package path."), *PackageDirectory),
			TEXT("Use a package path such as /Game/UI/Generated."));
		return false;
	}

	UClass* const ParentClass = ResolveWidgetBlueprintParentClass(Kind);
	if (!ParentClass)
	{
		AddError(
			OutResult,
			TEXT("UNSUPPORTED_WIDGET_KIND"),
			TEXT("kind"),
			FString::Printf(TEXT("kind '%s' does not map to a WidgetBlueprint base class yet."), *Kind),
			TEXT("Use HUD/Menu/Popup/Modal/Debug/WorldSpace/Component for the current compiler."));
		return false;
	}

	const FString AssetName = UE::UESpec::Editor::Private::MakeGeneratedAssetName(SpecName);
	const FString PackageName = UE::UESpec::Editor::Private::MakePackagePath(PackageDirectory, AssetName);
	FText InvalidNameReason;
	if (!FPackageName::IsValidLongPackageName(PackageName, /*bIncludeReadOnlyRoots*/ false, &InvalidNameReason))
	{
		AddError(
			OutResult,
			TEXT("INVALID_ASSET_NAME"),
			TEXT("name"),
			FString::Printf(TEXT("'%s' is not a valid asset package name: %s"), *PackageName, *InvalidNameReason.ToString()),
			TEXT("Use a spec name made of letters, digits and underscores."));
		return false;
	}
	OutAssetPath = PackageName + TEXT(".") + AssetName;

	UObject* ExistingObject = nullptr;
	if (UPackage* const ExistingPackage = FindPackage(nullptr, *PackageName))
	{
		ExistingObject = FindObject<UObject>(ExistingPackage, *AssetName);
	}
	if (!IsValid(ExistingObject) && FPackageName::DoesPackageExist(PackageName))
	{
		ExistingObject = LoadObject<UObject>(nullptr, *OutAssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!IsValid(ExistingObject))
		{
			AddError(
				OutResult,
				TEXT("TARGET_NOT_GENERATED"),
				TEXT("name"),
				FString::Printf(TEXT("Package '%s' exists but holds no loadable asset named '%s'; the compiler never overwrites it."), *PackageName, *AssetName),
				TEXT("Rename the spec, pick another output directory, or remove the existing package."));
			return false;
		}
	}

	if (!IsValid(ExistingObject))
	{
		return true;
	}

	UWidgetBlueprint* const ExistingWidgetBlueprint = Cast<UWidgetBlueprint>(ExistingObject);
	if (!ExistingWidgetBlueprint || UE::UESpec::Editor::Private::GetOwnershipSpecName(*ExistingWidgetBlueprint).IsEmpty())
	{
		AddError(
			OutResult,
			TEXT("TARGET_NOT_GENERATED"),
			TEXT("name"),
			FString::Printf(TEXT("'%s' (%s) already exists and has no UESpecSpecName ownership tag; the compiler only updates assets it generated."), *OutAssetPath, *ExistingObject->GetClass()->GetName()),
			TEXT("Rename the spec, pick another output directory, or move the existing asset out of the way."));
		return false;
	}

	if (ExistingWidgetBlueprint->ParentClass != ParentClass)
	{
		AddError(
			OutResult,
			TEXT("TARGET_PARENT_CLASS_MISMATCH"),
			TEXT("kind"),
			FString::Printf(TEXT("'%s' derives from '%s' but kind '%s' needs '%s'; an in-place update cannot reparent it."),
				*OutAssetPath,
				ExistingWidgetBlueprint->ParentClass ? *ExistingWidgetBlueprint->ParentClass->GetName() : TEXT("None"),
				*Kind,
				*ParentClass->GetName()),
			TEXT("Delete the generated asset (or rename the spec) to change its kind."));
		return false;
	}

	if (GEditor && GEditor->IsPlaySessionInProgress())
	{
		AddError(
			OutResult,
			TEXT("TARGET_UPDATE_REQUIRES_PIE_OFF"),
			TEXT("$"),
			FString::Printf(TEXT("'%s' cannot be rebuilt in place while a PIE session is running."), *OutAssetPath),
			TEXT("Stop PIE, then compile again."));
		return false;
	}

	const FString PackageFile = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().FileExists(*PackageFile) && IFileManager::Get().IsReadOnly(*PackageFile))
	{
		AddError(
			OutResult,
			TEXT("TARGET_READ_ONLY"),
			TEXT("$"),
			FString::Printf(TEXT("'%s' is read-only on disk; the compiler does no source-control checkout."), *FPaths::ConvertRelativePathToFull(PackageFile)),
			TEXT("Open the file for edit in source control (p4 edit), then compile again."));
		return false;
	}

	OutExistingWidgetBlueprint = ExistingWidgetBlueprint;
	return true;
}

void UUESpecCompilerSubsystem::DescribeViewModelClass(TSubclassOf<UUESpecViewModelBase> ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods) const
{
	UUESpecViewModelBase::DescribeViewModelClass(ViewModelClass, OutMethods);
}

bool UUESpecCompilerSubsystem::ParseJsonTextToObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutRootObject)
{
	OutRootObject.Reset();

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	return FJsonSerializer::Deserialize(Reader, OutRootObject) && OutRootObject.IsValid();
}

bool UUESpecCompilerSubsystem::NormalizeJsonObject(TSharedPtr<FJsonObject> JsonObject, FString& OutNormalizedJson, FUESpecCompilerError& OutError)
{
	if (!JsonObject.IsValid())
	{
		OutError.Code = TEXT("INVALID_ROOT");
		OutError.Path = TEXT("$");
		OutError.Message = TEXT("UISpec root must be a JSON object.");
		OutError.Hint = TEXT("Ensure the document starts with a root object.");
		return false;
	}

	bool bChanged = false;
	NormalizeObject(JsonObject, FString(), bChanged);
	OutNormalizedJson = UE::UESpec::Editor::Private::SerializeJsonObject(JsonObject);
	return true;
}

void UUESpecCompilerSubsystem::NormalizeValue(const TSharedPtr<FJsonValue>& JsonValue, const FString& ParentKey, bool& bChanged)
{
	if (!JsonValue.IsValid())
	{
		return;
	}

	if (JsonValue->Type == EJson::Object)
	{
		NormalizeObject(JsonValue->AsObject(), ParentKey, bChanged);
		return;
	}

	if (JsonValue->Type == EJson::Array)
	{
		for (const TSharedPtr<FJsonValue>& ArrayValue : JsonValue->AsArray())
		{
			NormalizeValue(ArrayValue, ParentKey, bChanged);
		}
	}
}

void UUESpecCompilerSubsystem::NormalizeObject(TSharedPtr<FJsonObject> JsonObject, const FString& ParentKey, bool& bChanged)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	TMap<FString, TSharedPtr<FJsonValue>> NormalizedValues;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*JsonObject))
	{
		const FString NormalizedKey = NormalizePropertyKeyForContext(ParentKey, Pair.Key);
		bChanged |= NormalizedKey != Pair.Key;

		const FString ChildContextKey = ParentKey.Equals(TEXT("stateBindings"), ESearchCase::IgnoreCase) ? TEXT("style") : NormalizedKey;
		NormalizeValue(Pair.Value, ChildContextKey, bChanged);
		NormalizedValues.Add(NormalizedKey, Pair.Value);
	}

	JsonObject->Values.Reset();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : NormalizedValues)
	{
		JsonObject->SetField(Pair.Key, Pair.Value);
	}
}

void UUESpecCompilerSubsystem::CollectVmActionReferences(const TSharedPtr<FJsonValue>& JsonValue, const FString& CurrentPath, TMap<FString, FString>& OutReferences)
{
	if (!JsonValue.IsValid())
	{
		return;
	}

	if (JsonValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*JsonObject))
		{
			const FString ChildPath = CurrentPath.IsEmpty() ? Pair.Key : CurrentPath + TEXT(".") + Pair.Key;
			if (Pair.Key == TEXT("do") && Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
			{
				const FString ActionString = Pair.Value->AsString();
				const FRegexPattern VmActionPattern(TEXT("^vm\\.([A-Z][A-Za-z0-9_]*)\\(.*\\)$"));
				FRegexMatcher Matcher(VmActionPattern, ActionString);
				if (Matcher.FindNext())
				{
					OutReferences.Add(Matcher.GetCaptureGroup(1), ChildPath);
				}
			}

			CollectVmActionReferences(Pair.Value, ChildPath, OutReferences);
		}
		return;
	}

	if (JsonValue->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonValue->AsArray();
		for (int32 Index = 0; Index < JsonArray.Num(); ++Index)
		{
			CollectVmActionReferences(JsonArray[Index], FString::Printf(TEXT("%s[%d]"), *CurrentPath, Index), OutReferences);
		}
	}
}

void UUESpecCompilerSubsystem::ExtractDeclaredViewModelMethods(const TSharedPtr<FJsonObject>& RootObject, TMap<FString, FString>& OutMethodPaths)
{
	OutMethodPaths.Reset();

	TSharedPtr<FJsonObject> ViewModelObject;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("viewModel"), ViewModelObject))
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* MethodsArray = nullptr;
	if (!ViewModelObject->TryGetArrayField(TEXT("methods"), MethodsArray) || !MethodsArray)
	{
		return;
	}

	for (int32 Index = 0; Index < MethodsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* MethodObject = nullptr;
		if (!(*MethodsArray)[Index].IsValid() || !(*MethodsArray)[Index]->TryGetObject(MethodObject) || !MethodObject || !MethodObject->IsValid())
		{
			continue;
		}

		FString MethodName;
		if ((*MethodObject)->TryGetStringField(TEXT("name"), MethodName) && !MethodName.IsEmpty())
		{
			OutMethodPaths.Add(MethodName, FString::Printf(TEXT("viewModel.methods[%d].name"), Index));
		}
	}
}

UClass* UUESpecCompilerSubsystem::ResolveViewModelClass(const FString& ViewModelClassName)
{
	if (ViewModelClassName.IsEmpty())
	{
		return nullptr;
	}

	if (ViewModelClassName.Contains(TEXT("/")) || ViewModelClassName.Contains(TEXT(".")))
	{
		if (UClass* LoadedByPath = LoadObject<UClass>(nullptr, *ViewModelClassName))
		{
			return LoadedByPath;
		}
	}

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Candidate = *ClassIt;
		if (Candidate->GetName().Equals(ViewModelClassName, ESearchCase::CaseSensitive) ||
			Candidate->GetPathName().Equals(ViewModelClassName, ESearchCase::CaseSensitive))
		{
			return Candidate;
		}
	}

	if (UClass* const AngelscriptClass = UE::UESpec::Angelscript::ResolveClassByAngelscriptName(ViewModelClassName))
	{
		return AngelscriptClass;
	}

	return nullptr;
}

UClass* UUESpecCompilerSubsystem::ResolveWidgetBlueprintParentClass(const FString& Kind)
{
	if (Kind == TEXT("HUD") || Kind == TEXT("Menu") || Kind == TEXT("Popup") || Kind == TEXT("Modal") || Kind == TEXT("Debug"))
	{
		return UUESpecActivatableWidgetBase::StaticClass();
	}

	if (Kind == TEXT("Component") || Kind == TEXT("WorldSpace"))
	{
		return UUESpecWidgetBase::StaticClass();
	}

	return nullptr;
}

UClass* UUESpecCompilerSubsystem::ResolveWidgetClass(const FString& WidgetType, TArray<FString>& OutCandidates)
{
	OutCandidates.Reset();

	if (WidgetType.IsEmpty())
	{
		return nullptr;
	}

	auto AddCandidate = [&OutCandidates](const FString& Candidate)
	{
		if (!Candidate.IsEmpty())
		{
			OutCandidates.AddUnique(Candidate);
		}
	};

	const UUESpecComponentRegistry& Registry = UUESpecComponentRegistry::Get();
	TArray<FUESpecComponentRegistration> EffectiveComponents;
	Registry.GetEffectiveComponents(EffectiveComponents);
	for (const FUESpecComponentRegistration& Registration : EffectiveComponents)
	{
		AddCandidate(Registration.Type);

		if (Registration.Type.Equals(WidgetType, ESearchCase::CaseSensitive))
		{
			if (UClass* RegisteredClass = Registration.WidgetClass.LoadSynchronous())
			{
				return RegisteredClass->IsChildOf(UWidget::StaticClass()) ? RegisteredClass : nullptr;
			}
		}
	}

	if (WidgetType.Contains(TEXT("/")) || WidgetType.Contains(TEXT(".")))
	{
		if (UClass* LoadedByPath = LoadObject<UClass>(nullptr, *WidgetType))
		{
			if (LoadedByPath->IsChildOf(UWidget::StaticClass()))
			{
				return LoadedByPath;
			}
		}
	}

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Candidate = *ClassIt;
		if (!Candidate->IsChildOf(UWidget::StaticClass()) || Candidate->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		AddCandidate(Candidate->GetName());

		if (Candidate->GetName().Equals(WidgetType, ESearchCase::CaseSensitive) ||
			Candidate->GetPathName().Equals(WidgetType, ESearchCase::CaseSensitive))
		{
			return Candidate;
		}
	}

	if (GEngine)
	{
		if (UUESpecSupportSurfaceSubsystem* SurfaceSubsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
		{
			const FUESpecSupportSurfaceSnapshot Snapshot = SurfaceSubsystem->BuildSnapshot();

			for (const FUESpecSurfaceWidgetEntry& Entry : Snapshot.BuiltInUMGWidgets)
			{
				AddCandidate(Entry.Type);
			}
			for (const FUESpecSurfaceWidgetEntry& Entry : Snapshot.CommonUIWidgets)
			{
				AddCandidate(Entry.Type);
			}
			for (const FUESpecSurfaceWidgetEntry& Entry : Snapshot.RegisteredComponents)
			{
				AddCandidate(Entry.Type);
			}
		}
	}

	return nullptr;
}

UWidgetBlueprint* UUESpecCompilerSubsystem::CreateWidgetBlueprintAsset(const FString& SpecName, const FString& Kind, const FString& OutputDirectory, FString& OutAssetPath, FUESpecCompilerResult& OutResult)
{
	const FString PackageDirectory = OutputDirectory.IsEmpty() ? FString(UE::UESpec::Editor::Private::DefaultOutputDirectory) : OutputDirectory;
	if (!FPackageName::IsValidLongPackageName(PackageDirectory))
	{
		AddError(
			OutResult,
			TEXT("INVALID_OUTPUT_DIRECTORY"),
			TEXT("$"),
			FString::Printf(TEXT("Output directory '%s' is not a valid long package path."), *PackageDirectory),
			TEXT("Use a package path such as /Game/UI/Generated."));
		return nullptr;
	}

	UClass* ParentClass = ResolveWidgetBlueprintParentClass(Kind);
	if (!ParentClass)
	{
		AddError(
			OutResult,
			TEXT("UNSUPPORTED_WIDGET_KIND"),
			TEXT("kind"),
			FString::Printf(TEXT("kind '%s' does not map to a WidgetBlueprint base class yet."), *Kind),
			TEXT("Use HUD/Menu/Popup/Modal/Debug/WorldSpace/Component for the current compiler."));
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// HF: deterministic spec->asset identity. The asset is always <OutputDirectory>/WBP_<name> (no CreateUniqueAssetName
	// _1/_2 variants); CompileJsonFileToWidgetBlueprint resolves an existing compiler-owned asset first and updates it in place.
	const FString AssetName = UE::UESpec::Editor::Private::MakeGeneratedAssetName(SpecName);
	const FString PackageName = UE::UESpec::Editor::Private::MakePackagePath(PackageDirectory, AssetName);

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(
		AssetToolsModule.Get().CreateAsset(AssetName, PackageDirectory, UWidgetBlueprint::StaticClass(), Factory));

	if (!WidgetBlueprint)
	{
		AddError(
			OutResult,
			TEXT("WIDGET_BLUEPRINT_CREATE_FAILED"),
			TEXT("$"),
			FString::Printf(TEXT("Failed to create WidgetBlueprint '%s'."), *AssetName),
			TEXT("Check the parent class mapping and package path."));
		return nullptr;
	}

	OutAssetPath = PackageName + TEXT(".") + AssetName;
	return WidgetBlueprint;
}

bool UUESpecCompilerSubsystem::BuildWidgetBlueprintTree(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	TSharedPtr<FJsonObject> RootWidgetNode;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("root"), RootWidgetNode))
	{
		AddError(
			OutResult,
			TEXT("MISSING_ROOT"),
			TEXT("root"),
			TEXT("Non-test UISpec documents must declare root."),
			TEXT("Add a root widget node."));
		return false;
	}

	TMap<FString, TSharedPtr<FJsonValue>> LocalStyleTokens;
	TMap<FString, FString> GlobalStyleTokens;
	CollectLocalStyleTokens(RootObject, LocalStyleTokens);
	CollectGlobalStyleTokens(GlobalStyleTokens);

	UWidget* RootWidget = BuildWidgetNode(WidgetBlueprint, RootWidgetNode, TEXT("root"), LocalStyleTokens, GlobalStyleTokens, OutResult);
	if (!RootWidget)
	{
		return false;
	}

	WidgetBlueprint.Modify();
	WidgetBlueprint.WidgetTree->RootWidget = RootWidget;
	// MarkBlueprintAsStructurallyModified compiles: bring the variable GUID map in step with the new tree first.
	UE::UESpec::Editor::Private::ReconcileWidgetVariableGuids(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
	return OutResult.Errors.Num() == 0;
}

UWidget* UUESpecCompilerSubsystem::BuildWidgetNode(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult)
{
	if (!NodeObject.IsValid())
	{
		AddError(
			OutResult,
			TEXT("INVALID_WIDGET_NODE"),
			NodePath,
			TEXT("Widget node must be a JSON object."),
			TEXT("Replace this node with a valid widget object."));
		return nullptr;
	}

	FString WidgetType;
	NodeObject->TryGetStringField(TEXT("type"), WidgetType);

	FString WidgetId;
	NodeObject->TryGetStringField(TEXT("id"), WidgetId);

	if (WidgetType.IsEmpty() || WidgetId.IsEmpty())
	{
		AddError(
			OutResult,
			TEXT("WIDGET_NODE_MISSING_TYPE_OR_ID"),
			NodePath,
			TEXT("Widget nodes must declare both type and id."),
			TEXT("Add type and id to the widget node."));
		return nullptr;
	}

	TArray<FString> Candidates;
	UClass* WidgetClass = ResolveWidgetClass(WidgetType, Candidates);
	if (!WidgetClass)
	{
		AddError(
			OutResult,
			TEXT("UNKNOWN_WIDGET_TYPE"),
			UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("type")),
			FString::Printf(TEXT("Widget type '%s' is not available in the current support surface."), *WidgetType),
			TEXT("Use a built-in UMG/CommonUI widget or register the component first."),
			BuildNearestSuggestions(WidgetType, Candidates));
		return nullptr;
	}

	if (!UE::UESpec::Editor::Private::IsConstructibleWidgetClass(WidgetClass))
	{
		AddError(
			OutResult,
			TEXT("WIDGET_CLASS_NOT_CONSTRUCTIBLE"),
			UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("type")),
			FString::Printf(TEXT("Widget class '%s' for type '%s' is abstract or deprecated and cannot be instantiated."), *WidgetClass->GetPathName(), *WidgetType),
			TEXT("Register a concrete widget class for this type before compiling."));
		return nullptr;
	}

	UWidget* Widget = WidgetBlueprint.WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetId));
	if (!Widget)
	{
		AddError(
			OutResult,
			TEXT("WIDGET_CONSTRUCT_FAILED"),
			NodePath,
			FString::Printf(TEXT("Failed to construct widget '%s' of type '%s'."), *WidgetId, *WidgetType),
			TEXT("Check the widget class and its constructor requirements."));
		return nullptr;
	}

	Widget->bIsVariable = true;
	// An in-place update keeps the previous GUID of a same-named widget (OnVariableAdded ensures on an existing entry).
	if (!WidgetBlueprint.WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
	{
		WidgetBlueprint.OnVariableAdded(Widget->GetFName());
	}
	ApplyWidgetProperties(*Widget, NodeObject, NodePath, LocalStyleTokens, GlobalStyleTokens, OutResult);

	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
	if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
	{
		return Widget;
	}

	for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
	{
		const TSharedPtr<FJsonObject>* ChildObject = nullptr;
		if (!(*ChildrenArray)[ChildIndex].IsValid() || !(*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) || !ChildObject || !ChildObject->IsValid())
		{
			AddError(
				OutResult,
				TEXT("INVALID_WIDGET_CHILD"),
				UE::UESpec::Editor::Private::MakeArrayPath(UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("children")), ChildIndex),
				TEXT("children entries must be widget objects."),
				TEXT("Replace the child entry with a valid widget node."));
			continue;
		}

		const FString ChildPath = UE::UESpec::Editor::Private::MakeArrayPath(UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("children")), ChildIndex);
		UWidget* ChildWidget = BuildWidgetNode(WidgetBlueprint, *ChildObject, ChildPath, LocalStyleTokens, GlobalStyleTokens, OutResult);
		if (!ChildWidget)
		{
			continue;
		}

		if (!AttachChildWidget(*Widget, *ChildWidget, *ChildObject, ChildPath, LocalStyleTokens, GlobalStyleTokens, OutResult))
		{
			return Widget;
		}
	}

	return Widget;
}

bool UUESpecCompilerSubsystem::AttachChildWidget(UWidget& ParentWidget, UWidget& ChildWidget, const TSharedPtr<FJsonObject>& ChildNodeObject, const FString& ChildPath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult)
{
	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(&ParentWidget))
	{
		if (UPanelSlot* PanelSlot = PanelWidget->AddChild(&ChildWidget))
		{
			TSharedPtr<FJsonObject> SlotObject;
			if (UE::UESpec::Editor::Private::TryGetObjectField(ChildNodeObject, TEXT("slot"), SlotObject))
			{
				ApplySlotProperties(*PanelSlot, SlotObject, UE::UESpec::Editor::Private::MakeChildPath(ChildPath, TEXT("slot")), LocalStyleTokens, GlobalStyleTokens, OutResult);
			}

			return true;
		}
	}
	else if (UContentWidget* ContentWidget = Cast<UContentWidget>(&ParentWidget))
	{
		if (ContentWidget->GetChildrenCount() > 0)
		{
			AddError(
				OutResult,
				TEXT("CONTENT_WIDGET_ALREADY_HAS_CHILD"),
				ChildPath,
				FString::Printf(TEXT("Widget '%s' accepts only one child."), *ParentWidget.GetName()),
				TEXT("Use a panel widget if you need multiple children."));
			return false;
		}

		if (UPanelSlot* PanelSlot = ContentWidget->SetContent(&ChildWidget))
		{
			TSharedPtr<FJsonObject> SlotObject;
			if (UE::UESpec::Editor::Private::TryGetObjectField(ChildNodeObject, TEXT("slot"), SlotObject))
			{
				ApplySlotProperties(*PanelSlot, SlotObject, UE::UESpec::Editor::Private::MakeChildPath(ChildPath, TEXT("slot")), LocalStyleTokens, GlobalStyleTokens, OutResult);
			}

			return true;
		}
	}

	AddError(
		OutResult,
		TEXT("WIDGET_DOES_NOT_ACCEPT_CHILDREN"),
		ChildPath,
		FString::Printf(TEXT("Widget '%s' does not accept declarative children yet."), *ParentWidget.GetClass()->GetName()),
		TEXT("Use a panel/content widget parent, or register a component compiler for this type."));
	return false;
}

void UUESpecCompilerSubsystem::ApplyWidgetProperties(UWidget& Widget, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult)
{
	const TSharedPtr<FJsonValue>* VisibilityValue = NodeObject->Values.Find(TEXT("visibility"));
	if (VisibilityValue && VisibilityValue->IsValid() && (*VisibilityValue)->Type == EJson::String)
	{
		ESlateVisibility Visibility = ESlateVisibility::Visible;
		if (UE::UESpec::Editor::Private::ParseVisibility((*VisibilityValue)->AsString(), Visibility))
		{
			Widget.SetVisibility(Visibility);
		}
	}

	TSharedPtr<FJsonObject> StyleObject;
	if (UE::UESpec::Editor::Private::TryGetObjectField(NodeObject, TEXT("style"), StyleObject))
	{
		if (const TSharedPtr<FJsonValue>* OpacityValue = StyleObject->Values.Find(TEXT("Opacity")))
		{
			float Opacity = 1.0f;
			if (TryResolveStyleNumber(*OpacityValue, LocalStyleTokens, GlobalStyleTokens, Opacity))
			{
				Widget.SetRenderOpacity(Opacity);
			}
		}
	}

	if (UTextBlock* TextBlock = Cast<UTextBlock>(&Widget))
	{
		const TSharedPtr<FJsonValue>* ContentValue = NodeObject->Values.Find(TEXT("content"));
		if (ContentValue && ContentValue->IsValid())
		{
			const FString ContentString = ExtractLiteralContentString(*ContentValue);
			if (!ContentString.IsEmpty() || (*ContentValue)->Type == EJson::String)
			{
				TextBlock->SetText(FText::FromString(ContentString));
			}
		}

		if (StyleObject.IsValid())
		{
			if (const TSharedPtr<FJsonValue>* FontSizeValue = StyleObject->Values.Find(TEXT("FontSize")))
			{
				float FontSize = 0.0f;
				if (TryResolveStyleNumber(*FontSizeValue, LocalStyleTokens, GlobalStyleTokens, FontSize))
				{
					FSlateFontInfo FontInfo = TextBlock->GetFont();
					FontInfo.Size = FMath::RoundToInt(FontSize);
					TextBlock->SetFont(FontInfo);
				}
			}

			if (const TSharedPtr<FJsonValue>* FontColorValue = StyleObject->Values.Find(TEXT("FontColor")))
			{
				FLinearColor FontColor;
				if (TryResolveStyleColor(*FontColorValue, LocalStyleTokens, GlobalStyleTokens, FontColor))
				{
					TextBlock->SetColorAndOpacity(FSlateColor(FontColor));
				}
			}
		}
	}
	else if (UProgressBar* ProgressBar = Cast<UProgressBar>(&Widget))
	{
		const TSharedPtr<FJsonValue>* ContentValue = NodeObject->Values.Find(TEXT("content"));
		if (ContentValue && ContentValue->IsValid())
		{
			float Percent = 0.0f;
			bool bHasPercent = false;

			if ((*ContentValue)->Type == EJson::Number)
			{
				Percent = static_cast<float>((*ContentValue)->AsNumber());
				bHasPercent = true;
			}
			else
			{
				const FString PercentString = ExtractLiteralContentString(*ContentValue);
				bHasPercent = !PercentString.IsEmpty() && LexTryParseString(Percent, *PercentString);
			}

			if (bHasPercent)
			{
				ProgressBar->SetPercent(Percent);
			}
		}

		if (StyleObject.IsValid())
		{
			if (const TSharedPtr<FJsonValue>* FillColorValue = StyleObject->Values.Find(TEXT("FillColor")))
			{
				FLinearColor FillColor;
				if (TryResolveStyleColor(*FillColorValue, LocalStyleTokens, GlobalStyleTokens, FillColor))
				{
					ProgressBar->SetFillColorAndOpacity(FillColor);
				}
			}
		}
	}
	else if (UImage* Image = Cast<UImage>(&Widget))
	{
		const TSharedPtr<FJsonValue>* ContentValue = NodeObject->Values.Find(TEXT("content"));
		if (ContentValue && ContentValue->IsValid())
		{
			const FString ResourcePath = ExtractLiteralContentString(*ContentValue);
			if (!ResourcePath.IsEmpty() && (ResourcePath.Contains(TEXT("/")) || ResourcePath.Contains(TEXT("."))))
			{
				if (UObject* ResourceObject = LoadObject<UObject>(nullptr, *ResourcePath))
				{
					FSlateBrush Brush = Image->GetBrush();
					Brush.SetResourceObject(ResourceObject);
					Image->SetBrush(Brush);
				}
			}
		}

		if (StyleObject.IsValid())
		{
			if (const TSharedPtr<FJsonValue>* TintValue = StyleObject->Values.Find(TEXT("TintColor")))
			{
				FLinearColor TintColor;
				if (TryResolveStyleColor(*TintValue, LocalStyleTokens, GlobalStyleTokens, TintColor))
				{
					Image->SetColorAndOpacity(TintColor);
				}
			}
		}
	}
}

void UUESpecCompilerSubsystem::ApplySlotProperties(UPanelSlot& PanelSlot, const TSharedPtr<FJsonObject>& SlotObject, const FString& SlotPath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult)
{
	if (!SlotObject.IsValid())
	{
		return;
	}

	FMargin Padding;
	const bool bHasPadding = UE::UESpec::Editor::Private::ParseMargin(SlotObject, TEXT("Padding"), Padding);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(&PanelSlot))
	{
		FString AnchorsName;
		if (SlotObject->TryGetStringField(TEXT("Anchors"), AnchorsName))
		{
			FAnchors Anchors;
			if (UE::UESpec::Editor::Private::ParseAnchors(AnchorsName, Anchors))
			{
				CanvasSlot->SetAnchors(Anchors);
			}
		}

		FVector2D Alignment;
		if (UE::UESpec::Editor::Private::ParseVector2(SlotObject, TEXT("Alignment"), Alignment))
		{
			CanvasSlot->SetAlignment(Alignment);
		}

		TArray<float> OffsetValues;
		if (UE::UESpec::Editor::Private::ParseNumberArray(SlotObject, TEXT("Offset"), OffsetValues))
		{
			if (OffsetValues.Num() == 2)
			{
				CanvasSlot->SetPosition(FVector2D(OffsetValues[0], OffsetValues[1]));
			}
			else if (OffsetValues.Num() == 4)
			{
				CanvasSlot->SetOffsets(FMargin(OffsetValues[0], OffsetValues[1], OffsetValues[2], OffsetValues[3]));
			}
		}

		FVector2D Size;
		if (UE::UESpec::Editor::Private::ParseVector2(SlotObject, TEXT("Size"), Size))
		{
			CanvasSlot->SetSize(Size);
		}

		float ZOrder = 0.0f;
		if (const TSharedPtr<FJsonValue>* ZOrderValue = SlotObject->Values.Find(TEXT("ZOrder")))
		{
			if (TryResolveStyleNumber(*ZOrderValue, LocalStyleTokens, GlobalStyleTokens, ZOrder))
			{
				CanvasSlot->SetZOrder(FMath::RoundToInt(ZOrder));
			}
		}
	}
	else if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(&PanelSlot))
	{
		if (bHasPadding)
		{
			HorizontalBoxSlot->SetPadding(Padding);
		}

		FString HAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("HorizontalAlignment"), HAlignValue) || SlotObject->TryGetStringField(TEXT("HAlign"), HAlignValue)))
		{
			EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseHorizontalAlignment(HAlignValue, HorizontalAlignment))
			{
				HorizontalBoxSlot->SetHorizontalAlignment(HorizontalAlignment);
			}
		}

		FString VAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("VerticalAlignment"), VAlignValue) || SlotObject->TryGetStringField(TEXT("VAlign"), VAlignValue)))
		{
			EVerticalAlignment VerticalAlignment = VAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseVerticalAlignment(VAlignValue, VerticalAlignment))
			{
				HorizontalBoxSlot->SetVerticalAlignment(VerticalAlignment);
			}
		}
	}
	else if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(&PanelSlot))
	{
		if (bHasPadding)
		{
			VerticalBoxSlot->SetPadding(Padding);
		}

		FString HAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("HorizontalAlignment"), HAlignValue) || SlotObject->TryGetStringField(TEXT("HAlign"), HAlignValue)))
		{
			EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseHorizontalAlignment(HAlignValue, HorizontalAlignment))
			{
				VerticalBoxSlot->SetHorizontalAlignment(HorizontalAlignment);
			}
		}

		FString VAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("VerticalAlignment"), VAlignValue) || SlotObject->TryGetStringField(TEXT("VAlign"), VAlignValue)))
		{
			EVerticalAlignment VerticalAlignment = VAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseVerticalAlignment(VAlignValue, VerticalAlignment))
			{
				VerticalBoxSlot->SetVerticalAlignment(VerticalAlignment);
			}
		}
	}
	else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(&PanelSlot))
	{
		if (bHasPadding)
		{
			OverlaySlot->SetPadding(Padding);
		}

		FString HAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("HorizontalAlignment"), HAlignValue) || SlotObject->TryGetStringField(TEXT("HAlign"), HAlignValue)))
		{
			EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseHorizontalAlignment(HAlignValue, HorizontalAlignment))
			{
				OverlaySlot->SetHorizontalAlignment(HorizontalAlignment);
			}
		}

		FString VAlignValue;
		if ((SlotObject->TryGetStringField(TEXT("VerticalAlignment"), VAlignValue) || SlotObject->TryGetStringField(TEXT("VAlign"), VAlignValue)))
		{
			EVerticalAlignment VerticalAlignment = VAlign_Fill;
			if (UE::UESpec::Editor::Private::ParseVerticalAlignment(VAlignValue, VerticalAlignment))
			{
				OverlaySlot->SetVerticalAlignment(VerticalAlignment);
			}
		}
	}
	else if (bHasPadding)
	{
		AddError(
			OutResult,
			TEXT("UNSUPPORTED_SLOT_TYPE"),
			SlotPath,
			FString::Printf(TEXT("Slot type '%s' does not support compiler-authored layout yet."), *PanelSlot.GetClass()->GetName()),
			TEXT("Use CanvasPanel/HorizontalBox/VerticalBox/Overlay slots for the current compiler."));
	}
}

void UUESpecCompilerSubsystem::CollectLocalStyleTokens(const TSharedPtr<FJsonObject>& RootObject, TMap<FString, TSharedPtr<FJsonValue>>& OutLocalStyleTokens)
{
	OutLocalStyleTokens.Reset();

	TSharedPtr<FJsonObject> StyleTokensObject;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("styleTokens"), StyleTokensObject))
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*StyleTokensObject))
	{
		OutLocalStyleTokens.Add(Pair.Key, Pair.Value);
	}
}

void UUESpecCompilerSubsystem::CollectGlobalStyleTokens(TMap<FString, FString>& OutGlobalStyleTokens)
{
	OutGlobalStyleTokens.Reset();

	if (!GEngine)
	{
		return;
	}

	if (UUESpecSupportSurfaceSubsystem* SurfaceSubsystem = GEngine->GetEngineSubsystem<UUESpecSupportSurfaceSubsystem>())
	{
		const FUESpecSupportSurfaceSnapshot Snapshot = SurfaceSubsystem->BuildSnapshot();
		for (const FUESpecSurfaceStyleTokenEntry& Entry : Snapshot.StyleTokens)
		{
			OutGlobalStyleTokens.Add(Entry.Name, Entry.Value);
			if (Entry.Name.StartsWith(TEXT("@")))
			{
				OutGlobalStyleTokens.Add(Entry.Name.RightChop(1), Entry.Value);
			}
		}
	}
}

TSharedPtr<FJsonValue> UUESpecCompilerSubsystem::ResolveStyleValue(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens)
{
	if (!RawValue.IsValid() || RawValue->Type != EJson::String)
	{
		return RawValue;
	}

	const FString RawString = RawValue->AsString();
	if (!RawString.StartsWith(TEXT("@")))
	{
		return RawValue;
	}

	const FString TokenName = RawString.RightChop(1);
	if (const TSharedPtr<FJsonValue>* LocalToken = LocalStyleTokens.Find(TokenName))
	{
		return *LocalToken;
	}

	if (const FString* GlobalTokenWithPrefix = GlobalStyleTokens.Find(RawString))
	{
		return MakeShared<FJsonValueString>(*GlobalTokenWithPrefix);
	}

	if (const FString* GlobalToken = GlobalStyleTokens.Find(TokenName))
	{
		return MakeShared<FJsonValueString>(*GlobalToken);
	}

	return RawValue;
}

bool UUESpecCompilerSubsystem::TryResolveStyleNumber(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, float& OutNumber)
{
	const TSharedPtr<FJsonValue> ResolvedValue = ResolveStyleValue(RawValue, LocalStyleTokens, GlobalStyleTokens);
	if (!ResolvedValue.IsValid())
	{
		return false;
	}

	if (ResolvedValue->Type == EJson::Number)
	{
		OutNumber = static_cast<float>(ResolvedValue->AsNumber());
		return true;
	}

	if (ResolvedValue->Type == EJson::String)
	{
		return LexTryParseString(OutNumber, *ResolvedValue->AsString());
	}

	return false;
}

bool UUESpecCompilerSubsystem::TryResolveStyleColor(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FLinearColor& OutColor)
{
	FString RawString;
	if (!TryResolveStringLikeValue(RawValue, LocalStyleTokens, GlobalStyleTokens, RawString))
	{
		return false;
	}

	FString ColorString = RawString;
	ColorString.RemoveFromStart(TEXT("#"));

	if (ColorString.Len() == 6 || ColorString.Len() == 8)
	{
		OutColor = FLinearColor(FColor::FromHex(ColorString));
		return true;
	}

	return OutColor.InitFromString(RawString);
}

bool UUESpecCompilerSubsystem::TryResolveStringLikeValue(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FString& OutString)
{
	const TSharedPtr<FJsonValue> ResolvedValue = ResolveStyleValue(RawValue, LocalStyleTokens, GlobalStyleTokens);
	if (!ResolvedValue.IsValid())
	{
		return false;
	}

	if (ResolvedValue->Type == EJson::String)
	{
		OutString = ResolvedValue->AsString();
		return true;
	}

	if (ResolvedValue->Type == EJson::Number)
	{
		OutString = FString::SanitizeFloat(ResolvedValue->AsNumber());
		return true;
	}

	return false;
}

FString UUESpecCompilerSubsystem::ExtractLiteralContentString(const TSharedPtr<FJsonValue>& ContentValue)
{
	if (!ContentValue.IsValid())
	{
		return FString();
	}

	if (ContentValue->Type == EJson::String)
	{
		return ContentValue->AsString();
	}

	if (ContentValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> BindingObject = ContentValue->AsObject();
		FString FallbackValue;
		if (BindingObject.IsValid() && BindingObject->TryGetStringField(TEXT("fallback"), FallbackValue))
		{
			return FallbackValue;
		}
	}

	return FString();
}

bool UUESpecCompilerSubsystem::SaveWidgetBlueprintAsset(UWidgetBlueprint& WidgetBlueprint, const FString& AssetPath, FUESpecCompilerResult& OutResult)
{
	FKismetEditorUtilities::CompileBlueprint(&WidgetBlueprint);
	if (WidgetBlueprint.Status == BS_Error || !WidgetBlueprint.IsUpToDate())
	{
		AddError(
			OutResult,
			TEXT("WIDGET_BLUEPRINT_COMPILE_FAILED"),
			TEXT("$"),
			FString::Printf(TEXT("WidgetBlueprint '%s' failed to compile cleanly."), *AssetPath),
			TEXT("Fix the generated graph/tree errors before saving this asset."));
		return false;
	}

	WidgetBlueprint.MarkPackageDirty();

	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;

	if (!UPackage::SavePackage(WidgetBlueprint.GetPackage(), &WidgetBlueprint, *PackageFileName, SaveArgs))
	{
		AddError(
			OutResult,
			TEXT("SAVE_WIDGET_BLUEPRINT_FAILED"),
			TEXT("$"),
			FString::Printf(TEXT("Failed to save WidgetBlueprint '%s'."), *AssetPath),
			TEXT("Check the package path and editor save state."));
		return false;
	}

	return true;
}

bool UUESpecCompilerSubsystem::ValidateTopLevelDocument(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	FString ApiVersion;
	if (!RootObject->TryGetStringField(TEXT("apiVersion"), ApiVersion) || ApiVersion.IsEmpty())
	{
		AddError(
			OutResult,
			TEXT("MISSING_API_VERSION"),
			TEXT("apiVersion"),
			TEXT("UISpec root must declare apiVersion."),
			TEXT("Add apiVersion: \"0.2\" to the root object."));
	}
	else if (!UE::UESpec::Editor::Private::IsNumericVersionString(ApiVersion))
	{
		AddError(
			OutResult,
			TEXT("INVALID_API_VERSION"),
			TEXT("apiVersion"),
			FString::Printf(TEXT("apiVersion '%s' must match ^0\\.[0-9]+$."), *ApiVersion),
			TEXT("Use a numeric minor version such as \"0.1\" or \"0.2\"."));
	}
	else if (!UE::UESpec::Editor::Private::IsApiVersionString(ApiVersion) || !IsSupportedApiVersion(ApiVersion))
	{
		AddError(
			OutResult,
			TEXT("API_VERSION_TOO_HIGH"),
			TEXT("apiVersion"),
			FString::Printf(TEXT("apiVersion '%s' is newer than the compiler maximum '%s'."), *ApiVersion, UE::UESpec::Editor::Private::SupportedApiVersion),
			TEXT("Lower apiVersion to the highest compiler-supported version, currently \"0.2\"."));
	}

	FString Kind;
	RootObject->TryGetStringField(TEXT("kind"), Kind);
	TArray<FString> SupportedKinds;
	UE::UESpec::Editor::Private::BuildSupportedKindNames(SupportedKinds);
	if (!SupportedKinds.Contains(Kind))
	{
		AddError(
			OutResult,
			TEXT("INVALID_KIND"),
			TEXT("kind"),
			FString::Printf(TEXT("kind '%s' is not one of the supported UISpec kinds."), *Kind),
			TEXT("Use one of HUD/Menu/Popup/Modal/Debug/WorldSpace/Component/Test."),
			BuildNearestSuggestions(Kind, SupportedKinds));
	}

	FString Name;
	RootObject->TryGetStringField(TEXT("name"), Name);
	if (!UE::UESpec::Editor::Private::IsValidSpecName(Name))
	{
		AddError(
			OutResult,
			TEXT("INVALID_NAME"),
			TEXT("name"),
			FString::Printf(TEXT("name '%s' must match ^[A-Z][A-Za-z0-9_]*$."), *Name),
			TEXT("Rename the spec with a PascalCase identifier, for example SkillBarHUD."));
	}

	if (Kind == TEXT("Test"))
	{
		TSharedPtr<FJsonObject> TestSpecObject;
		if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("testSpec"), TestSpecObject))
		{
			AddError(
				OutResult,
				TEXT("MISSING_ROOT_OR_TEST_SPEC"),
				TEXT("testSpec"),
				TEXT("kind='Test' documents must declare testSpec."),
				TEXT("Add a testSpec object with target/phases."));
		}
	}
	else
	{
		TSharedPtr<FJsonObject> RootWidgetObject;
		if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("root"), RootWidgetObject))
		{
			AddError(
				OutResult,
				TEXT("MISSING_ROOT_OR_TEST_SPEC"),
				TEXT("root"),
				TEXT("Non-test UISpec documents must declare root."),
				TEXT("Add a root widget node."));
		}
	}

	return OutResult.Errors.Num() == 0;
}

void UUESpecCompilerSubsystem::ValidateWidgetTree(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	FString Kind;
	RootObject->TryGetStringField(TEXT("kind"), Kind);
	if (Kind == TEXT("Test"))
	{
		return;
	}

	TSharedPtr<FJsonObject> RootWidgetNode;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("root"), RootWidgetNode))
	{
		return;
	}

	FString TopLevelApiVersion;
	RootObject->TryGetStringField(TEXT("apiVersion"), TopLevelApiVersion);

	TMap<FString, FString> WidgetIdPaths;
	UE::UESpec::Editor::Private::CollectWidgetIds(RootWidgetNode, TEXT("root"), TopLevelApiVersion, WidgetIdPaths, OutResult);

	TSet<FString> StateNames;
	UE::UESpec::Editor::Private::CollectStateNames(RootObject, StateNames);

	TSet<FString> GlobalTokenNames;
	TArray<FString> StyleTokenCandidates;
	UE::UESpec::Editor::Private::BuildGlobalStyleTokenNames(GlobalTokenNames, StyleTokenCandidates);

	TSet<FString> LocalTokenNames;
	TArray<FString> LocalStyleTokenCandidates;
	UE::UESpec::Editor::Private::BuildLocalStyleTokenNames(RootObject, LocalTokenNames, LocalStyleTokenCandidates);

	for (const FString& TokenName : LocalTokenNames)
	{
		GlobalTokenNames.Add(TokenName);
	}
	for (const FString& TokenName : LocalStyleTokenCandidates)
	{
		StyleTokenCandidates.AddUnique(TokenName);
	}

	UE::UESpec::Editor::Private::ValidateWidgetNodeDetails(RootWidgetNode, TEXT("root"), WidgetIdPaths, StateNames, GlobalTokenNames, StyleTokenCandidates, OutResult);

	const TArray<TSharedPtr<FJsonValue>>* DeletionsArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("deletions"), DeletionsArray) && DeletionsArray)
	{
		for (int32 DeletionIndex = 0; DeletionIndex < DeletionsArray->Num(); ++DeletionIndex)
		{
			if (!(*DeletionsArray)[DeletionIndex].IsValid() || (*DeletionsArray)[DeletionIndex]->Type != EJson::String)
			{
				continue;
			}

			const FString DeletionId = (*DeletionsArray)[DeletionIndex]->AsString();
			if (WidgetIdPaths.Contains(DeletionId))
			{
				AddError(
					OutResult,
					TEXT("DELETION_CONFLICT"),
					UE::UESpec::Editor::Private::MakeArrayPath(TEXT("deletions"), DeletionIndex),
					FString::Printf(TEXT("deletions lists id '%s' but the new spec still declares that widget."), *DeletionId),
					TEXT("Remove the id from deletions, or remove/rename the widget in the new spec."));
			}
		}
	}

	TSet<FString> ConverterNames;
	TArray<FString> ConverterCandidates;
	UE::UESpec::Editor::Private::BuildConverterNames(ConverterNames, ConverterCandidates);
	UE::UESpec::Editor::Private::ValidateConverterReferences(MakeShared<FJsonValueObject>(RootObject), TEXT("$"), ConverterNames, ConverterCandidates, OutResult);
}

void UUESpecCompilerSubsystem::ValidateWidgetNode(const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const FString& TopLevelApiVersion, FUESpecCompilerResult& OutResult)
{
	if (!NodeObject.IsValid())
	{
		return;
	}

	FString WidgetId;
	if (NodeObject->TryGetStringField(TEXT("id"), WidgetId) && !WidgetId.IsEmpty() && !IsCamelCaseIdentifier(WidgetId))
	{
		AddError(
			OutResult,
			TEXT("INVALID_WIDGET_ID"),
			UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("id")),
			FString::Printf(TEXT("Widget id '%s' must be camelCase."), *WidgetId),
			TEXT("Rename the widget id to a camelCase identifier."));
	}

	FString NodeApiVersion;
	if (NodeObject->TryGetStringField(TEXT("apiVersion"), NodeApiVersion) && !NodeApiVersion.IsEmpty() && IsApiVersionGreaterThan(NodeApiVersion, TopLevelApiVersion))
	{
		AddError(
			OutResult,
			TEXT("SUBTREE_API_VERSION_EXCEEDS_ROOT"),
			UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("apiVersion")),
			FString::Printf(TEXT("Subtree apiVersion '%s' cannot exceed root apiVersion '%s'."), *NodeApiVersion, *TopLevelApiVersion),
			TEXT("Lower the subtree apiVersion or raise the root version within compiler support."));
	}

	const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
	if (NodeObject->TryGetArrayField(TEXT("actions"), ActionsArray) && ActionsArray)
	{
		ValidateActions(*ActionsArray, UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("actions")), OutResult);
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
	if (!NodeObject->TryGetArrayField(TEXT("children"), ChildrenArray) || !ChildrenArray)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < ChildrenArray->Num(); ++ChildIndex)
	{
		const TSharedPtr<FJsonObject>* ChildObject = nullptr;
		if ((*ChildrenArray)[ChildIndex].IsValid() && (*ChildrenArray)[ChildIndex]->TryGetObject(ChildObject) && ChildObject && ChildObject->IsValid())
		{
			ValidateWidgetNode(*ChildObject, UE::UESpec::Editor::Private::MakeArrayPath(UE::UESpec::Editor::Private::MakeChildPath(NodePath, TEXT("children")), ChildIndex), TopLevelApiVersion, OutResult);
		}
	}
}

void UUESpecCompilerSubsystem::ValidateActions(const TArray<TSharedPtr<FJsonValue>>& ActionsArray, const FString& ActionsPath, FUESpecCompilerResult& OutResult)
{
	for (int32 ActionIndex = 0; ActionIndex < ActionsArray.Num(); ++ActionIndex)
	{
		const TSharedPtr<FJsonObject>* ActionObject = nullptr;
		if (!ActionsArray[ActionIndex].IsValid() || !ActionsArray[ActionIndex]->TryGetObject(ActionObject) || !ActionObject || !ActionObject->IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonValue>* DoValue = (*ActionObject)->Values.Find(TEXT("do"));
		if (DoValue && DoValue->IsValid())
		{
			ValidateActionDoValue(*DoValue, UE::UESpec::Editor::Private::MakeChildPath(UE::UESpec::Editor::Private::MakeArrayPath(ActionsPath, ActionIndex), TEXT("do")), OutResult);
		}
	}
}

void UUESpecCompilerSubsystem::ValidateActionDoValue(const TSharedPtr<FJsonValue>& DoValue, const FString& DoPath, FUESpecCompilerResult& OutResult)
{
	if (!DoValue.IsValid())
	{
		return;
	}

	if (DoValue->Type == EJson::String)
	{
		const FString ActionString = DoValue->AsString();
		const FRegexPattern VmActionPattern(TEXT("^vm\\.[A-Z][A-Za-z0-9_]*\\(.*\\)$"));
		FRegexMatcher Matcher(VmActionPattern, ActionString);
		if (!Matcher.FindNext())
		{
			AddError(
				OutResult,
				TEXT("INVALID_VM_METHOD_REF"),
				DoPath,
				FString::Printf(TEXT("Action 'do' value '%s' is not a valid vmMethodRef."), *ActionString),
				TEXT("Use vm.MethodName(...) or an actionBlock primitive."));
		}
		return;
	}

	if (DoValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> ActionBlockObject = DoValue->AsObject();
		int32 PrimitiveCount = 0;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*ActionBlockObject))
		{
			if (UE::UESpec::Editor::Private::ActionPrimitiveKeys.Contains(Pair.Key))
			{
				++PrimitiveCount;
			}
		}

		if (PrimitiveCount == 0)
		{
			AddError(
				OutResult,
				TEXT("ACTION_BLOCK_EMPTY"),
				DoPath,
				TEXT("actionBlock must contain one primitive."),
				TEXT("Add exactly one of openLayer/closeLayer/setState/callVM/playAnim/emitSignal."));
		}
		else if (PrimitiveCount > 1)
		{
			AddError(
				OutResult,
				TEXT("ACTION_BLOCK_AMBIGUOUS"),
				DoPath,
				TEXT("actionBlock contains more than one primitive."),
				TEXT("Keep exactly one of openLayer/closeLayer/setState/callVM/playAnim/emitSignal in this block."));
		}
		return;
	}

	AddError(
		OutResult,
		TEXT("INVALID_VM_METHOD_REF"),
		DoPath,
		TEXT("Action 'do' must be either a vmMethodRef string or an actionBlock object."),
		TEXT("Replace the value with vm.MethodName(...) or a single actionBlock primitive."));
}

void UUESpecCompilerSubsystem::ValidateTestSpec(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	FString Kind;
	RootObject->TryGetStringField(TEXT("kind"), Kind);
	if (Kind != TEXT("Test"))
	{
		return;
	}

	TSharedPtr<FJsonObject> TestSpecObject;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("testSpec"), TestSpecObject))
	{
		return;
	}

	TSharedPtr<FJsonObject> PhasesObject;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(TestSpecObject, TEXT("phases"), PhasesObject))
	{
		return;
	}

	for (const FString& PhaseName : { TEXT("assume"), TEXT("arrange"), TEXT("act"), TEXT("assert") })
	{
		const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
		if (PhasesObject->TryGetArrayField(PhaseName, StepsArray) && StepsArray)
		{
			ValidateTestSteps(*StepsArray, UE::UESpec::Editor::Private::MakeChildPath(TEXT("testSpec.phases"), PhaseName), OutResult);
		}
	}
}

void UUESpecCompilerSubsystem::ValidateTestSteps(const TArray<TSharedPtr<FJsonValue>>& Steps, const FString& PhasePath, FUESpecCompilerResult& OutResult)
{
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		const TSharedPtr<FJsonObject>* StepObject = nullptr;
		if (!Steps[StepIndex].IsValid() || !Steps[StepIndex]->TryGetObject(StepObject) || !StepObject || !StepObject->IsValid())
		{
			continue;
		}

		TArray<FString> InvalidKeys;
		TArray<FString> ValidVerbKeys;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UE::UESpec::Json::GetFields(*(*StepObject)))
		{
			if (Pair.Key == TEXT("step"))
			{
				continue;
			}

			if (UE::UESpec::Editor::Private::TestVerbKeys.Contains(Pair.Key))
			{
				ValidVerbKeys.Add(Pair.Key);
			}
			else
			{
				InvalidKeys.Add(Pair.Key);
			}
		}

		const FString StepPath = UE::UESpec::Editor::Private::MakeArrayPath(PhasePath, StepIndex);
		if (InvalidKeys.Num() > 0)
		{
			AddError(
				OutResult,
				TEXT("TEST_STEP_VERB_MISSING"),
				StepPath,
				FString::Printf(TEXT("testStep contains unsupported verb '%s'."), *InvalidKeys[0]),
				TEXT("Replace the unsupported key with one whitelisted test verb."),
				BuildNearestSuggestions(InvalidKeys[0], UE::UESpec::Editor::Private::TestVerbKeys));
			continue;
		}

		if (ValidVerbKeys.Num() == 0)
		{
			AddError(
				OutResult,
				TEXT("TEST_STEP_VERB_MISSING"),
				StepPath,
				TEXT("testStep must contain exactly one verb key in addition to 'step'."),
				TEXT("Add one verb such as click/emitSignal/assertVisible."));
			continue;
		}

		if (ValidVerbKeys.Num() > 1)
		{
			AddError(
				OutResult,
				TEXT("TEST_STEP_VERB_AMBIGUOUS"),
				StepPath,
				TEXT("testStep must contain exactly one verb key."),
				TEXT("Split this step into multiple test steps."));
		}
	}
}

bool UUESpecCompilerSubsystem::IsSupportedApiVersion(const FString& ApiVersion)
{
	const int32 MinorVersion = UE::UESpec::Editor::Private::ParseApiVersionMinor(ApiVersion);
	return MinorVersion != INDEX_NONE && MinorVersion <= UE::UESpec::Editor::Private::ParseApiVersionMinor(UE::UESpec::Editor::Private::SupportedApiVersion);
}

bool UUESpecCompilerSubsystem::IsApiVersionGreaterThan(const FString& ApiVersion, const FString& MaximumApiVersion)
{
	const int32 VersionMinor = UE::UESpec::Editor::Private::ParseApiVersionMinor(ApiVersion);
	const int32 MaxMinor = UE::UESpec::Editor::Private::ParseApiVersionMinor(MaximumApiVersion);
	return VersionMinor != INDEX_NONE && MaxMinor != INDEX_NONE && VersionMinor > MaxMinor;
}

bool UUESpecCompilerSubsystem::IsCamelCaseIdentifier(const FString& Identifier)
{
	const FRegexPattern Pattern(TEXT("^[a-z][a-zA-Z0-9_]*$"));
	FRegexMatcher Matcher(Pattern, Identifier);
	return Matcher.FindNext();
}

FString UUESpecCompilerSubsystem::NormalizePropertyKeyForContext(const FString& ParentKey, const FString& OldKey)
{
	if (OldKey.IsEmpty())
	{
		return OldKey;
	}

	if (UE::UESpec::Editor::Private::IsProtocolObjectParent(ParentKey))
	{
		return OldKey;
	}

	if (ParentKey.Equals(TEXT("style"), ESearchCase::IgnoreCase) ||
		ParentKey.Equals(TEXT("slot"), ESearchCase::IgnoreCase) ||
		ParentKey.EndsWith(TEXT("Style"), ESearchCase::IgnoreCase))
	{
		return UE::UESpec::Editor::Private::ApplyUEHardcodedMapping(OldKey, UE::UESpec::Editor::Private::FirstCharUpper(OldKey));
	}

	return OldKey;
}

void UUESpecCompilerSubsystem::AddError(FUESpecCompilerResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint, TArray<FString> NearestDeclared)
{
	FUESpecCompilerError Error;
	Error.Code = Code;
	Error.Path = Path;
	Error.Message = Message;
	Error.Hint = Hint;
	Error.NearestDeclared = MoveTemp(NearestDeclared);
	OutResult.Errors.Add(MoveTemp(Error));
}

void UUESpecCompilerSubsystem::AddInfo(FUESpecCompilerResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint, TArray<FString> NearestDeclared)
{
	FUESpecCompilerError Info;
	Info.Code = Code;
	Info.Path = Path;
	Info.Message = Message;
	Info.Hint = Hint;
	Info.NearestDeclared = MoveTemp(NearestDeclared);
	OutResult.Infos.Add(MoveTemp(Info));
}

TArray<FString> UUESpecCompilerSubsystem::BuildNearestSuggestions(const FString& Needle, const TArray<FString>& Candidates)
{
	TArray<TPair<int32, FString>> RankedCandidates;
	const FString LowerNeedle = Needle.ToLower();

	for (const FString& Candidate : Candidates)
	{
		RankedCandidates.Emplace(Algo::LevenshteinDistance(LowerNeedle, Candidate.ToLower()), Candidate);
	}

	RankedCandidates.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
	{
		return A.Key == B.Key ? A.Value < B.Value : A.Key < B.Key;
	});

	TArray<FString> Suggestions;
	for (int32 Index = 0; Index < RankedCandidates.Num() && Suggestions.Num() < 3; ++Index)
	{
		Suggestions.Add(RankedCandidates[Index].Value);
	}

	return Suggestions;
}

bool UUESpecCompilerSubsystem::ValidateViewModelMethods(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult)
{
	TSharedPtr<FJsonObject> ViewModelObject;
	if (!UE::UESpec::Editor::Private::TryGetObjectField(RootObject, TEXT("viewModel"), ViewModelObject))
	{
		return true;
	}

	FString ViewModelClassName;
	if (!ViewModelObject->TryGetStringField(TEXT("class"), ViewModelClassName) || ViewModelClassName.IsEmpty())
	{
		return true;
	}

	UClass* ViewModelClass = ResolveViewModelClass(ViewModelClassName);
	if (!ViewModelClass)
	{
		AddError(
			OutResult,
			TEXT("VM_CLASS_NOT_FOUND"),
			TEXT("viewModel.class"),
			FString::Printf(TEXT("ViewModel class '%s' could not be resolved."), *ViewModelClassName),
			TEXT("Use a loaded class name or full class path."));
		return false;
	}

	if (!ViewModelClass->IsChildOf(UUESpecViewModelBase::StaticClass()))
	{
		AddError(
			OutResult,
			TEXT("VM_CLASS_WRONG_BASE"),
			TEXT("viewModel.class"),
			FString::Printf(TEXT("ViewModel class '%s' must derive from UUESpecViewModelBase."), *ViewModelClassName),
			TEXT("Rebase the class on UUESpecViewModelBase or update the spec."));
		return false;
	}

	FString BindingModeValue;
	if (ViewModelObject->TryGetStringField(TEXT("bindingMode"), BindingModeValue))
	{
		const bool bSupportedBindingMode =
			BindingModeValue.Equals(TEXT("mvvm"), ESearchCase::IgnoreCase) ||
			BindingModeValue.Equals(TEXT("manual"), ESearchCase::IgnoreCase);
		if (!bSupportedBindingMode)
		{
			AddError(
				OutResult,
				TEXT("VM_BINDING_MODE_UNSUPPORTED"),
				TEXT("viewModel.bindingMode"),
				FString::Printf(TEXT("bindingMode '%s' is not supported."), *BindingModeValue),
				TEXT("Use 'mvvm' or 'manual'."));
			return false;
		}

		if (BindingModeValue.Equals(TEXT("manual"), ESearchCase::IgnoreCase))
		{
			AddInfo(
				OutResult,
				TEXT("VM_BINDING_MODE_MANUAL"),
				TEXT("viewModel.bindingMode"),
				TEXT("ViewModel bindings will use the manual tick-driven runtime fallback."),
				TEXT("Manual mode is useful when MVVM field-notify compatibility is unavailable."));
		}
	}

	if (UE::UESpec::Angelscript::IsAngelscriptGeneratedClass(ViewModelClass))
	{
		const FString SourcePath = UE::UESpec::Angelscript::GetAngelscriptSourcePath(ViewModelClass);
		AddInfo(
			OutResult,
			TEXT("VM_CLASS_IS_ANGELSCRIPT"),
			TEXT("viewModel.class"),
			SourcePath.IsEmpty()
				? FString::Printf(TEXT("ViewModel class '%s' resolves to an AngelScript-generated class."), *ViewModelClassName)
				: FString::Printf(TEXT("ViewModel class '%s' resolves to AngelScript source '%s'."), *ViewModelClassName, *SourcePath),
			TEXT("UISpec will treat this ViewModel as an AngelScript class for reflection and runtime call routing."));
	}

	TArray<FUESpecViewModelMethodDescriptor> ReflectedMethods;
	UUESpecViewModelBase::DescribeViewModelClass(ViewModelClass, ReflectedMethods);

	TArray<FString> ReflectedNames;
	for (const FUESpecViewModelMethodDescriptor& Method : ReflectedMethods)
	{
		ReflectedNames.Add(Method.Name);
	}

	TMap<FString, FString> DeclaredMethodPaths;
	ExtractDeclaredViewModelMethods(RootObject, DeclaredMethodPaths);

	TArray<FString> DeclaredNames;
	DeclaredMethodPaths.GetKeys(DeclaredNames);

	TMap<FString, FString> VmActionReferences;
	CollectVmActionReferences(MakeShared<FJsonValueObject>(RootObject), FString(), VmActionReferences);

	for (const TPair<FString, FString>& ActionReference : VmActionReferences)
	{
		if (!DeclaredMethodPaths.Contains(ActionReference.Key))
		{
			AddError(
				OutResult,
				TEXT("VM_METHOD_NOT_DECLARED"),
				ActionReference.Value,
				FString::Printf(TEXT("vm.%s referenced but not declared in viewModel.methods."), *ActionReference.Key),
				TEXT("Add the method to viewModel.methods or remove the action."),
				BuildNearestSuggestions(ActionReference.Key, DeclaredNames.Num() > 0 ? DeclaredNames : ReflectedNames));
		}
	}

	for (const TPair<FString, FString>& DeclaredMethod : DeclaredMethodPaths)
	{
		if (!ReflectedNames.Contains(DeclaredMethod.Key))
		{
			AddError(
				OutResult,
				TEXT("VM_METHOD_NOT_REFLECTED"),
				DeclaredMethod.Value,
				FString::Printf(TEXT("Declared method '%s' was not found on ViewModel class '%s'."), *DeclaredMethod.Key, *ViewModelClassName),
				TEXT("Rename the declared method or add it to the ViewModel class."),
				BuildNearestSuggestions(DeclaredMethod.Key, ReflectedNames));
		}
	}

	return OutResult.Errors.Num() == 0;
}
