# AngelScript Widget Exploration

## Goal

Round 4 asked whether AngelScript can inherit `UUESpecWidgetBase` or `UUESpecActivatableWidgetBase`, and whether that path is ready to become part of the supported UESpec surface.

## Inputs

- `TestData/as/UW_AsExperiment.as`
- `TestData/as/UAW_AsExperiment.as`
- `Script-Examples/Examples/Example_Widget_UMG.as`

## What Was Verified

1. A minimal AngelScript widget class can compile against `UUESpecWidgetBase`.
2. A minimal AngelScript activatable widget class can compile against `UUESpecActivatableWidgetBase`.
3. Both classes register as AngelScript-generated `UClass` objects and resolve through `ResolveClassByAngelscriptName`.
4. The bridge now accepts both the script-facing name and the generated Unreal-style name for these object classes.
5. Engine sample `Example_Widget_UMG.as` still proves the upstream plugin supports `UUserWidget` inheritance in general.

The concrete regression point we locked down in Round 4 is `UESpec.Angelscript.Bridge.ResolveWidgetBaseExperiment`.

## Conclusion

Verdict: `partial`.

AngelScript inheritance from `UUESpecWidgetBase` and `UUESpecActivatableWidgetBase` is viable at the class-compilation and reflection-registration level. That is enough to treat the path as technically plausible.

We did **not** promote AS widget inheritance to an official UESpec authoring surface this round. The missing piece is not class generation itself, but the downstream authoring contract:

- manual creation of `WidgetBlueprint` children from those AS parents
- editor compile/open workflow stability
- PIE/runtime instantiation as a maintained product path
- clear guidance for when users should choose AS widgets instead of generated WBP + AS ViewModel

## Main Gotcha

AngelScript object class names are easy to misresolve. The script class name and the generated Unreal class name are not always identical after prefix normalization. `UW_...` and `UAW_...` style names are especially easy to break if the bridge strips one more prefix than the generator did. Round 4 hardened the bridge so the lookup accepts both forms.

## Recommendation

Keep the supported production path as:

- UESpec-generated `WidgetBlueprint`
- UESpec widget bases in C++/WBP
- AngelScript ViewModel support

Treat AS widget inheritance as a follow-up spike only if one of these becomes a real product requirement:

- custom widget behavior that cannot live cleanly in generated EventGraph + ViewModel methods
- teams that want to author widget parent classes in script rather than C++/Blueprint
- a future compiler phase that intentionally emits WBP children of AS widget parents
