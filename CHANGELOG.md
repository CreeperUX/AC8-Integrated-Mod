# Changelog

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
