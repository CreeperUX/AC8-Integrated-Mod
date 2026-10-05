# Author's flight and camera bindings

[简体中文](../KEYBINDINGS.md) | **English** · [English home](../../README.en.md)

Verified from the author's saved AC8 settings on **2026-10-05**. These are personal bindings, not game defaults. Only flight, camera and HMD-related target switching are included. Primary and secondary keys are alternatives, not chords. The game control type is **Expert**.

Current correction: landing gear is **G**, and target switching is **F** (right mouse button remains the alternative). Older bundled references using R/T describe the previous configuration.

## Saved game bindings

| Action | Primary | Secondary |
|---|---|---|
| Pitch down | W | Unbound |
| Pitch up | S | Unbound |
| Roll left | A | Unbound |
| Roll right | D | Unbound |
| Yaw left | Q | Unbound |
| Yaw right | E | Unbound |
| Accelerate / increase throttle | Left Shift | Unbound |
| Decelerate / brake | Left Ctrl | Unbound |
| Native autopilot | Z | Unbound |
| Landing gear | G | Unbound |
| Native camera control | C | Left Alt |
| Look up | Number row 7 | Numpad 8 |
| Look down | Number row 8 | Numpad 2 |
| Look left | Number row 9 | Numpad 4 |
| Look right | Number row 0 | Numpad 6 |
| Native view switch | V | Unbound |
| Switch target (HMD-related) | F | Right mouse button |

The installer does not modify these settings. Apply them manually in the game if desired. The [structured reference](../KEYBINDINGS.json) contains only selected binding fields; it is not an importable game save.

## Mod shortcuts

| Input | Function |
|---|---|
| Mouse movement | Set a flight target direction; the controller coordinates aircraft attitude |
| Hold C + move mouse | Mod free look; releasing C preserves the flight target |
| F2 | Toggle HMD; disabled at startup, with four inward reticle ticks while enabled |
| F3 | Switch native relative / farther follow-camera position |
| F4 | Switch CLASSIC / AGILE control policy |
| F5 | Performance diagnostics |
| F6 | Camera diagnostics |
| F7 | Show or hide the mod HUD and its notifications |
| F8 | Toggle mouse flight control; does not uninstall the missile module |
| F9 | Recenter the aiming direction |
| F10 | Reload the current runtime copy of settings and recenter; package-root edits normally apply next launch |

## Important distinctions

- **C / Left Alt:** both are saved native camera bindings, but mod free look checks C directly. Use C for aircraft-centered mod observation; Left Alt is not an equivalent mod shortcut.
- **V / F3:** V switches the native game view; F3 changes the mod's camera-position profile. Use the game's third-person view as instructed.
- **F2 / F3 / F4:** release Shift, Ctrl and Alt before pressing them. Since throttle/brake use Left Shift/Left Ctrl here, holding them may prevent a toggle.
- **W/S, A/D, Q/E:** manual override detection currently uses these fixed keys; W/S also yield automatic roll. Rebinding only the game does not automatically update that detection.
- **F / right mouse button:** use the original target-switch action after enabling HMD. Candidate priority changes, not weapon range, lock angle or lock time.
- Number-row keys and F-keys are different. Native autopilot, landing gear and directional camera actions depend on game context; this reference records saved bindings, not exhaustive live testing of every combination.

No original System.sav, account data or campaign progress is provided.
