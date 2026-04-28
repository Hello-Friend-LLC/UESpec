# UESpec Roadmap

This roadmap reflects the current prerelease state rather than aspirational marketing.

## Completed

### Round 4 - AngelScript Integration

- Added AngelScript class detection and name resolution.
- Validated `vm.*` event routing into AngelScript ViewModels.
- Added manual runtime binding fallback for text, percent, and visibility.
- Documented why AngelScript widget-base inheritance remains exploratory.
- Known limitation carried forward: complex bind paths with array indexes are skipped by `BindingBuilder`.

### Round 5 - Structured Failure Reports And Headless Test Runner

- Added structured test failure context and JSON failure reports.
- Hardened `UESpecRunTests` commandlet output for MCP consumption.
- Improved headless Slate initialization and unbound click classification.
- Supported the companion `uespec-mcp` closed-loop autofix work.

### Round 6 - ARPG/MMO Component Library

- Added a 12-component built-in library for combat, items, quests, nameplates, and chat.
- Added focused component examples, HUD examples, and test specs.
- Added `Round6ShowcaseHUD` as an integrated example that exercises all current built-ins.
- Added support-surface, compiler, and test-runner automation coverage for the new examples.

## Current Prerelease Hardening

- Improve public README and release metadata.
- Keep commandlet contracts aligned with `uespec-mcp`.
- Make known limitations explicit before any public repository switch.
- Avoid committing personal machine paths or private relay configuration into reports.

## Next

### Round 7 - Screenshot To UISpec Support

- Use the component library as the retrieval surface for screenshot-to-UISpec generation.
- Keep screenshot examples local unless they are redistributable.
- Validate generated specs through the same commandlet/test pipeline.

### Round 8 - Contract Alignment And Diagnostics

- Align mock bridge semantic error codes with UE compiler error codes.
- Decide whether UE should emit a real `UNKNOWN_SIGNAL` compiler error or whether eval fixtures should use an existing UE-side code.
- Promote complex bind-path skips from warnings into explicit compiler diagnostics where feasible.
- Expand release CI around commandlet smoke tests in a minimal UE project.
