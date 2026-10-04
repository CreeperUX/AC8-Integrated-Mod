# 2.3.2 (local candidate)

Repair mission-specific A-6E control metadata; isolate model observation from optional recorder failures. Retains the 2.3.1 control law. See docs/V232-OBSERVATION.md. Actual mission acceptance pending.

# Changelog

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
