# Camera positions and free look

[English home](../../README.en.md) · [Key reference](KEYBINDINGS.md) · [Implementation record (Chinese)](../CAMERA-CONTEXT.md)

Use the game's third-person view. **F3** switches the mod's camera-position profile during flight:

- **Native relative position:** preserves the game's relative framing while retaining mod mouse-follow orientation and aircraft-centered observation.
- **Farther follow position:** defaults to 36 metres behind and 6 metres above the aircraft, without changing the game's FOV or post-processing.

The profiles share orientation following; switching does not reset the flight target. F3 changes only the current session. Release Shift, Ctrl and Alt before pressing it. It is gated while paused, observing or under scripted control.

Hold **C** and move the mouse for free look around the aircraft. Releasing C preserves the flight target. Short and long holds use the same mod observation mechanism; scripted and impact cameras retain priority. The native **V** view-switch binding is separate from F3. The game's saved Left Alt alternative is not an equivalent mod C shortcut.

To change startup settings, close the game and edit `MouseAim-Settings.ini`:

```ini
camera_mode=1
camera_distance_m=36
camera_height_m=6
```

`camera_mode=0` selects native relative framing; `1` selects farther follow. Distance accepts 10–100 metres and height 0–20 metres, affecting only the farther profile. F10 reloads the current runtime copy; package-root edits normally apply next launch.

After mission initialization, a new player aircraft, scripted-camera handback or pause recovery, the controller waits for a short stable interval and recenters once. Releasing C does not use that scripted-camera recenter path. Press F9 for a manual recenter.

Different missions and camera transitions still require live testing. If reporting a camera problem, include F3 mode, C state, aircraft, mission and the steps immediately before the issue.
