# AC8 Integrated v2.3.6

**严禁用于线上或多人模式，仅限离线单人使用。**

**正式推荐版本，建议新用户与旧版用户统一下载本页的完整安装包。**

本版整合鼠标飞控、摄像机切换、头盔瞄准具（HMD）、三种安装范围，以及 CreeperUX 图形安装器。

本次为已验证 v2.3.6 安装包的正式发布状态升级，安装包内容与 SHA-256 保持一致。包内个别“预发布”文字是原构建标识；当前发布状态以本页为准。

## 下载与安装

下载 **AC8-Integrated-v2.3.6-share.zip**，完整解压到游戏目录之外，双击 **Start-GUI.cmd**。核对自动定位的 Steam 和游戏位置，选择安装范围并保存，再把生成的启动选项粘贴到 Steam。游戏操纵类型选择 Expert／专家，使用第三人称视角。请保留启动控制台，正常退出后等待清理。

## 主要更新

- 三种范围均包含飞控、摄像机与 HMD：仅比例引导、完整导弹强化、仅飞控。
- 仅比例引导不改动原版性能、锁定、伤害、装填、近炸与模型；完整强化以接近《战争雷霆》表现为目标大幅调整性能，不是其仿真的完全复刻。
- F2 开关 HMD，每次启动默认关闭；四向刻度与原有通知显示开关状态。HMD 不绕过武器锁定条件。
- F3 两种机位与 C 自由观察，包含任务／过场后的回中处理。
- 包含任务退出的生命周期修复，并保留检查点外观恢复。
- GUI 自动定位、多候选、手动路径、备份清理和诊断；主界面无滚动条。
- 首页增加当前 GUI 图片以及作者实际使用的飞行／视角键位参考，安装器不会改写键位。

## 验证与限制

已完成原生、Lua、PowerShell、GUI 及完整包检查，用户已批准公开发布。本版已转为正式发布；不同设备、任务、机型、HUD 驱动与退出任务／回机库／检查点重生的组合未获全面覆盖。不能据模拟测试保证所有 UE4SS 原生崩溃均已消除。

升级请使用新解压目录，并更新 Steam 启动选项。旧包不会自动更新。分享原始 ZIP，不要转发使用后的 sessions 或存档备份。

- 项目说明：https://github.com/CreeperUX/AC8-Integrated-Mod
- 安装：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/INSTALL.md
- 飞行与视角键位：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/KEYBINDINGS.md
- 已知限制：https://github.com/CreeperUX/AC8-Integrated-Mod/blob/main/docs/LIMITATIONS.md

基于 FletcherMiya/AC8-Mouse-Aim 发展，感谢原作者及贡献者；完整署名和许可随包保留。
