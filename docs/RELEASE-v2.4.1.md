# v2.4.1 — 参考准星开关

**正式版。仅限离线单人，禁止带 Mod 进入线上或多人模式。** 飞控（WAR v13.7、PEACE）与 v2.4.0 完全相同。

## 更新内容

- **参考准星开关**：
  - 飞行中按 **Alt+F7** 显示／隐藏机头参考准星（十字），并弹出“REFERENCE GUN CROSS”提示。只对本次运行有效。
  - 启动时是否显示由 `MouseAim-Settings.ini` 的 `hud_boresight` 决定：1 显示（默认），0 隐藏。关闭游戏后修改，下次启动生效。
  - F1 面板新增“Gun cross”一行，显示当前状态。
  - 单按 F7 仍是显示／隐藏整个 Mod HUD；同时按住 Ctrl 或 Shift 时，Alt+F7 不切换。
  - 隐藏十字不影响鼠标圆环、方向刻度和头瞄圆环。
- **修复：启动器“键位设置”对话框**：
  - 保存、恢复默认和按键录入的按钮在启动器不以 `-File` 方式运行时，会报错找不到函数（例如 `Save-AC8Keybindings`）。
  - 现在无论以何种方式启动都能正常工作。通过 Start-GUI.cmd 正常启动的用户此前不受影响。

## 升级

与 v2.4.0 相同：
- 下载完整的 **AC8-Integrated-v2.4.1-share.zip**，解压到游戏目录之外的新目录；
- 运行 Start-GUI.cmd 或 Setup.cmd，并把新生成的 Steam 启动选项替换进去；
- 不要复制旧版的 DLL、设置或 sessions。

设置文件里没有 `hud_boresight` 时，参考准星按显示处理。安装和已知边界见 [v2.4.0 说明](RELEASE-v2.4.0.md)。v2.4.0 的发布已合并到 v2.4.1，不再单独提供下载；回退请使用 [v2.3.15-beta.1](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.15-beta.1) 或 [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6)，并替换为其目录生成的启动选项。

## 验证

- **原生测试**：全部通过（公开源码自带 23 项及 GPU HUD 测试）。新增覆盖：F7／Alt+F7 按键判定、十字默认显示，以及提示和 F1 面板的渲染预览。
- **HUD 脚本离线测试**：
  - 十字关闭时，圆环、方向刻度和头瞄圆环不受影响，重新打开后十字恢复；
  - 旧版 DLL 没有这个值时，十字照常显示。
- **设置文件测试**（新增）：
  - `hud_boresight` 的 0／1 正确写入运行配置；
  - 2.4.0 的运行配置缺少这一行时会自动补上；
  - 非法值会被拒绝。
- **键位设置对话框测试**：改用之前失败的调用方式运行，现在通过。
- **实机确认**：已在游戏中确认 Alt+F7 切换十字、提示和 F1 面板状态行，单按 F7 仍开关整个 HUD；日志记录 `HUD_BORESIGHT`。

## English summary

**Stable release. Offline single-player only.** Flight control (WAR v13.7, PEACE) is unchanged from v2.4.0.

- **Reference gun cross switch.**
  - **Alt+F7** shows or hides the nose reference gun cross in flight, with a notice. It applies to this session only.
  - `hud_boresight` in MouseAim-Settings.ini sets the startup state: 1 = shown (default), 0 = hidden.
  - The F1 panel shows the current state.
  - Plain F7 still toggles the whole mod HUD; Alt+F7 does nothing while Ctrl or Shift is held.
  - Hiding the cross does not affect the mouse ring, connector ticks or the HMD ring.
- **Fix.** The launcher key-settings dialog (Save, Restore defaults, key capture) failed with "Save-AC8Keybindings is not recognized" when the launcher was not run with `-File`. It now works however it is started; Start-GUI.cmd users were not affected.

Checked in game: Alt+F7 toggles the cross with its notice and the F1 panel row; plain F7 still toggles the HUD.

Install as for v2.4.0: extract the complete share ZIP to a new folder outside the game, run Start-GUI.cmd or Setup.cmd and replace the Steam launch option. A settings file without `hud_boresight` keeps the cross shown. See the [v2.4.0 notes](RELEASE-v2.4.0.md) for installation and known limits. v2.4.0 is no longer offered separately (its release was merged into v2.4.1); to roll back, use [v2.3.15-beta.1](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.15-beta.1) or [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6).
