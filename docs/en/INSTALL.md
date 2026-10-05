# Installation, modes and upgrades

[简体中文](../INSTALL.md) | **English** · [English home](../../README.en.md)

**Offline single-player only. Never use this mod in online or multiplayer modes.**

Use the [recommended v2.3.6 release](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/latest), compatible with Windows x64 and Steam game build 25201480. Choose the English ZIP for a fixed English interface, or the separate Chinese ZIP. In-app language switching is planned for 2.3.7. Windows system dialogs may follow your Windows language.

## First installation

1. Exit the game normally and wait for the previous launcher to finish cleanup.
2. Download the full release ZIP, not the Source code ZIP. Extract it outside the game directory, for example `D:\Mods\AC8-Integrated`.
3. Run `Start-GUI.cmd`. It detects Steam and AC8 and performs read-only checks. Multiple candidates are offered for selection. Manual input is retained; you can enter the root, `Win64` directory or EXE path.
4. Check both paths, select a mode, and click **保存设置 — Save settings**.
5. Click **复制启动选项 — Copy launch options**. Paste the entire line into Steam → AC8 → Properties → General → Launch Options.
6. Click **从 Steam 启动 — Launch through Steam**, or start the game from Steam. Select Expert controls and use a third-person view.
7. Keep the launcher console open during play. After exiting, wait for cleanup and Steam Cloud synchronization.

### Button names in the two packages

| Chinese label | Meaning |
|---|---|
| 游戏位置 | Game location |
| Steam 程序 | Steam executable |
| 浏览… | Browse… |
| 自动定位 | Detect installations |
| 检查环境 | Check environment |
| 仅比例引导 | Guidance only, with flight control |
| 完整导弹强化 | Full missile enhancements, with flight control |
| 仅鼠标飞控 | Flight control only |
| 保存设置 | Save settings |
| 复制启动选项 | Copy launch options |
| 从 Steam 启动 | Launch through Steam |
| 备份并清理 | Back up and clean up |
| 诊断文件夹 | Open diagnostics folder |
| 切换浅色 / 切换深色 | Switch to light / dark theme |

## Installation modes

| Mode | Console menu | Saved value |
|---|---:|---|
| Flight control + proportional guidance only | 1 | `missile_mode=guidance` |
| Flight control + full missile enhancements | 2 | `missile_mode=full` |
| Flight control only, stock missiles | 3 | `missile_mode=none` |

All modes include camera functions and HMD. Guidance-only changes player-missile guidance while retaining stock performance, lock, damage, reload, ready-round, proximity-fuze and appearance settings. Full mode enables the existing performance and visual changes. Flight-only does not install the missile module.

The choices are independent. A new package defaults to full enhancements. Old `missile_enhancement=1/0` values read as `full/none`; saving writes the new format. Do not store several mode entries together. Menu numbers are not configuration values.

To change a mode, exit the game, wait for cleanup, select a different mode in the GUI and save. Alternatively run `Choose-Features.cmd`. The change takes effect next launch; it does not require new Steam options unless the package location changed. F2/F3/F4/F8 are runtime controls, not installation-mode switches.

## Upgrades and removal

Extract a new version into a new directory. Finish cleanup in the old package, configure the new one and replace Steam launch options with its generated line. Do not run multiple packages at once or copy over an unfinished session.

If a loader conflict remains, use [backup cleanup](CLEANUP.md) after confirming ownership. To return to the original game, finish cleanup and remove this package's Steam launch option. F8 alone does not remove missile changes.

## Flight and camera settings

Edit `MouseAim-Settings.ini` while the game is closed. These settings apply at the next launch.

| Setting | Default | Meaning |
|---|---:|---|
| `control_mode` | 1 | 0 = CLASSIC, 1 = AGILE; F4 switches during flight |
| `sensitivity` | 0.10 | Mouse-direction sensitivity |
| `model_assist` | 1 | Enable model-based control |
| `model_assist_strength` | 0.20 | Mixing scale for validated learned parameters; not a simple controller gain |
| `hud_fps` | 120 | Target HUD submission rate, not guaranteed display FPS |
| `follow_rate` / `level_rate` | 8 / 3 | Camera-direction following and horizon return |
| `camera_mode` | 1 | 0 = native relative position, 1 = farther follow position |
| `camera_distance_m` / `camera_height_m` | 36 / 6 | Farther camera distance and height in metres |

F10 reloads the current runtime copy and recenters; root-directory edits normally apply on the next launch. F3 changes camera position during flight. C free look preserves the flight target direction when released. See the [key reference](KEYBINDINGS.md).

## Checks and diagnostics

Automatic detection and the GUI environment check are read-only. Saving performs a temporary write probe; it does not guarantee that a DLL will not be blocked separately. Inspect the relevant file's permissions and security-software protection history if deployment is denied.

A full read-only session preflight is available with system Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Console.ps1 -Action Session -CheckOnly
```

It checks the entry path without launching, staging, cleaning or saving settings. An unfinished session is reported rather than modified. Game-directory junctions and symbolic links are rejected consistently with cleanup. Concurrent operations must finish before another launch or cleanup starts.

Python and NumPy are optional analysis dependencies. Missing dependencies do not prevent ordinary gameplay or cleanup. `sessions` may contain personal save backups; never share that directory in full. Keep the original release ZIP for sharing.
