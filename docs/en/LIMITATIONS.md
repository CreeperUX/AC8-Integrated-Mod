# Known limitations and validation scope

[简体中文](../LIMITATIONS.md) | **English** · [English home](../../README.en.md)

- Only the pinned game build and UE4SS ABI are supported. A game update can cause version validation to refuse launch.
- v2.4.2 is the current stable release. WAR has been flown on several aircraft, but not every computer, mission, aircraft or binding configuration is covered.
- The independent-axis model does not fully model aerodynamic coupling, angle of attack, G-loads, stalls or energy management. (This applies to PEACE; WAR uses a reconstructed model of the game's rotation law but does not change aircraft performance.)
- Learning is not guaranteed for every axis or flight and does not persist between game processes. Validated speed-range coverage is incomplete.
- HUD submissions and a 120 Hz target do not guarantee display FPS. The GDI fallback does not show the same text notifications as the GPU renderer; consult console logs.
- Native game bindings can conflict with mod keys. The installer does not change game bindings. See the [flight/camera reference](KEYBINDINGS.md).
- Flight-only mode means this package does not modify missiles; it does not control other mods you have installed.
- Proximity-fuze field readback is not proof of actual triggering for every weapon. Four ready LAAM rounds have not been confirmed on every aircraft.
- Checkpoint appearance repair applies only to recognized local components. Launched missiles with collision enabled, geometry merged into other meshes and unknown material layouts can be skipped.
- Mounted-appearance maintenance checks at most 16 known cached entries every 60 game-thread frames. It does not perform a global scan or repeatedly write stable objects. Timing depends on frame scheduling.
- HMD selection does not bypass weapon locks. Its candidate set is provided by the game, samples expire, and invalid conditions fall back to native selection.
- The lifecycle fixes have simulated regression coverage but cannot prove that all native UE4SS crashes are eliminated. Mission exit, hangar return and checkpoint transitions remain important feedback areas.
- Steam launching preserves its Cloud workflow but does not resolve pre-existing sync conflicts automatically. Save backups and logs may contain personal information; do not publish them wholesale.

## What the tests establish

Native tests cover control endpoints, transitions, learning-state retention, model error, limits, fallbacks, camera context, target selection and recording. Lua simulations cover weapon-mode separation, object validity, lifecycle handling, checkpoint resets, material rollback and target selection. Installer tests cover staging, mode/readiness matching, cleanup, paths, host behavior and GUI interaction.

These are distinct from real game startup, external appearance, hit behavior and subjective handling. Simulated response-time changes must not be advertised as in-game performance percentages.
