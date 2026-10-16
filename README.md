# UESpec

UESpec is a UE5 plugin for spec-driven UMG/CommonUI generation and UI test automation. It reads declarative UISpec JSON, validates the schema and support surface, compiles widgets through Unreal commandlets, and exposes structured failure reports for agent-driven repair loops.

The companion MCP server lives at `https://github.com/Mithril0rd/uespec-mcp-public`.

## What

- Compiles UISpec JSON into Widget Blueprint assets.
- Dumps the current support surface: UMG/CommonUI widgets, registered ARPG/MMO components, converters, style tokens, signals, and action primitives.
- Validates spec shape and known symbols before generation.
- Runs JSON test specs against generated widgets and emits JUnit plus structured failure reports.
- Provides AngelScript ViewModel integration through manual binding fallback.
- Ships a Round 6 ARPG/MMO component library slice with 12 built-in components and example HUD/test specs.

## Quickstart

Prerequisites:

- Unreal Engine 5.4.
- A UE project with `CommonUI`, `ModelViewViewModel`, and `Angelscript` available.
- This plugin placed under the project or engine `Plugins/UESpec` directory.

Enable the plugin in the target `.uproject`:

```json
{
  "Plugins": [
    { "Name": "UESpec", "Enabled": true },
    { "Name": "CommonUI", "Enabled": true },
    { "Name": "ModelViewViewModel", "Enabled": true },
    { "Name": "Angelscript", "Enabled": true }
  ]
}
```

Build the editor target:

```powershell
<UE_ROOT>\Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development -Project=<PROJECT>.uproject -WaitMutex -NoHotReloadFromIDE
```

Run the first commandlet to verify the plugin loads and the support surface can be serialized:

```powershell
<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <PROJECT>.uproject -run=UESpecDumpSupportSurface -Format=json -Output=<PROJECT>\Saved\UESpec\support-surface.json -unattended -NoSplash -NoP4
```

Validate and compile an example HUD:

```powershell
<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <PROJECT>.uproject -run=UESpecValidate -SpecFile=<PLUGIN_ROOT>\Examples\HUDs\combat-status-hud.json -Output=<PROJECT>\Saved\UESpec\validate-combat-status.json -unattended -NoSplash -NoP4
```

```powershell
<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <PROJECT>.uproject -run=UESpecCompile -SpecFile=<PLUGIN_ROOT>\Examples\HUDs\combat-status-hud.json -OutputDir=/Game/UESpec/Generated -Output=<PROJECT>\Saved\UESpec\compile-combat-status.json -unattended -NoSplash -NoP4
```

Run a test spec against the generated widget:

```powershell
<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <PROJECT>.uproject -run=UESpecRunTests -SpecFile=<PLUGIN_ROOT>\Examples\Tests\combat-status-hud.test.json -Report=<PROJECT>\Saved\UESpec\combat-status.junit.xml -JsonReport=<PROJECT>\Saved\UESpec\combat-status.failure-report.json -Output=<PROJECT>\Saved\UESpec\combat-status.result.json -unattended -NoSplash -NoP4 -NullRHI
```

## Architecture

```text
UISpec JSON
    |
    v
UESpecValidate / compiler schema checks
    |
    v
UESpecCompile -> Widget Blueprint assets under /Game/UESpec/Generated
    |
    v
UESpecRunTests -> JUnit + structured failure-report.json
```

Core modules:

- `UESpecRuntime`: runtime widget bases, component registry, support surface data, sample widgets, test runner primitives.
- `UESpecEditor`: commandlets, compiler subsystem, binding builder, Widget Blueprint generation.
- `UESpecTests`: automation coverage for compiler behavior, AngelScript integration, support surface, and JSON test execution.

## Component Library

Round 6 registers these built-in ARPG/MMO components:

- `SkillSlot`
- `RedDot`
- `HealthBar`
- `ResourceBar`
- `DamageNumber`
- `StatusEffectIcon`
- `QuestEntry`
- `PlayerNamePlate`
- `ItemSlot`
- `ItemTooltip`
- `EquipmentPanel`
- `ChatLine`

Example specs live under `Examples/Components`, `Examples/HUDs`, and `Examples/Tests`. The broadest example is `Examples/HUDs/round6-showcase-hud.json`, which exercises all 12 built-ins.

## Companion Repo

`uespec-mcp` is the agent-facing orchestration layer. It talks to this plugin through the commandlets above and adds:

- MCP tools for validate, compile, test, support-surface lookup, and autofix.
- Python mock bridge for offline evals.
- LLM adapters and JSON Patch proposal handling.
- Closed-loop failure repair and eval reports.

Use the MCP repo when integrating with Claude Desktop, Codex, or other MCP-capable agents. Use this repo when changing Unreal-side generation, commandlets, runtime widgets, or tests.

## Known Limitations

- The plugin currently targets UE 5.4.
- `bindingMode: "manual"` is the supported path for AngelScript ViewModels. Full MVVM FieldNotify parity for AngelScript is not claimed.
- Complex bind paths with array indexes or calls, such as `skills[0].cooldownProgress`, are not supported: compile fails with `UNSUPPORTED_BINDING_PATH`. Unresolved paths fail with `UNRESOLVED_BINDING_PATH`, content binds on widgets that are not TextBlock/ProgressBar (or subclasses) with `UNSUPPORTED_BINDING_TARGET`, and mistyped converters with `INVALID_CONVERTER`. See `docs/round4-report.md` section 3.5.
- Action events `FocusReceived`, `FocusLost`, `DoubleClicked`, `LongPressed`, `DragDetected`, `Drop` and the `playAnim` primitive are declared but unimplemented (`"status": "unimplemented"` in the support surface); validation rejects them with `UNSUPPORTED_EVENT` / `UNSUPPORTED_ACTION`.
- AngelScript widget-base inheritance was explored but not promoted to the supported authoring surface.
- The Round 6 component widgets are lightweight host classes for generation/test coverage, not final production art.
- Real end-to-end use still requires a local Unreal project and compatible plugin dependencies.

## Documentation

- `docs/round4-report.md`: AngelScript integration and binding limitations.
- `docs/round5-report.md`: structured failure reports and test-runner hardening.
- `docs/round6-report.md`: ARPG/MMO component library and showcase.
- `docs/component-library.md`: current component inventory.
- `docs/roadmap.md`: development timeline and release hardening plan.
