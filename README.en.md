# AC8 Integrated Mod — v2.5.3

Stable release. **Offline single-player only; do not use online or in multiplayer.**

**2.5.3:** supports the 2026-11-02 game update (Steam build 26002268); the native camera and HMD target selection work on it. **2.5.2:** extra UE4SS mods keep the asset files in their own folders (for example PW-MkI), and deployed extra mods are no longer archived after every session. **2.5.1:** a Windows setup program with one-click updates from GitHub, self-checks and repair, and a Steam launch option that never needs changing again; WAR is now the default flight control (F4 switches to PEACE). Other UE4SS mods can be used alongside (add a zip or folder in the launcher), a "save flight records" switch in the launcher (off by default), and sessions no longer copy the DLLs. **2.4.2:** fixes the keyboard-takeover lag in WAR. Since 2.4.1 the nose reference gun cross can be switched with **Alt+F7**.

The mouse sets a direction and the mod coordinates the aircraft's attitude to fly to the ring. **F4** toggles two flight controllers:
- **WAR** (default): built on a reconstructed model of AC8's rotation law with a War Thunder-style mouse-instructor structure: straight nose paths, smooth levelling once the mouse rests, quicker reversals, and War Thunder-style keyboard takeover.
- **PEACE**: the unchanged CLASSIC 2.0.

Also included:
- a reference gun cross at the nose, visible in C free look as well;
- HMD target selection along the view direction while holding C;
- frame-synchronous input and configurable keys;
- a fix for the mod camera/HUD not returning after cutscenes.

[2.5.3 changes](docs/RELEASE-v2.5.3.md) · [2.5.2 changes](docs/RELEASE-v2.5.2.md) · [2.5.1 changes](docs/RELEASE-v2.5.1.md) · [the installed app](docs/APP.md) · [2.4.2 changes](docs/RELEASE-v2.4.2.md) · [2.4.1 changes](docs/RELEASE-v2.4.1.md) · [2.4.0 changes (English summary at the end)](docs/RELEASE-v2.4.0.md).

Recommended: run **AC8-Integrated-Setup-v2.5.3.exe** with the game closed, import the portable package's settings when asked, and paste the shown line into Steam's launch options. Afterwards open AC8 Integrated from the Start menu; updates and repair are in the launcher. The interface is Chinese.

The portable **AC8-Integrated-v2.5.3-share.zip** remains available (not the Source code ZIP): extract it to a new folder outside the game, run AC8 Integrated.exe (or Setup.cmd), and replace the Steam launch option with the newly generated line. Do not mix payloads, settings or used session folders between versions.

A saved PEACE selection moves to WAR once with 2.5; switching back with F4 or `control_mode=0` is kept afterwards.

Based on [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim). Thanks to the original author and contributors; [licenses and credits](THIRD_PARTY_NOTICES.md) are retained.

[简体中文](README.md)
