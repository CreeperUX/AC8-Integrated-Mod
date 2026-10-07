# AC8 Integrated Mod — v2.4.0

**正式版。仅限离线单人，禁止用于线上或多人游戏。**

鼠标指定方向，Mod 协调飞机姿态飞向圆环。按 **F4** 在两种飞控之间切换：
- **PEACE**（默认）：原 CLASSIC 2.0；
- **WAR**：2.4.0 全新飞控。基于还原的 AC8 转动规律，决策结构参考 War Thunder 鼠标教官；机头走直线，鼠标停下后平滑改平，来回拖动时反应更快，按住操纵键可按 War Thunder 规则接管。

本版还新增：
- 机头参考准星，按住 C 自由观察时也显示；
- 自由观察中的头瞄；
- 逐帧同步输入与自定义按键；
- 修复过场后模组相机／HUD 不恢复的问题。

- [v2.4.0 更新、安装及回退说明](docs/RELEASE-v2.4.0.md) · [WAR 飞控说明](docs/WAR-FLIGHT-CONTROL.md)
- [安装与三种范围](docs/INSTALL.md) · [游戏内调整](docs/IN-GAME-SETTINGS.md) · [自定义键位](docs/CUSTOM-KEYBINDINGS.md) · [快捷停用](docs/MOD-DISABLE.md)
- [按键参考](docs/KEYBINDINGS.md) · [控制原理](docs/CONTROL.md) · [原生 HUD 与参考准星](docs/NATIVE-UI-STYLE.md) · [头盔瞄准](docs/HELMET-SELECTION.md) · [已知限制](docs/LIMITATIONS.md)
- [GitHub 下载](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.4.0)

## 安装

下载完整的 share.zip，不要用 Source code ZIP 安装。解压到游戏目录之外的新文件夹，运行 Start-GUI.cmd 或 Setup.cmd，保存后把新生成的 Steam 启动选项替换进去。退出游戏时等待控制台清理完成。

## 从旧版本升级

- 不要覆盖旧目录，也不要复制旧版的 DLL、设置或 sessions。
- 旧设置中的 `control_mode` 会自动迁移：1 → PEACE，2/4/5 → WAR。
- v2.3.15-beta.1 默认是 AGILE 2.1，此版本默认 PEACE。

**不保证原生圆环达到 120 Hz，也不保证 F1／通知对所有设备零性能影响。** WAR 未覆盖所有机型和任务，已知边界见发布说明。

基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展，感谢原作者及贡献者。完整署名和许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

[English overview](https://github.com/CreeperUX/AC8-Integrated-Mod/blob/release/v2.4.0/README.en.md)
