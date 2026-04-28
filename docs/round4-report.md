# Round 4 Report

## Scope

Round 4 focused on AngelScript integration for UESpec. The delivery order was `AS-1 -> AS-2 -> AS-3 -> AS-4`.

## 1. Task Implementation Files

### AS-1: AngelScript ViewModel bridge

- `Source/UESpecRuntime/Public/ViewModel/UESpecAngelscriptBridge.h`
- `Source/UESpecRuntime/Private/ViewModel/UESpecAngelscriptBridge.cpp`
- `Source/UESpecRuntime/Private/ViewModel/UESpecViewModelBase.cpp`
- `Source/UESpecEditor/Public/Compiler/UESpecCompilerSubsystem.h`
- `Source/UESpecEditor/Public/Compiler/UESpecCompilerTypes.h`
- `Source/UESpecEditor/Private/Compiler/UESpecCompilerSubsystem.cpp`
- `Source/UESpecEditor/Private/Commandlets/UESpecCommandletUtils.cpp`
- `Source/UESpecTests/Private/Compiler/UESpecAngelscriptBridgeTests.cpp`
- `Source/UESpecTests/Private/UESpecAngelscriptTestHelpers.h`
- `TestData/as/VM_AsSample.as`

What shipped:

- AngelScript class detection and source-path reporting
- AngelScript name resolution from compiler input
- ViewModel method reflection for AngelScript-generated classes
- pure-function detection for AngelScript methods
- compiler info channel `VM_CLASS_IS_ANGELSCRIPT`

### AS-2: EventGraph routing into AngelScript methods

- `Source/UESpecTests/Private/Compiler/UESpecAngelscriptRuntimeTests.cpp`
- `TestData/as/VM_AsSample.as`

What shipped:

- the existing K2 `vm.*` call path was validated against AngelScript ViewModels
- no special AngelScript-only event graph node was required

### AS-3: Binding cooperation with AngelScript ViewModels

- `Source/UESpecRuntime/Public/Widgets/UESpecWidgetBase.h`
- `Source/UESpecRuntime/Private/Widgets/UESpecWidgetBase.cpp`
- `Source/UESpecEditor/Private/Compiler/UESpecBindingBuilder.cpp`
- `Source/UESpecEditor/Private/Compiler/UESpecCompilerSubsystem.cpp`
- `Source/UESpecTests/Private/Compiler/UESpecCompilerSchemaTests.cpp`
- `Source/UESpecTests/Private/Compiler/UESpecAngelscriptRuntimeTests.cpp`
- `TestData/hud-skill-bar-as.json`

What shipped:

- `viewModel.bindingMode`
- `manual` runtime binding fallback for text / percent / visibility
- compiler validation for unsupported binding modes
- generated widget classes marked to tick when manual binding is active

### AS-4: Widget-base inheritance exploration

- `TestData/as/UW_AsExperiment.as`
- `TestData/as/UAW_AsExperiment.as`
- `Source/UESpecTests/Private/Compiler/UESpecAngelscriptBridgeTests.cpp`
- `docs/angelscript-widget-exploration.md`

What shipped:

- exploration samples for widget-base and activatable-widget-base inheritance
- bridge regression coverage for script-name vs generated-name resolution
- documentation of the product conclusion

## 2. New Build.cs Dependencies

- `Source/UESpecRuntime/UESpecRuntime.Build.cs`
  - added private dependency: `AngelscriptCode`
- `Source/UESpecTests/UESpecTests.Build.cs`
  - added private dependency: `AngelscriptCode`

No new editor-module dependency was needed for Round 4.

## 3. Concrete AngelScript Pitfalls

1. `FieldNotify` is the real incompatibility point, not K2 method dispatch.
   AngelScript ViewModels can expose fields and callable methods, but the current plugin/runtime path does not give us the same `FieldNotify` contract that UE MVVM expects for generated binding libraries.

2. MVVM editor assumptions are stricter than plain reflection.
   UESpec can reflect AngelScript fields and methods, and K2 can call them, but `UMVVMEditorSubsystem` compatibility is not the same as "class is a valid `UClass`".

3. Object-class name lookup is easy to get wrong.
   AngelScript object classes may exist in both script-facing and generated Unreal-style forms. `UW_*` and `UAW_*` names were especially error-prone when the bridge stripped one more prefix than the generator did. `ResolveClassByAngelscriptName` was hardened in Round 4 to accept both forms.

4. Manual binding needed generic numeric-property handling.
   Restricting reads to `FFloatProperty` or `FDoubleProperty` was not enough for AngelScript-reflected numeric fields. The runtime fallback now also handles `FNumericProperty`.

5. Manual fallback requires ticking generated widget blueprints.
   Without `SetClassRequiresNativeTick(true)`, the fallback path would compile but not actually refresh at runtime.

6. Test helper cleanup is noisy.
   The staged test scripts are deleted after each automation case. AngelScript then logs "treating file as deleted" on the next reload. These warnings are expected in the current helper flow and did not fail the suite.

## Section 3.5: Known Limitations

Complex bind paths that include array indexing are still skipped by `BindingBuilder`.

This means expressions such as `skills[0].cooldownProgress` and `skills[0].isOnCooldown` do not become live bindings. The current builder rejects any bind path containing `[` or `]`, logs a warning, and continues compilation without producing a structured compiler error. In practice this is a "silent skip" from the orchestrator's point of view because `CompileJsonFileToWidgetBlueprint` still succeeds and `Result.bOk` remains true.

Impact in current tests:

- `UESpec.Compiler.Compile.WritesStateMachineDefaults`

Concrete warning log examples from the Round 4 full automation run:

- `LogUESpecBindingBuilder: Warning: Skipping unsupported complex bind expression 'skills[0].cooldownProgress' at root.children[0].children[1].content.`
- `LogUESpecBindingBuilder: Warning: Skipping unsupported complex bind expression 'skills[0].isOnCooldown' at root.children[0].children[1].visibility.`
- `LogUESpecBindingBuilder: Warning: Skipping unsupported complex bind expression 'skills[0].cooldownRemaining' at root.children[0].children[2].content.`
- `LogUESpecBindingBuilder: Warning: Skipping unsupported complex bind expression 'skills[0].hasUnseenUpgrade' at root.children[0].children[4].visibility.`

Why this matters:

- `WritesStateMachineDefaults` still passes because that test only asserts state-machine default serialization, not live binding coverage for those complex paths.
- the limitation was not introduced by Round 4; the skip behavior already existed in the Round 3 binding pipeline and Round 4 only carried it forward into the AngelScript-enabled branch
- Round 5 orchestrator work should treat complex bind paths as an explicit capability check, not assume "compile success" means "all declared bindings are active"

## 4. AS-3 Final Binding Mode

Round 4 ended on `manual` fallback for AngelScript ViewModels.

The compiler and runtime still support the normal MVVM path for non-AngelScript cases, but the safe supported path for AngelScript ViewModels in Round 4 is:

- `viewModel.class = <AngelScript VM>`
- `viewModel.bindingMode = "manual"`

## 5. AS-4 Conclusion

Conclusion: `partial`.

What we confirmed:

- AngelScript can define classes inheriting `UUESpecWidgetBase`
- AngelScript can define classes inheriting `UUESpecActivatableWidgetBase`
- those classes compile, register, and resolve through the UESpec bridge

What we did not promote to product surface in Round 4:

- official UESpec compiler support for generating WBP children of AS widget parents
- a maintained editor workflow for AS-widget-parent authoring
- a guaranteed supported runtime path beyond class registration and inheritance viability

Round 4 therefore treats AS widgets as an exploration result, while AngelScript ViewModels are the supported integration surface.

## 6. Test Results

### Build

- `Build.bat UnrealEditor Win64 Development -WaitMutex -NoHotReloadFromIDE`
- result: success

### AngelScript-only automation

- command: `Automation RunTests UESpec.Angelscript.`
- result: `7` tests found, `EXIT CODE: 0`
- log: `Templates/TP_BlankBP/Saved/Logs/TP_BlankBP-backup-2026.04.19-15.28.34.log`

Covered tests:

- `UESpec.Angelscript.Binding.TextBindToAsField`
- `UESpec.Angelscript.Bridge.DetectAsClass`
- `UESpec.Angelscript.Bridge.ResolveByName`
- `UESpec.Angelscript.Bridge.ResolveWidgetBaseExperiment`
- `UESpec.Angelscript.EventGraph.CallAsMethod`
- `UESpec.Angelscript.ViewModel.CompileWithAsVm`
- `UESpec.Angelscript.ViewModel.ReflectMethods`

### Full UESpec automation

- command: `Automation RunTests UESpec.`
- result: `65` tests found, `EXIT CODE: 0`
- log: `Templates/TP_BlankBP/Saved/Logs/TP_BlankBP.log`

Notes:

- `JUnitEscape` and `SkipOnAssumeFailure` intentionally emit fail/skip runner messages as part of self-testing, but their automation cases completed successfully.
- Round 3 baseline coverage stayed green after the Round 4 changes.

## 7. Most Surprising Finding

The part that worked immediately was K2 calling into AngelScript methods; the part that forced the fallback design was MVVM field-notify compatibility, not EventGraph routing.
