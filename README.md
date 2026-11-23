# AC8 Integrated Mod — v2.5.3

**正式版。仅限离线单人，禁止用于线上或多人游戏。**

**2.5.3 更新**：适配 2026 年 11 月 2 日的游戏更新（Steam Build 26002268），原生相机和头瞄选目标在新版游戏中照常可用。**2.5.2**：附加 Mod 会完整安装 Mod 自带的资源文件（例如 PW-MkI 的飞机资源），退出游戏后附加 Mod 不再重复备份。**2.5.1**：提供 Windows 安装程序，从 GitHub 一键更新、自动检查修复，Steam 启动选项以后不用再改；默认飞控改为 WAR（F4 可切回 PEACE）。还可以和其他 UE4SS Mod 一起使用（启动器中添加 zip 或文件夹即可），启动器新增“保存飞行记录”开关（默认关闭），会话不再复制 DLL。**2.4.2**：修复 WAR 中键盘接管的迟滞。2.4.1 起，机头参考准星（十字）可用 **Alt+F7** 开关。

鼠标指定方向，Mod 协调飞机姿态飞向圆环。按 **F4** 在两种飞控之间切换：
- **WAR**（默认）：基于还原的 AC8 转动规律，决策结构参考 War Thunder 鼠标教官；机头走直线，鼠标停下后平滑改平，来回拖动时反应更快，按住操纵键可按 War Thunder 规则接管。
- **PEACE**：原 CLASSIC 2.0。

还包括：
- 机头参考准星，按住 C 自由观察时也显示；
- 自由观察中的头瞄；
- 逐帧同步输入与自定义按键；
- 修复过场后模组相机／HUD 不恢复的问题。

- [v2.5.3 更新说明](docs/RELEASE-v2.5.3.md) · [v2.5.2 更新说明](docs/RELEASE-v2.5.2.md) · [v2.5.1 更新说明](docs/RELEASE-v2.5.1.md) · [安装版说明](docs/APP.md) · [v2.4.2 更新说明](docs/RELEASE-v2.4.2.md) · [v2.4.1 更新说明](docs/RELEASE-v2.4.1.md) · [v2.4.0 更新说明](docs/RELEASE-v2.4.0.md) · [WAR 飞控说明](docs/WAR-FLIGHT-CONTROL.md)
- [安装与三种范围](docs/INSTALL.md) · [游戏内调整](docs/IN-GAME-SETTINGS.md) · [自定义键位](docs/CUSTOM-KEYBINDINGS.md) · [快捷停用](docs/MOD-DISABLE.md)
- [按键参考](docs/KEYBINDINGS.md) · [控制原理](docs/CONTROL.md) · [原生 HUD 与参考准星](docs/NATIVE-UI-STYLE.md) · [头盔瞄准](docs/HELMET-SELECTION.md) · [其他 UE4SS Mod](docs/EXTRA-MODS.md) · [已知限制](docs/LIMITATIONS.md)
- [GitHub 下载](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.5.3)

## 安装

推荐使用安装程序 **AC8-Integrated-Setup-v2.5.3.exe**：退出游戏后运行，按提示导入便携版设置，再把显示的一行替换进 Steam 启动选项。以后从开始菜单打开 AC8 Integrated，更新和修复都在启动器里。详见 [安装版说明](docs/APP.md)。

也可以继续使用便携版：下载完整的 share.zip（不要用 Source code ZIP），解压到游戏目录之外的新文件夹，双击 AC8 Integrated.exe（或运行 Setup.cmd），保存后把新生成的 Steam 启动选项替换进去。退出游戏时等待控制台清理完成。

## 从旧版本升级

- 安装版：直接运行安装程序，可导入旧便携版的设置；旧便携版原样保留，确认无误后可自行删除。
- 便携版：不要覆盖旧目录，也不要复制旧版的 DLL 或 sessions。
- 2.5 起默认飞控为 WAR。以前保存为 PEACE 的设置会改为 WAR 一次；之后按 F4 或设置 `control_mode=0` 改回 PEACE，不会再被改动。旧值 1 → PEACE，2/4/5 → WAR。
- 设置文件里没有 `hud_boresight` 时，参考准星按显示处理。

**不保证原生圆环达到 120 Hz，也不保证 F1／通知对所有设备零性能影响。** WAR 未覆盖所有机型和任务，见 [已知限制](docs/LIMITATIONS.md)。

基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展，感谢原作者及贡献者。完整署名和许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

[English overview](README.en.md)
