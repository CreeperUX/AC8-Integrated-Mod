# HMD target selection

[English home](../../README.en.md) · [Key reference](KEYBINDINGS.md) · [Implementation record (Chinese)](../HELMET-SELECTION.md)

All three installation modes include HMD. Press **F2** during active flight with mouse control enabled to toggle it. It starts **off** each game launch. Release Shift, Ctrl and Alt first; it is unavailable during pause, scripted control or free look.

With HMD enabled, use your normal target-switch action. In the author's current configuration this is **F**, with **right mouse button** as an alternative. The mod prefers another valid target near the mouse aiming circle. If only the current target qualifies, it stays selected. Missing, unsuitable or stale candidates fall back to the native selection path.

Four short inward ticks appear on the aiming ring while HMD is enabled. The F2 ON/OFF/UNAVAILABLE notification remains, but its fade does not remove the persistent enabled-state ticks. F7 hides the mod HUD, including these indicators. The legacy renderer does not provide all GPU text notifications.

HMD affects target choice, not weapon lock completion. Range, lock angle and lock time remain game constraints. Holding C does not turn the observation direction into a new flight or HMD aiming target.

The feature uses only candidates supplied by the game; it cannot guarantee that a target outside the game's candidate filtering will be available. It is build-specific. If HMD reports UNAVAILABLE, keep the diagnostic output and check the supported game version.

The release includes the preview.4 lifecycle fix. Continue to test target switching, camera changes, mission exit and checkpoints on your hardware. Simulation coverage is not proof of every live-game combination.
