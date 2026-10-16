#pragma once

#include "Compiler/UESpecCompilerTypes.h"
#include "EditorSubsystem.h"
#include "ViewModel/UESpecViewModelBase.h"

#include "UESpecCompilerSubsystem.generated.h"

class UUESpecViewModelBase;
class FJsonObject;
class FJsonValue;
class UPanelSlot;
class UWidget;
class UWidgetBlueprint;

UCLASS()
class UESPECEDITOR_API UUESpecCompilerSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Compiler")
	bool ValidateJsonText(const FString& JsonText, FUESpecCompilerResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Compiler")
	bool ValidateJsonFile(const FString& SpecFilePath, FUESpecCompilerResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Compiler")
	bool CompileJsonFileToWidgetBlueprint(const FString& SpecFilePath, const FString& OutputDirectory, FUESpecCompilerResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "UESpec|Compiler")
	void DescribeViewModelClass(TSubclassOf<UUESpecViewModelBase> ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods) const;

public:
	static bool ParseJsonTextToObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutRootObject);
	static bool NormalizeJsonObject(TSharedPtr<FJsonObject> JsonObject, FString& OutNormalizedJson, FUESpecCompilerError& OutError);
	static void NormalizeValue(const TSharedPtr<FJsonValue>& JsonValue, const FString& ParentKey, bool& bChanged);
	static void NormalizeObject(TSharedPtr<FJsonObject> JsonObject, const FString& ParentKey, bool& bChanged);
	static void CollectVmActionReferences(const TSharedPtr<FJsonValue>& JsonValue, const FString& CurrentPath, TMap<FString, FString>& OutReferences);
	static void ExtractDeclaredViewModelMethods(const TSharedPtr<FJsonObject>& RootObject, TMap<FString, FString>& OutMethodPaths);
	static UClass* ResolveViewModelClass(const FString& ViewModelClassName);
	static UClass* ResolveWidgetBlueprintParentClass(const FString& Kind);
	static UClass* ResolveWidgetClass(const FString& WidgetType, TArray<FString>& OutCandidates);
	static UWidgetBlueprint* CreateWidgetBlueprintAsset(const FString& SpecName, const FString& Kind, const FString& OutputDirectory, FString& OutAssetPath, FUESpecCompilerResult& OutResult);
	/** Resolves the deterministic target <OutputDirectory>/WBP_<SpecName>. Returns false (with a TARGET_* error) when the target exists
	 *  but must not be rebuilt; OutExistingWidgetBlueprint is set when a compiler-owned asset is there to update in place. */
	static bool ResolveTargetWidgetBlueprint(const FString& SpecName, const FString& Kind, const FString& OutputDirectory, FString& OutAssetPath, UWidgetBlueprint*& OutExistingWidgetBlueprint, FUESpecCompilerResult& OutResult);
	/** SHA-256 (lowercase hex) of the UTF-8 normalized UISpec JSON; the value of the UESpecSpecHash ownership tag. */
	static FString ComputeSpecHash(const FString& NormalizedJson);
	static bool BuildWidgetBlueprintTree(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
	static UWidget* BuildWidgetNode(UWidgetBlueprint& WidgetBlueprint, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult);
	static bool AttachChildWidget(UWidget& ParentWidget, UWidget& ChildWidget, const TSharedPtr<FJsonObject>& ChildNodeObject, const FString& ChildPath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult);
	static void ApplyWidgetProperties(UWidget& Widget, const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult);
	static void ApplySlotProperties(UPanelSlot& PanelSlot, const TSharedPtr<FJsonObject>& SlotObject, const FString& SlotPath, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FUESpecCompilerResult& OutResult);
	static void CollectLocalStyleTokens(const TSharedPtr<FJsonObject>& RootObject, TMap<FString, TSharedPtr<FJsonValue>>& OutLocalStyleTokens);
	static void CollectGlobalStyleTokens(TMap<FString, FString>& OutGlobalStyleTokens);
	static TSharedPtr<FJsonValue> ResolveStyleValue(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens);
	static bool TryResolveStyleNumber(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, float& OutNumber);
	static bool TryResolveStyleColor(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FLinearColor& OutColor);
	static bool TryResolveStringLikeValue(const TSharedPtr<FJsonValue>& RawValue, const TMap<FString, TSharedPtr<FJsonValue>>& LocalStyleTokens, const TMap<FString, FString>& GlobalStyleTokens, FString& OutString);
	static FString ExtractLiteralContentString(const TSharedPtr<FJsonValue>& ContentValue);
	static bool SaveWidgetBlueprintAsset(UWidgetBlueprint& WidgetBlueprint, const FString& AssetPath, FUESpecCompilerResult& OutResult);
	static bool ValidateTopLevelDocument(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
	static void ValidateWidgetTree(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
	static void ValidateWidgetNode(const TSharedPtr<FJsonObject>& NodeObject, const FString& NodePath, const FString& TopLevelApiVersion, FUESpecCompilerResult& OutResult);
	static void ValidateActions(const TArray<TSharedPtr<FJsonValue>>& ActionsArray, const FString& ActionsPath, FUESpecCompilerResult& OutResult);
	static void ValidateActionDoValue(const TSharedPtr<FJsonValue>& DoValue, const FString& DoPath, FUESpecCompilerResult& OutResult);
	static void ValidateTestSpec(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
	static void ValidateTestSteps(const TArray<TSharedPtr<FJsonValue>>& Steps, const FString& PhasePath, FUESpecCompilerResult& OutResult);
	static bool IsSupportedApiVersion(const FString& ApiVersion);
	static bool IsApiVersionGreaterThan(const FString& ApiVersion, const FString& MaximumApiVersion);
	static bool IsCamelCaseIdentifier(const FString& Identifier);
	static FString NormalizePropertyKeyForContext(const FString& ParentKey, const FString& OldKey);
	static void AddInfo(FUESpecCompilerResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint, TArray<FString> NearestDeclared = {});
	static void AddError(FUESpecCompilerResult& OutResult, const FString& Code, const FString& Path, const FString& Message, const FString& Hint, TArray<FString> NearestDeclared = {});
	static TArray<FString> BuildNearestSuggestions(const FString& Needle, const TArray<FString>& Candidates);
	static bool ValidateViewModelMethods(const TSharedPtr<FJsonObject>& RootObject, FUESpecCompilerResult& OutResult);
};
