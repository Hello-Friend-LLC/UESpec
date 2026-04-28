#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UESpecTestHelpers.h"

namespace
{
	using namespace UE::UESpec::Tests;

	static bool HasErrorCode(const FUESpecCompilerResult& Result, const FString& ExpectedCode)
	{
		for (const FUESpecCompilerError& Error : Result.Errors)
		{
			if (Error.Code == ExpectedCode)
			{
				return true;
			}
		}

		return false;
	}

	static bool ValidateJson(FAutomationTestBase& Test, const FString& JsonText, FUESpecCompilerResult& OutResult)
	{
		FScopedRegistryOverride RegistryOverride;
		UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(Test);
		if (!Compiler)
		{
			return false;
		}

		Compiler->ValidateJsonText(JsonText, OutResult);
		return true;
	}

	static bool ValidateFile(FAutomationTestBase& Test, const FString& FileName, FUESpecCompilerResult& OutResult)
	{
		FScopedRegistryOverride RegistryOverride;
		UUESpecCompilerSubsystem* Compiler = GetCompilerSubsystem(Test);
		if (!Compiler)
		{
			return false;
		}

		const FString FilePath = GetTestDataPath(FileName);
		Test.TestTrue(FString::Printf(TEXT("%s exists"), *FileName), FPaths::FileExists(FilePath));
		if (!FPaths::FileExists(FilePath))
		{
			return false;
		}

		Compiler->ValidateJsonFile(FilePath, OutResult);
		return true;
	}

	static bool ParseNormalizedJson(FAutomationTestBase& Test, const FUESpecCompilerResult& Result, TSharedPtr<FJsonObject>& OutObject)
	{
		Test.TestTrue(TEXT("NormalizedJson is not empty"), !Result.NormalizedJson.IsEmpty());
		return ParseJsonObject(Result.NormalizedJson, OutObject);
	}

	static void ExpectReject(FAutomationTestBase& Test, const FString& JsonText, const FString& ExpectedCode)
	{
		FUESpecCompilerResult Result;
		if (!ValidateJson(Test, JsonText, Result))
		{
			return;
		}

		Test.TestFalse(TEXT("Validation rejects invalid input"), Result.bOk);
		Test.TestTrue(TEXT("Compiler returns at least one error"), Result.Errors.Num() >= 1);
		Test.TestTrue(FString::Printf(TEXT("Compiler emits code %s"), *ExpectedCode), HasErrorCode(Result, ExpectedCode));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerCasingNormalizationRuleDrivenTest,
	"UESpec.Compiler.CasingNormalization.RuleDriven",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerCasingNormalizationRuleDrivenTest::RunTest(const FString& Parameters)
{
	const FString JsonText = TEXT(R"JSON(
{
  "apiVersion": "0.2",
  "kind": "Component",
  "name": "CasingProbe",
  "root": {
    "type": "CanvasPanel",
    "id": "rootPanel",
    "children": [
      {
        "type": "TextBlock",
        "id": "titleText",
        "slot": {
          "padding": [1, 2, 3, 4],
          "hAlign": "Fill",
          "vAlign": "Center"
        },
        "style": {
          "fontSize": 24,
          "fontColor": "@color.white"
        },
        "actions": [
          { "on": "Clicked", "do": "vm.UseSkill(0)" }
        ],
        "content": "Hello"
      }
    ]
  },
  "viewModel": {
    "class": "VM_SkillBar",
    "methods": [
      { "name": "UseSkill", "params": [{ "name": "index", "type": "int" }], "pure": false }
    ]
  }
}
)JSON");

	FUESpecCompilerResult Result;
	if (!ValidateJson(*this, JsonText, Result))
	{
		return false;
	}

	TestTrue(TEXT("Casing probe validates"), Result.bOk);
	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Normalized json parses back"), ParseNormalizedJson(*this, Result, RootObject));
	if (!RootObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RootWidgetObject = nullptr;
	TestTrue(TEXT("Root object exists"), RootObject->TryGetObjectField(TEXT("root"), RootWidgetObject) && RootWidgetObject && (*RootWidgetObject).IsValid());
	if (!RootWidgetObject || !(*RootWidgetObject).IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
	TestTrue(TEXT("children exists"), (*RootWidgetObject)->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray && ChildrenArray->Num() == 1);
	if (!ChildrenArray || ChildrenArray->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ChildObject = nullptr;
	TestTrue(TEXT("child widget exists"), (*ChildrenArray)[0]->TryGetObject(ChildObject) && ChildObject && (*ChildObject).IsValid());
	if (!ChildObject || !(*ChildObject).IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* SlotObject = nullptr;
	const TSharedPtr<FJsonObject>* StyleObject = nullptr;
	TestTrue(TEXT("slot object exists"), (*ChildObject)->TryGetObjectField(TEXT("slot"), SlotObject) && SlotObject && (*SlotObject).IsValid());
	TestTrue(TEXT("style object exists"), (*ChildObject)->TryGetObjectField(TEXT("style"), StyleObject) && StyleObject && (*StyleObject).IsValid());
	if (!SlotObject || !(*SlotObject).IsValid() || !StyleObject || !(*StyleObject).IsValid())
	{
		return false;
	}

	TestTrue(TEXT("Padding normalized"), (*SlotObject)->HasField(TEXT("Padding")));
	TestTrue(TEXT("HorizontalAlignment hard-mapped"), (*SlotObject)->HasField(TEXT("HorizontalAlignment")));
	TestTrue(TEXT("VerticalAlignment hard-mapped"), (*SlotObject)->HasField(TEXT("VerticalAlignment")));
	TestTrue(TEXT("FontSize normalized"), (*StyleObject)->HasField(TEXT("FontSize")));
	TestTrue(TEXT("FontColor normalized"), (*StyleObject)->HasField(TEXT("FontColor")));
	TestTrue(TEXT("actions.on preserved"), Result.NormalizedJson.Contains(TEXT("\"on\"")));
	TestTrue(TEXT("actions.do preserved"), Result.NormalizedJson.Contains(TEXT("\"do\"")));
	TestTrue(TEXT("widget id preserved"), Result.NormalizedJson.Contains(TEXT("\"titleText\"")));
	TestTrue(TEXT("widget type preserved"), Result.NormalizedJson.Contains(TEXT("\"TextBlock\"")));
	return true;
}

#define UESPEC_DEFINE_REJECT_TEST(TestClass, PrettyName, JsonLiteral, ExpectedCode) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST( \
	TestClass, \
	PrettyName, \
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
bool TestClass::RunTest(const FString& Parameters) \
{ \
	ExpectReject(*this, JsonLiteral, ExpectedCode); \
	return true; \
}

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectActionDoInjectionTest,
	"UESpec.Compiler.Reject.ActionDoInjection",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadDo","viewModel":{"class":"VM_SkillBar","methods":[{"name":"UseSkill","params":[{"name":"index","type":"int"}],"pure":false}]},"root":{"type":"Button","id":"slotQ","actions":[{"on":"Clicked","do":"execute('rm -rf /')"}]}})JSON"),
	TEXT("INVALID_VM_METHOD_REF"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectActionBlockAmbiguousTest,
	"UESpec.Compiler.Reject.ActionBlockAmbiguous",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadActionBlock","root":{"type":"Button","id":"slotQ","actions":[{"on":"Clicked","do":{"openLayer":"Tooltip","emitSignal":"Ping"}}]}})JSON"),
	TEXT("ACTION_BLOCK_AMBIGUOUS"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectActionBlockEmptyTest,
	"UESpec.Compiler.Reject.ActionBlockEmpty",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"EmptyActionBlock","root":{"type":"Button","id":"slotQ","actions":[{"on":"Clicked","do":{}}]}})JSON"),
	TEXT("ACTION_BLOCK_EMPTY"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectWidgetIdPascalCaseTest,
	"UESpec.Compiler.Reject.WidgetIdPascalCase",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadWidgetId","root":{"type":"TextBlock","id":"SlotQ","content":"Q"}})JSON"),
	TEXT("INVALID_WIDGET_ID"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectApiVersionOutOfRangeTest,
	"UESpec.Compiler.Reject.ApiVersionOutOfRange",
	TEXT(R"JSON({"apiVersion":"1.0","kind":"Component","name":"BadVersion","root":{"type":"TextBlock","id":"slotQ","content":"Q"}})JSON"),
	TEXT("API_VERSION_TOO_HIGH"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectApiVersionMalformedTest,
	"UESpec.Compiler.Reject.ApiVersionMalformed",
	TEXT(R"JSON({"apiVersion":"v0.2","kind":"Component","name":"BadVersionFormat","root":{"type":"TextBlock","id":"slotQ","content":"Q"}})JSON"),
	TEXT("INVALID_API_VERSION"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectTestStepUnknownVerbTest,
	"UESpec.Compiler.Reject.TestStepUnknownVerb",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Test","name":"BadVerb","testSpec":{"target":"SkillBarHUD","phases":{"assert":[{"step":"bad","tap":"slotQ"}]}}})JSON"),
	TEXT("TEST_STEP_VERB_MISSING"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectTestStepAmbiguousVerbTest,
	"UESpec.Compiler.Reject.TestStepAmbiguousVerb",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Test","name":"AmbiguousVerb","testSpec":{"target":"SkillBarHUD","phases":{"assert":[{"step":"bad","click":"slotQ","assertVisible":{"id":"slotQ","expected":true}}]}}})JSON"),
	TEXT("TEST_STEP_VERB_AMBIGUOUS"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectTestStepMissingVerbTest,
	"UESpec.Compiler.Reject.TestStepMissingVerb",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Test","name":"MissingVerb","testSpec":{"target":"SkillBarHUD","phases":{"assert":[{"step":"bad"}]}}})JSON"),
	TEXT("TEST_STEP_VERB_MISSING"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectKindTestMissingSpecTest,
	"UESpec.Compiler.Reject.KindTestMissingSpec",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Test","name":"MissingTestSpec"})JSON"),
	TEXT("MISSING_ROOT_OR_TEST_SPEC"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectKindHudMissingRootTest,
	"UESpec.Compiler.Reject.KindHudMissingRoot",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"HUD","name":"MissingRoot"})JSON"),
	TEXT("MISSING_ROOT_OR_TEST_SPEC"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectDuplicateWidgetIdTest,
	"UESpec.Compiler.Reject.DuplicateWidgetId",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"DuplicateIdProbe","root":{"type":"VerticalBox","id":"rootPanel","children":[{"type":"TextBlock","id":"slotQ","content":"Q"},{"type":"TextBlock","id":"slotQ","content":"W"}]}})JSON"),
	TEXT("DUPLICATE_WIDGET_ID"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectUnknownNavTargetTest,
	"UESpec.Compiler.Reject.UnknownNavTarget",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadNav","root":{"type":"HorizontalBox","id":"skillRow","children":[{"type":"TextBlock","id":"slotQ","content":"Q","navigation":{"right":"slotW"}}]}})JSON"),
	TEXT("UNKNOWN_NAV_TARGET"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectVmMethodNotDeclaredTest,
	"UESpec.Compiler.Reject.VmMethodNotDeclared",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadVmCall","viewModel":{"class":"VM_SkillBar","methods":[{"name":"UseSkill","params":[{"name":"index","type":"int"}],"pure":false}]},"root":{"type":"Button","id":"slotQ","actions":[{"on":"Clicked","do":"vm.Unknown(0)"}]}})JSON"),
	TEXT("VM_METHOD_NOT_DECLARED"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectUnsupportedBindingModeTest,
	"UESpec.Compiler.Reject.UnsupportedBindingMode",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadBindingMode","viewModel":{"class":"VM_SkillBar","bindingMode":"reactive","methods":[{"name":"UseSkill","params":[{"name":"index","type":"int"}],"pure":false}]},"root":{"type":"Button","id":"slotQ","actions":[{"on":"Clicked","do":"vm.UseSkill(0)"}]}})JSON"),
	TEXT("VM_BINDING_MODE_UNSUPPORTED"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectInvalidKindTest,
	"UESpec.Compiler.Reject.InvalidKind",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"OverlayMenu","name":"BadKind","root":{"type":"TextBlock","id":"slotQ","content":"Q"}})JSON"),
	TEXT("INVALID_KIND"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectInvalidNameTest,
	"UESpec.Compiler.Reject.InvalidName",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"skill_bar_hud","root":{"type":"TextBlock","id":"slotQ","content":"Q"}})JSON"),
	TEXT("INVALID_NAME"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectUnknownConverterTest,
	"UESpec.Compiler.Reject.UnknownConverter",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadConverter","root":{"type":"TextBlock","id":"slotQ","content":{"bind":"skills[0].label","convert":"MagicConvert","fallback":"Q"}}})JSON"),
	TEXT("UNKNOWN_CONVERTER"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectUnknownStyleTokenTest,
	"UESpec.Compiler.Reject.UnknownStyleToken",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadToken","root":{"type":"TextBlock","id":"slotQ","content":"Q","style":{"fontColor":"@color.missing"}}})JSON"),
	TEXT("UNKNOWN_STYLE_TOKEN"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectUnknownStateTest,
	"UESpec.Compiler.Reject.UnknownState",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadState","stateMachine":{"initial":"Normal","states":[{"name":"Normal"}]},"root":{"type":"TextBlock","id":"slotQ","content":"Q","stateBindings":{"MissingState":{"opacity":0.5}}}})JSON"),
	TEXT("UNKNOWN_STATE"))

UESPEC_DEFINE_REJECT_TEST(
	FUESpecCompilerRejectDeletionConflictTest,
	"UESpec.Compiler.Reject.DeletionConflict",
	TEXT(R"JSON({"apiVersion":"0.2","kind":"Component","name":"BadDelete","deletions":["slotQ"],"root":{"type":"TextBlock","id":"slotQ","content":"Q"}})JSON"),
	TEXT("DELETION_CONFLICT"))

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAcceptValidHudSpecTest,
	"UESpec.Compiler.Accept.ValidHudSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAcceptValidHudSpecTest::RunTest(const FString& Parameters)
{
	FUESpecCompilerResult Result;
	if (!ValidateFile(*this, TEXT("hud-skill-bar.json"), Result))
	{
		return false;
	}

	TestTrue(TEXT("Valid HUD spec passes validation"), Result.bOk);
	TestEqual(TEXT("Valid HUD spec has zero errors"), Result.Errors.Num(), 0);

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Normalized HUD spec parses"), ParseNormalizedJson(*this, Result, RootObject));
	if (!RootObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RootWidgetObject = nullptr;
	TestTrue(TEXT("root exists"), RootObject->TryGetObjectField(TEXT("root"), RootWidgetObject) && RootWidgetObject && (*RootWidgetObject).IsValid());
	if (!RootWidgetObject || !(*RootWidgetObject).IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
	TestTrue(TEXT("root.children exists"), (*RootWidgetObject)->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray && ChildrenArray->Num() > 0);
	if (!ChildrenArray || ChildrenArray->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FirstChildObject = nullptr;
	TestTrue(TEXT("first child exists"), (*ChildrenArray)[0]->TryGetObject(FirstChildObject) && FirstChildObject && (*FirstChildObject).IsValid());
	if (!FirstChildObject || !(*FirstChildObject).IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FirstChildSlotObject = nullptr;
	TestTrue(TEXT("first child slot exists"), (*FirstChildObject)->TryGetObjectField(TEXT("slot"), FirstChildSlotObject) && FirstChildSlotObject && (*FirstChildSlotObject).IsValid());
	if (!FirstChildSlotObject || !(*FirstChildSlotObject).IsValid())
	{
		return false;
	}

	TestTrue(TEXT("slot.padding normalized to Padding"), (*FirstChildSlotObject)->HasField(TEXT("Padding")));
	const TSharedPtr<FJsonObject>* StateBindingsObject = nullptr;
	TestTrue(TEXT("stateBindings exists"), (*FirstChildObject)->TryGetObjectField(TEXT("stateBindings"), StateBindingsObject) && StateBindingsObject && (*StateBindingsObject).IsValid());
	if (StateBindingsObject && (*StateBindingsObject).IsValid())
	{
		const TSharedPtr<FJsonObject>* CooldownObject = nullptr;
		TestTrue(TEXT("Cooldown state binding exists"), (*StateBindingsObject)->TryGetObjectField(TEXT("Cooldown"), CooldownObject) && CooldownObject && (*CooldownObject).IsValid());
		if (CooldownObject && (*CooldownObject).IsValid())
		{
			TestTrue(TEXT("stateBindings opacity normalized to Opacity"), (*CooldownObject)->HasField(TEXT("Opacity")));
			TestTrue(TEXT("stateBindings saturation normalized to Saturation"), (*CooldownObject)->HasField(TEXT("Saturation")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESpecCompilerAcceptValidTestSpecTest,
	"UESpec.Compiler.Accept.ValidTestSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUESpecCompilerAcceptValidTestSpecTest::RunTest(const FString& Parameters)
{
	FUESpecCompilerResult Result;
	if (!ValidateFile(*this, TEXT("test-skill-bar.json"), Result))
	{
		return false;
	}

	TestTrue(TEXT("Valid TestSpec passes validation"), Result.bOk);
	TestEqual(TEXT("Valid TestSpec has zero errors"), Result.Errors.Num(), 0);

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("Normalized test spec parses"), ParseNormalizedJson(*this, Result, RootObject));
	if (!RootObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* TestSpecObject = nullptr;
	TestTrue(TEXT("testSpec exists"), RootObject->TryGetObjectField(TEXT("testSpec"), TestSpecObject) && TestSpecObject && (*TestSpecObject).IsValid());
	if (!TestSpecObject || !(*TestSpecObject).IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PhasesObject = nullptr;
	TestTrue(TEXT("phases exists"), (*TestSpecObject)->TryGetObjectField(TEXT("phases"), PhasesObject) && PhasesObject && (*PhasesObject).IsValid());
	if (!PhasesObject || !(*PhasesObject).IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssertSteps = nullptr;
	TestTrue(TEXT("assert steps exist"), (*PhasesObject)->TryGetArrayField(TEXT("assert"), AssertSteps) && AssertSteps && AssertSteps->Num() >= 3);
	if (!AssertSteps || AssertSteps->Num() < 3)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* AssertStyleStep = nullptr;
	TestTrue(TEXT("assertStyle step exists"), (*AssertSteps)[2]->TryGetObject(AssertStyleStep) && AssertStyleStep && (*AssertStyleStep).IsValid());
	if (!AssertStyleStep || !(*AssertStyleStep).IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* AssertStyleObject = nullptr;
	TestTrue(TEXT("assertStyle object exists"), (*AssertStyleStep)->TryGetObjectField(TEXT("assertStyle"), AssertStyleObject) && AssertStyleObject && (*AssertStyleObject).IsValid());
	if (AssertStyleObject && (*AssertStyleObject).IsValid())
	{
		TArray<FString> AssertStyleKeys;
		(*AssertStyleObject)->Values.GetKeys(AssertStyleKeys);
		AssertStyleKeys.Sort();
		TestTrue(TEXT("assertStyle keeps property lowercase"), (*AssertStyleObject)->HasField(TEXT("property")));
		const bool bContainsUpperProperty = AssertStyleKeys.ContainsByPredicate([](const FString& Key)
		{
			return Key.Equals(TEXT("Property"), ESearchCase::CaseSensitive);
		});
		TestFalse(TEXT("assertStyle does not normalize protocol key to Property"), bContainsUpperProperty);
	}

	return true;
}
