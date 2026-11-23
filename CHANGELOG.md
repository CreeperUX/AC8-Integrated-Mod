## 2.5.3

- Steam game build 26002268 (2026-11-02 update, #9, #10; PR #11 by bakasayaka): Get-AC8GameBuild maps each supported AceCombat8.exe hash to its build; Setup, the environment check and Launch-Offline accept 26002268 and 25201480 and still refuse any other build (GAME_BUILD).
- Native camera and HMD selection keep one row per build (image TimeDateStamp/SizeOfImage and RVAs) and install for the running build after the same byte checks. The flight-control hook fingerprint matches both builds unchanged.
- version.json and validation-status.json name build 26002268 (supported_game_builds lists both). Version 2.5.3; flight control unchanged.

## 2.5.2

- Extra-mod manager: Pak/IoStore files inside a mod folder are installed with the mod (PW-MkI: AC8OverrideLoader\payloads mounts them); only Pak files outside every mod folder are left out with a note. The zip's own UE4SS loader is still never installed.
- Cleanup: deployed extra-mod files identical (size, SHA-256) to ue4ss-mods are removed before the archive (Remove-AC8DeployedExtraMods; the session record lists the deployed extra mods); files the mod wrote or changed are archived as before.
- Tests: PW-MkI-shaped zip and cleanup of deployed copies in Test-ExtraModsManager. Version 2.5.2; flight control unchanged.

## 2.5.1

- First public release of the 2.5 line (the entries under 2.5.0 below). 2.5.0 was a small test build; installed copies update to 2.5.1 from the launcher.
- A Start menu shortcut from before the native launcher (powershell.exe) is renewed to AC8 Integrated.exe when the launcher opens (Update-AC8AppRegistration).
- Version 2.5.1; "since 2.5" wording. Flight control unchanged (WAR v13.8 / PEACE).

## 2.5.0

- Installed app: IExpress setup program (per user, %LOCALAPPDATA%\Programs\AC8 Integrated, no administrator rights), Start menu launcher, Apps & features uninstall, import of the portable package the Steam launch option points to. Versions in versions\<v>, player data in data\, fixed Steam entry in the install root (package-template/app).
- Updates from GitHub releases/latest: RSA-3072 signed update-manifest.json, zip size and SHA-256, full FILES.sha256 with no unlisted files and no unsafe paths; only newer versions; refused while the game runs or a session is unfinished; current and previous versions kept, rollback. Launcher: check for updates, optional daily check.
- Self-repair: version and host files checked before every launch and restored from the cached verified zip; "check and repair" downloads the same version when needed, rebuilds unreadable settings with a backup, cleans an unfinished session, reports loader leftovers and the Steam launch option, restores the registration.
- Launch diagnostics: DLLs removed or changed right after deployment stop the launch with a security-software hint; [NOT READY] explains the reason from UE4SS.log.
- WAR is the default flight control (control_modes::default_mode=3, template/runtime config, Read-MouseSettings); a saved PEACE moves to WAR once (migrations.json war-default-2.5.0).
- User data through Get-AC8DataRoot / Get-AC8SteamEntry (portable packages unchanged).
- Extra-mod manager (launcher "附加 Mod"): add from a zip or folder (drag and drop; mod folders found by layout; loader DLLs and Pak assets not installed; unsafe paths and links refused), replace keeping the enabled state with the old copy in ue4ss-mods-removed, enable/disable, remove to ue4ss-mods-removed (ExtraMods-Settings.ps1, ExtraMods-GUI.ps1/.xaml, Test-ExtraModsManager).
- Native launcher AC8 Integrated.exe (native/launcher, built with the .NET Framework C# compiler included in Windows; icon, version, asInvoker): the player tool as its own application. It hosts Host.ps1 -Action Gui (installed) or Launcher-GUI.ps1 (package) in its own process with the Windows PowerShell 5.1 engine, so no console window appears and the window, taskbar button, Alt+Tab, Task Manager and pinning show AC8 Integrated with its icon; a splash shows until the window appears. Start menu shortcut and Apps & features icon use it (older powershell.exe shortcuts are replaced by setup, update and repair); windows get the AC8 icon; consoles are titled "AC8 Integrated". Test-Launcher (dry run + the launcher smoke test hosted in the exe) in CI.
- Confirmation dialog takes its own title and action (updates and repair no longer show the cleanup wording).
- Tests: Test-App (installed app end to end), migration cases in Test-MouseSettings, modes_tests default.
- Also in 2.5.0 (prepared as 2.4.3, not released separately):
  - Other UE4SS mods: folders in the package's `ue4ss-mods` are validated (standard layout, name, no links, not reserved; `_` prefix disables) and deployed/enabled after this package's mods on every launch, removed with the runtime afterwards. Launcher button "附加 Mod" opens the folder and reports what loads; the console lists it. Bundles the UE4SS `shared` library (UEHelpers) of the same build. docs/EXTRA-MODS.md.
  - Flight-record switch: launcher checkbox (maintenance) / `flight_recording` in MouseAim-Settings.ini, off by default. Off: no FE trace and no Shadow CSV (the recorder is not armed), the optional analysis is skipped; the text log is kept. Applied at the next launch.
  - Sessions record UE4SS.dll and the mod DLL by hash (`runtime-dll-hashes.json`) instead of copying them (about 28 MB per launch).
  - Launcher: F2 HMD description matches the bracketed ring. Flight control unchanged from 2.4.2.
  - Tests: Test-MouseSettings (flight_recording, launcher in-place edit), Test-Cleanup (session archive without DLL copies), Test-InstallUX (analysis skip), hud_reference_tests (recording off by default).

## 2.4.2

- Fix: WAR keyboard takeover lag. A held key now takes over once the stock key ramp reaches 0.3 (about 0.1-0.15 s; was 0.8, 0.27-0.51 s), and the taken-over channel continues from the key ramp instead of re-ramping from the instructor's last command (never pulling back a command already in the key's direction). Reversing a held key keeps the keys in control. Hand-back about 0.1 s after release (at 0.8) and the WT ownership grouping are unchanged.
- Mouse flight control law unchanged (WAR v13.8 = v13.7 control law + takeover rule). Log line KEY_TAKEOVER on each handover.
- Fix: the launch console showed a fixed "AC8 2.3.5 ... F4 CLASSIC/AGILE ... candidate" text; it is now built from package-info.json / validation-status.json (Get-AC8ConsoleBanner).
- Tests: new public takeover_logic_tests (test.cmd now runs 24 suites) and Test-ConsoleBanner.

## 2.4.1

- Reference gun cross switch: Alt+F7 shows/hides it in flight (this session only, with a notice; the F1 panel shows the state). `hud_boresight=1/0` in MouseAim-Settings.ini sets the startup state (default shown). Plain F7 still toggles the whole mod HUD.
- Fix: the launcher key-settings dialog (Save, Restore defaults, key capture) no longer depends on the launcher running at global scope; it failed with "Save-AC8Keybindings is not recognized" when started via the call operator. Start-GUI.cmd users were not affected.
- Flight control unchanged from 2.4.0 (WAR v13.7, PEACE).
- Tests: new Test-MouseSettings; Test-Keybindings runs via the call operator in CI; native, GPU and Lua HUD tests cover the switch.

## 2.4.0

- Stable release. Offline single-player only.
- WAR is rebuilt on a reconstructed model of AC8's rotation law, with a War Thunder-style mouse-instructor structure:
  - path lock for straight nose paths;
  - smooth, bank-scaled levelling once the cursor rests (idle gate, minimum-jerk profile, no overshoot); levelling also resumes after a keyboard roll;
  - cursor-motion-aware roll decisions, used only near the nose (no roll wobble with the cursor far off-axis);
  - War Thunder-style keyboard takeover;
  - degraded fallback to WAR v11.1.
- F4 toggles PEACE (the unchanged CLASSIC 2.0, now the default) and WAR. AGILE 2.1 is retired; `control_mode` 1 migrates to 0, and 2/4/5 migrate to 3.
- HUD: a reference gun cross at the nose (War Thunder-style cross with a centre gap), drawn independently of the ring and also shown in C free look. The native game reticle sinks and does not match the gun line.
- HMD: the HMD ring now uses four corner brackets; while holding C, target switching picks along the view direction and the bracketed ring marks it.
- Includes the 2.3.16 frame-synchronous input and the 2.3.17 configurable keys.
- Fix: a rejected native-camera frame (invalid FOV on a cutscene or free-look transition) no longer disables the mod camera and HUD for the session; the camera now retries every 500 ms under all existing guards.
- See docs/RELEASE-v2.4.0.md, docs/WAR-FLIGHT-CONTROL.md, docs/NATIVE-UI-STYLE.md and docs/CAMERA-FAULT-RECOVERY.md.

## 2.3.17-keybindings-preview.1

Local configurable hold keys for free look, zoom and six manual-axis directions. Existing CreeperUX launcher gains key capture/save/defaults; validated numeric staging and one atomic runtime table. Default behavior and stock throttle/brake preserved.

## 2.3.16-phase-preview.1

Local frame-synchronous input candidate; QPC control delta, render-transform reticle motion and UI-only overlay without motion-frame reads/target locks. No public release or game acceptance claim. See docs/INPUT-PHASE-TEST.md.

## 2.3.15-beta.1

Optional public Beta of nativeui-preview.2. v2.3.6 remains stable. Native UMG ring, on-demand panel/toasts, in-session controls, native zoom transitions and disable tools. See docs/RELEASE-v2.3.15-beta.1.md for validation limits and rollback.

## 2.3.15-nativeui-preview.2

- Enlarge native ring visible diameter by 50%, with matching connector clearance.
- Project the captured mouse goal with its published camera snapshot; reuse that exact frame for the nose ticks.
- Keep an owned canvas stable; retain known canvas height through temporary unavailable geometry.
- No control-law changes, no display smoothing/prediction added. Actual jitter improvement needs game validation.

## 2.3.15-nativeui-preview.1

- Native cyan ring80 with dark outline, HMD inner ticks and directional ticks, owned texture retention and zero-geometry normalized tracking.
- Hybrid renderer3: native reticle; UI kit panel/toasts only in on-demand external overlay. No idle GPU submissions, panel20Hz/notifications60Hz.

## 2.3.14-umg-layout-preview.1

- Break hidden-widget/layout bootstrap dependency with a sized visible marker before target projection.
- Select laid-out visible root canvases, normalize placement, update after control context, and report each rejection stage.

## 2.3.13-umg-preview.1

- Attach a native UImage to a game-owned HUD canvas using existing ring texture; no external GUI or ReceiveDrawHUD dependency.
- Add Disable/Enable actions with verified owned cleanup, residue checks and original Steam-command passthrough. Never treat F8 as uninstall.
- Runtime UMG visibility and performance await offline game validation.

## 2.3.12-nativehud-preview.1

- Add renderer2 minimal in-game ReceiveDrawHUD ring probe, transient Canvas use and draw cadence/CPU diagnostics.
- Disable external overlay resources in this mode; no automatic overlay fallback. Actual game event/marshaling acceptance pending.

## 2.3.11-prediction-preview.1

- Isolated candidate from2.3.9; separate pose/camera QPC timestamps and add quantile timing diagnostics.
- Bounded display-only quaternion camera forecast, 8ms/half-frame horizon and0.35%-height pixel cap, stable-sample and lifecycle guards.
- Ctrl+End raw/predicted display; F1 state and bounds. No control, targeting or input architecture changes.

## 2.3.9-independent-preview.2

- Fix loss of mouse motion: independent target consumption no longer disables the proven Raw Input producer. DirectInput is used only if raw capture is unavailable.
- Add capture-to-consumption regression and raw nonzero packet diagnostics.

## 2.3.9-independent-preview.1

- Independent non-exclusive mouse polling advances shared target direction between control ticks; HUD and flight logic consume coherent target snapshots.
- Retain raw input fallback without double accumulation, lifecycle reset guards, native zoom envelope and automatic per-stage telemetry. Display interpolation defaults off.

## 2.3.8-smooth120-preview.2

- Add automatic per-stage HUD_PIPELINE frequency counters to distinguish Lua callback cadence, camera updates, lock contention and overlay submissions during testing.

## 2.3.8-smooth120-preview.1

- Display-only ring interpolation at the configured 120 Hz renderer rate; Ctrl+End toggles raw/smooth display. Adds approximately one source frame of visual delay without changing control or HMD targeting.
- Amplify native zoom excursion and preserve native hold/transition timing instead of stacking an instant zoom.
- Show source and submission rates separately.

## 2.3.7-settings-preview.4

- Add F1 free-look zoom row and Alt+PageUp/PageDown/Home session controls.
- Retry temporarily unavailable GPU queue at 1ms cadence; reread coherent latest POV before drawing. Add source-age diagnostics; keep configured HUD cap.

## 2.3.7-settings-preview.3

- Add configurable 1.75x additional optical zoom during C+RMB free look; retain native target-focus handling.
- Publish effective FOV to HUD, input projection and HMD sampling; preserve orbit geometry and post effects.

## 2.3.7-settings-preview.2

- Adapt the F1 panel and HMD/camera/control/sensitivity notifications to CreeperUX UI Kit dark tokens, bundled display/mono fonts, compact spacing and keycaps.
- Keep F1 immediate; use the kit 90 ms notification fade. Scale panels to the viewport, and separate simultaneous panel/notification placement.
- Verify generated tokens at native build and package creation; preserve controls and session-only sensitivity.

## 2.3.7-settings-preview.1

- Add F1 GPU HUD settings/diagnostics panel and Ctrl+PageUp/PageDown/Home runtime sensitivity controls.
- Changes are session-only, atomic, bounded and edge-triggered; no automatic calibration.
- Retain target-focus and lifecycle fixes.

## 2.3.6-hmd-preview.5

- Distinguish stock held target-focus from C free-look, yielding camera ownership for stock focus.
- Preserve the flight target across stock focus and release; keep scripted-camera recentering.
- Retain preview.4 lifecycle crash mitigation.

## 2.3.6-hmd-preview.4

- Remove runtime global lookups from missile visual polling, deferred equipment callbacks and HMD sampling.
- Invalidate visual callbacks on player EndPlay, LoadMap and world changes; preserve checkpoint mesh repair.
- Observe equipment only in the current BeginPlay event. Real-game teardown acceptance remains pending.

## 2.3.6-hmd-preview.3 — reticle A

- Show four inward ticks on the mouse ring while HMD is enabled; preserve the original ring while disabled.
- Keep the F2 transient notice and F3/F4 notice styling unchanged. Reticle state persists after the notice fades.
- Mirror the mode shape in the legacy renderer and invalidate its shape cache on mode changes.

## 2.3.6-hmd-preview.2 — local fix

- Fix HMD UNAVAILABLE when MinHook cannot allocate a near executable page (MH_ERROR_MEMORY_ALLOC).
- Add a version-checked absolute trampoline that relocates 21 complete position-independent prologue bytes; retain the original player-only dispatch guards.
- Test the executable detour/trampoline roundtrip, not just ranking and dispatch policy.

## 2.3.6-hmd-preview.1 — local candidate

- F2 toggles cursor-priority target selection, off at process start. Original switch-target binding is retained.
- Reuses the F3/F4 GPU notice style. Invalid/stale data falls back to stock selection.
- Tested geometry, dispatch guards and renderer; actual mission selection remains pending.

# Changelog

## 2.3.5 camera preview2 — local candidate

- Adds native-game/far-follow camera selection using F3 in game; defaults far-follow to36m/6m with configurable distance/height.
- Publishes HUD from actual final camera position in both modes; preserves game FOV.
- Adds mission/cinematic ownership gating and one-shot recenter after stable control return; preserves target across C free-look and view switches.
- Inherits launcher audit and three install scopes, with the2.3.2 flight policy unchanged. Real-flight acceptance pending.

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
