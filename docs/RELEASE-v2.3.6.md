# AC8 Integrated v2.3.6

[简体中文](#简体中文) | [English](#english)

## 简体中文

**英语补充包已提供：** `AC8-Integrated-v2.3.6-English.zip`。它与中文包的 28 个游戏运行文件完全一致，仅安装器、提示和随包使用说明为英语。v2.3.6 不加入语言切换，2.3.7 再实现。原中文 ZIP 及校验值保留。

当前作者按键参考已更新：起落架为 G，目标切换为 F（备用鼠标右键）。

**严禁用于线上或多人模式，仅限离线单人使用。**

**正式推荐版本，建议新用户与旧版用户统一下载本页的完整安装包。**

本版整合鼠标飞控、摄像机切换、头盔瞄准具（HMD）、三种安装范围，以及 图形安装器。

本次为已验证 v2.3.6 安装包的正式发布状态升级，安装包内容与 SHA-256 保持一致。包内个别“预发布”文字是原构建标识；当前发布状态以本页为准。

### 下载与安装

下载 **AC8-Integrated-v2.3.6-share.zip**，完整解压到游戏目录之外，双击 **Start-GUI.cmd**。核对自动定位的 Steam 和游戏位置，选择安装范围并保存，再把生成的启动选项粘贴到 Steam。游戏操纵类型选择 Expert／专家，使用第三人称视角。请保留启动控制台，正常退出后等待清理。

### 主要更新

- 三种范围均包含飞控、摄像机与 HMD：仅比例引导、完整导弹强化、仅飞控。
- 仅比例引导不改动原版性能、锁定、伤害、装填、近炸与模型；完整强化以接近《战争雷霆》表现为目标大幅调整性能，不是其仿真的完全复刻。
- F2 开关 HMD，每次启动默认关闭；四向刻度与原有通知显示开关状态。HMD 不绕过武器锁定条件。
- F3 两种机位与 C 自由观察，包含任务／过场后的回中处理。
- 包含任务退出的生命周期修复，并保留检查点外观恢复。
- 安装界面支持自动定位 Steam 和游戏、手动选择路径、备份清理及查看诊断。
- 首页增加当前 GUI 图片以及作者实际使用的飞行／视角键位参考，安装器不会改写键位。

### 验证与限制

已完成原生、Lua、PowerShell、GUI 及完整包检查，用户已批准公开发布。本版已转为正式发布；不同设备、任务、机型、HUD 驱动与退出任务／回机库／检查点重生的组合未获全面覆盖。不能据模拟测试保证所有 UE4SS 原生崩溃均已消除。

升级请使用新解压目录，并更新 Steam 启动选项。旧包不会自动更新。分享原始 ZIP，不要转发使用后的 sessions 或存档备份。

- 项目说明：https://github.com/CreeperUX/AC8-Integrated-Mod
- 安装：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/INSTALL.md
- 飞行与视角键位：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/KEYBINDINGS.md
- 已知限制：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/LIMITATIONS.md

基于 FletcherMiya/AC8-Mouse-Aim 发展，感谢原作者及贡献者；完整署名和许可随包保留。

## English

**Offline single-player only. Do not use in online or multiplayer modes.**

v2.3.6 is the recommended formal release. Download **AC8-Integrated-v2.3.6-share.zip**, extract it outside the game directory and run **Start-GUI.cmd**. Set Expert controls and use a third-person view. Keep the launcher console open until cleanup finishes after exiting.

**English supplement:** download **AC8-Integrated-v2.3.6-English.zip** for the English-only installer, prompts and player guides. Its 28 gameplay files are byte-identical to the Chinese release. No language switch is included in 2.3.6; switching is planned for 2.3.7. The existing Chinese ZIP and checksum are retained. Windows system dialogs may follow the OS language.

The author's current key reference has also been updated: G for landing gear and F / right mouse button for target switching.

### Features

- Mouse-directed flight control, F3 camera-position switching and C free look.
- F2 cursor-prioritized HMD target selection, disabled at startup, with persistent reticle ticks and notifications. Normal weapon-lock conditions still apply.
- Three modes: guidance only, full missile enhancements, or flight control with stock missiles. All include the camera and HMD features.
- Guidance-only preserves stock missile performance, fuze settings and appearance. Full enhancements aim to approach War Thunder-like behavior within AC8's mechanics; they are not a complete reproduction of its simulation.
- Automatic Steam/game discovery, manual path selection, verified backup cleanup and diagnostics.
- Lifecycle fixes for mission-exit lookup risks while retaining checkpoint appearance recovery.

### Installation and upgrades

Save settings, copy the generated launch-options line, and paste it into Steam → AC8 → Properties → General → Launch Options. Launch through Steam. Use a new extraction directory for upgrades and update the launch option. The Source code ZIP is not the runtime package.

The original v2.3.6 ZIP and SHA-256 are retained unchanged. Any prerelease wording inside that original build is a historical label; this release page is authoritative for its formal release status.

### Validation and feedback

Native, Lua, PowerShell, GUI and package checks have passed. Coverage is not exhaustive across devices, missions, aircraft, graphics drivers or lifecycle transitions. Simulated tests cannot guarantee elimination of all native UE4SS crashes.

Report the game build, aircraft, selected mode, F2/F3/F4 states and reproduction steps. Share the original ZIP, not used package directories containing sessions or save backups.

- English overview: https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/README.en.md
- Flight/camera bindings: https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/en/KEYBINDINGS.md
- Cleanup: https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/en/CLEANUP.md
- Known limitations: https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/en/LIMITATIONS.md

Based on FletcherMiya/AC8-Mouse-Aim. Thanks to the original author and contributors; credits and licenses are retained.
