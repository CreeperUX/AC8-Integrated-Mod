# v2.3.15-beta.1 — 原生圆环 HUD 与游戏内调整（Beta）

**这是供自愿尝试的 Beta 预发布版本，尚未完成广泛实机验证。v2.3.6 继续作为正式推荐版本；如果遇到抖动、掉帧、显示异常或兼容性问题，请回退到 [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6)。**

**仅限离线单人，禁止带 Mod 进入线上或多人模式。**

## 相比 v2.3.6 的主要变化

- **圆环改为游戏内原生 UMG 绘制**：青色描边圆环、头盔瞄准刻度和指向机头的方向刻度由游戏 HUD 绘制；不新增第二个机头十字。外部窗口不再负责高速移动的圆环。
- **更容易辨认的圆环**：相比上一轮原生 HUD 候选，可见直径增大 50%，连接刻度的间距同步调整。这个 50% 是相对上一轮候选，并非相对 v2.3.6。
- **针对显示抖动的修正**：投影使用已发布快照中的鼠标目标与相机，圆环和方向刻度复用同一份快照；保留所属画布及最后有效尺寸，减少临时布局变化。实际改善程度仍待不同机器实测。
- **F1 诊断／调整面板与切换通知**：保留 CreeperUX 样式，由独立线程按需绘制。仅面板时目标 20 Hz，通知期间目标 60 Hz；没有面板或通知时不提交外部界面帧。面板刷新率不会直接限制原生圆环，但共享 GPU、合成资源和少量数据同步，不能保证零性能影响。
- **游戏内灵敏度与自由观察放大调整**：Ctrl+PageUp/PageDown/Home 调灵敏度，Alt+PageUp/PageDown/Home 调 C+右键放大增强系数。只对本次运行生效；永久设置请在退出后修改 MouseAim-Settings.ini。
- **自由观察放大过渡修正**：增强游戏原生的缩放变化，沿用其长按门槛和进入／退出过渡，避免按下时先瞬间额外放大。原生长按切换目标键的注视逻辑仍交由游戏处理。
- **快捷停用／恢复入口**：退出并等待清理后，可用 Disable-Mod.cmd 停用、Enable-Mod.cmd 恢复离线 Mod。停用会检查本项目已知残留；F8 只关飞控，不等于卸载，停用检查也不代表对其他 Mod 或反作弊状态作出保证。

保留三轴模型飞控、F4 两档策略、F2 头盔瞄准、F3 两档机位，以及三种自由选择的安装范围：仅飞控／飞控+仅比例引导／飞控+完整导弹强化。本次不宣称重写控制律或已证明飞行性能超过 v2.3.6。

## 下载与安装

下载 **AC8-Integrated-v2.3.15-beta.1-share.zip**（完整运行包；GitHub 自动生成的 Source code 不是安装包），完整解压到**游戏目录之外的新目录**，不要覆盖旧版。

1. 正常退出旧版本，保留控制台直到归档、清理完成。
2. 双击新包的 Start-GUI.cmd，核对游戏／Steam 路径，选择安装范围并保存；也可使用 Setup.cmd。
3. 把新包生成的 Steam-Launch-Option.txt 整行替换到 Steam → 游戏属性 → 启动选项。
4. 从 Steam 启动；游戏操纵类型选择 Expert／专家，使用第三人称视角。

适配 Windows x64、Steam 游戏 Build 25201480。包内保留已有游戏版本与载荷校验。本 Beta 提供中文安装包；v2.3.6 的独立英语包不是本 Beta。

## 出现问题如何回退到 v2.3.6

1. 正常退出 Beta 并等待其控制台清理完成。异常退出时使用 **Beta 原目录内的 Cleanup-Offline.cmd**；无法清理时保留原包并按提示排查，不要直接覆盖未知加载器。
2. 从 [v2.3.6 正式发布页](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6) 下载 AC8-Integrated-v2.3.6-share.zip（或 English 包），解压到另一个新目录。
3. 在 2.3.6 中运行 Start-GUI.cmd／Setup.cmd，重新选择需要的安装范围，并将 Steam 启动选项替换为 **2.3.6 生成的那一行**，然后再启动。

仅重新解压或关闭 F8 不会切换版本。无需回滚游戏存档；不要把 Beta 的 payload、DLL、sessions 或整份配置覆盖到旧包。先完成清理，再删除不再需要的 Beta 目录，并妥善保留个人备份。

## 验证状态与已知边界

原生圆环已在本机前一轮验证中显示并可使用。本次放大和快照一致性修正通过编译、原生／Lua 回归与包检查，但尚未获得广泛实机验收；不能宣称已消除所有抖动或降低了所有机器的帧时间。

原生 UMG 不等于原生 120 Hz 输入或位置更新。本版原生圆环不使用此前候选的外部插值／预测补帧，Ctrl+End 不为原生圆环开启补帧。更新仍取决于游戏回调、相机数据和实际 HUD 渲染。若主要诉求是已发布版本的稳定体验，请继续使用 v2.3.6。

反馈请注明游戏版本、机型／任务、安装范围、F1 开关、F2/F3/F4 状态、显示模式及复现步骤。请分享原始 ZIP；使用过的 sessions 可能包含个人存档备份，不要公开上传整份目录。

## 鸣谢

本项目基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展，感谢原作者及贡献者提供的基础工作。完整第三方署名和许可保留在 THIRD_PARTY_NOTICES.md 与 licenses 中。

## English summary

**Optional Beta / GitHub prerelease. v2.3.6 remains the recommended stable release. Offline single-player only; do not use online or in multiplayer.**

This Beta moves the moving reticle to native UMG, with cyan ring, HMD marks and nose-direction ticks. It retains an on-demand external F1 panel and notifications, adds in-session sensitivity/zoom controls, follows native free-look zoom transitions, and provides verified project cleanup / disable / enable entry points. The ring is 50% larger than the previous native-HUD candidate, with shared projection snapshots and more stable canvas layout.

Native rendering does not guarantee 120 Hz updates or zero GPU/compositor overhead. The latest jitter changes need broader in-game validation; no general performance improvement or full control-law redesign is claimed. The default native ring does not use the earlier external interpolation/prediction experiments.

Extract the complete share ZIP outside the game into a new folder, run Start-GUI.cmd or Setup.cmd, and replace the Steam launch option with the newly generated line. This Beta ships the Chinese installer. To roll back, exit and finish Beta cleanup (use its Cleanup-Offline.cmd after an abnormal exit), install the already released [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6) in a separate directory, and replace the Steam launch option with the v2.3.6 line. Do not overlay packages or copy DLLs/settings/sessions between versions; no save rollback is needed.

Based on FletcherMiya/AC8-Mouse-Aim, with thanks to the upstream author and contributors. Licenses and acknowledgments are preserved.
