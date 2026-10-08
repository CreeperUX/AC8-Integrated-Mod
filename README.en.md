# AC8 Integrated Mod — v2.4.2

Stable release. **Offline single-player only; do not use online or in multiplayer.**

**2.4.2:** fixes the keyboard-takeover lag in WAR: a held pitch, roll or yaw key takes over after about 0.1-0.15 s and responds essentially like the stock keys. Since 2.4.1 the nose reference gun cross can be switched with **Alt+F7**, and `hud_boresight` in MouseAim-Settings.ini sets its startup state.

The mouse sets a direction and the mod coordinates the aircraft's attitude to fly to the ring. **F4** toggles two flight controllers:
- **PEACE** (default): the unchanged CLASSIC 2.0;
- **WAR**: new in 2.4.0. It is built on a reconstructed model of AC8's rotation law with a War Thunder-style mouse-instructor structure: straight nose paths, smooth levelling once the mouse rests, quicker reversals, and War Thunder-style keyboard takeover.

Also new:
- a reference gun cross at the nose, visible in C free look as well;
- HMD target selection along the view direction while holding C;
- frame-synchronous input and configurable keys;
- a fix for the mod camera/HUD not returning after cutscenes.

[2.4.2 changes](docs/RELEASE-v2.4.2.md) · [2.4.1 changes](docs/RELEASE-v2.4.1.md) · [2.4.0 changes, installation, limitations and rollback instructions (English summary at the end)](docs/RELEASE-v2.4.0.md).

Download the complete **AC8-Integrated-v2.4.2-share.zip**, not the Source code ZIP. The installer UI is Chinese. Extract it to a new folder outside the game, run Start-GUI.cmd or Setup.cmd, and replace the Steam launch option with the newly generated line. To roll back, finish 2.4.2 cleanup, install v2.3.15-beta.1 or v2.3.6 separately and use its launch option. Do not mix payloads, settings or used session folders.

Based on [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim). Thanks to the original author and contributors; [licenses and credits](THIRD_PARTY_NOTICES.md) are retained.

[简体中文](README.md)
