# Changelog

## 2.3.4 GUI preview3 — local release audit

- Unifies SHA256 and host/module loading across console, GUI and cleanup.
- Adds truly read-only session preflight, per-game operation exclusion, manifest uniqueness and archive path guards.
- Fixes Steam escaping/account validation and surfaces hidden GUI bootstrap failures.
- Adds environment-isolation and Steam-bridge regressions; see docs/RELEASE-AUDIT-20261004.md for remaining real-game acceptance.

## 2.3.4-gui-preview.2 / GUI-Test2 — local automatic discovery

- Reads Steam registration and default/multiple-library app manifests to locate AC8 on startup.
- Checks the environment in the background once paths are known; ambiguous results expose candidate lists.
- Preserves saved/manual inputs and all browse controls. Discovery does not save settings or modify the game.
- Adds parser, multi-library, manual override and read-only regression coverage. Personal test package only.

## 2.3.4-gui-preview.1 — local three-mode GUI integration

- Connects the CreeperUX UI to guidance-only, full missile and flight-only selection.
- Persists canonical missile_mode values, restores legacy choices and blocks unsaved guidance/full changes.
- Verifies GUI-to-staging round trips for every mode; retains cleanup, themes and the v2.3.2 native flight controller.
- Local preview only; no GitHub publication.

## 2.3.4 — three installation scopes (local candidate)

- Adds independently selectable mouse+guidance, mouse+full-missiles, and mouse-only scopes.
- Guidance writes only32HomingForesightAmount fields, with original performance/fuse/visuals retained.
- Legacy1/0 settings migrate tofull/none; new settings useguidance/full/none names.
- Flight-control binary remains the v2.3.2 baseline; no2.4 experimental control changes.

## 2.3.3 — unreleased draft; installation and startup experience

- Accepts the game root, Win64 folder or AceCombat8.exe path with Chinese/space/quoted paths and retryable input.
- Adds Chinese console guidance, local diagnostic logs and retained nonzero failure codes across CMD entrypoints.
- Integrates confirmed historical residual recovery and directory write probes; unknown loaders remain protected.
- Reports deployment failures before cleanup, keeps the primary failure if cleanup also fails, and skips optional analysis on failed deployment.
- Treats Python/NumPy as optional, captures analysis errors to a session log, and tolerates a first run without a saves directory.
- Includes the cleanup 1.0.0 fixes in the full package. Retains the exact v2.3.2 native DLL and payload; no flight-control or missile changes.

## Cleanup tool 1.0.0 — historical loader recovery

- Adds a standalone cleaner for historical package-state loss and partial cleanup; no controller or missile changes.
- Backs up and SHA-256 verifies all selected contents before removal, rejects unknown loaders and filesystem links, and reports residuals instead of false success.
- Supports retrying partial removal and explicitly confirmed recovery without the old package session record.
- Records intended deployment before copying files and retires completed session state before optional save/analysis diagnostics.
- Adds Windows PowerShell cleanup regressions and Chinese recovery documentation. Existing release ZIPs remain unchanged; download the separate cleanup tool for historical leftovers.

## 2.3.2 — installation encoding and mission observation fixes

- Removes the duplicate UTF-8 BOM in Setup.ps1 that turned its first assignment into a CommandNotFoundException.
- Adds actual Windows PowerShell Setup execution regression, first-statement checks and build-time duplicate-BOM rejection.
- Separates essential model metadata from optional recording; supports the mission15 A-6E with an independent learning identity. Recorder failures no longer disable model observations.
- Reports unconfirmed model actuation when no state evidence exists.
- Retains v2.3.1 control laws and missile parameters. The mission-specific fix has automated validation; real mission flight acceptance remains pending. See docs/V232-OBSERVATION.md.

## 2.3.1 — startup packaging fix

- Includes the required `validation-status.json` omitted from2.3.0.
- Runs package preflight during packaging and before installation, checking required files, gate, payload hashes and settings.
- Adds a read-only launcher `-CheckOnly` and regression coverage for missing metadata, disabled gates and tampered payloads.
- Flight control and missile parameters are unchanged. Use2.3.1 instead of the affected2.3.0 package.

## 2.3.0 — optional missile module

- Setup and `Choose-Features.cmd` select mouse-only or mouse-plus-missiles.
- Mouse-only staging omits AC8SourceInit and uses mouse-module readiness checks.
- Repository-ready source, build scripts, regression tests and Chinese documentation.
- Native flight controller remains the v2.2 implementation; no new gain tuning.

## 2.2.0 — two control policies

- F4 switches CLASSIC 2.0 and AGILE 2.1 with a0.35s transition and retained learning state.
- Short GPU mode notice; F8 remains the mouse-assistance switch.
- Mode selection telemetry expands CSV to100 columns.

## 2.1.1 — checkpoint mounted visuals

- Bounded mounted-mesh reference checks repair default-mesh resets.
- Initialization scheduling can rearm after the same pawn is reused.

## 2.1.0 — more active pursuit

- State-dependent input/rate margins and damping relax away from the braking region.
- Preserves near-target behavior and independent parameter validation.

## 2.0.0 — full three-axis model control

- Standard model control starts immediately for pitch, roll and yaw.
- Learned parameter validation is separated from control authority.
- Deployed parameters survive candidate resets and are monitored independently.
