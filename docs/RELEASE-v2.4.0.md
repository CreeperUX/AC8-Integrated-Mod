# v2.4.0 — WAR 精确飞控、参考准星与过场恢复修复

> v2.4.0 的发布已合并到 [v2.4.1](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.4.1)，不再单独提供下载；请下载 **AC8-Integrated-v2.4.1-share.zip**，安装步骤相同。v2.4.1 只在此基础上新增参考准星开关并修复键位设置对话框，见 [v2.4.1 说明](RELEASE-v2.4.1.md)。
>
> The v2.4.0 release was merged into [v2.4.1](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.4.1); download AC8-Integrated-v2.4.1-share.zip instead (same installation steps).

**正式版。仅限离线单人，禁止带 Mod 进入线上或多人模式。**

## 相比 v2.3.15-beta.1 的主要变化

- **全新 WAR 飞控**：
  - 控制器基于从游戏中还原的 AC8 转动规律，每帧读取飞机真实转动状态，按最短时间加速、到位前及时刹住；
  - 决策结构参考 War Thunder 鼠标教官：小偏差直接修正，大偏差先滚转再拉杆；
  - 路径锁定让机头沿接近直线的路径走向圆环，减少"L 形回归"；
  - 鼠标停下、机头到位后平滑改平，没有拖尾、没有超调；用键盘滚转后松开，也会自动改平；
  - 机头附近的滚转决策预判光标运动，来回拖动时反向更快；光标远离机头时不再左右来回滚转；
  - 按住俯仰、偏航或滚转键按 War Thunder 规则接管；
  - 数据异常时自动降级到 WAR v11.1。

  详见 [WAR 飞控说明](WAR-FLIGHT-CONTROL.md)。
- **F4 改为 PEACE ↔ WAR**：PEACE 就是原来的 CLASSIC 2.0，控制律未改动；AGILE 2.1 退役。
- **机头参考准星**：游戏原生准星会下沉，与机炮实际弹道对不上。2.4.0 在机头（机炮）方向 500 m 处画一个中心留空的十字（War Thunder 风格），与圆环同色同线宽。飞控到位时十字落在圆环中心；按住 C 自由观察时也显示。详见 [原生 HUD 与参考准星](NATIVE-UI-STYLE.md)。
- **头瞄（F2 HMD）**：
  - 圆环改为圆环加四角括号，避免与十字混淆；
  - 按住 C 自由观察时，切换目标改为按视线方向选，括号圆环画在视线点上；松开 C 后恢复按鼠标目标选。

  游戏的锁定角、射程和锁定时间判定不变，详见 [头盔瞄准](HELMET-SELECTION.md)。
- **逐帧同步输入**（来自 2.3.16 预览版）：鼠标目标由游戏帧统一消费，控制步长使用高精度计时，圆环以渲染平移移动。
- **自定义按键**（来自 2.3.17 预览版）：启动器"维护与恢复 → 键位设置"可配置 8 个动作：
  - 自由观察、观察放大；
  - 俯仰、滚转、偏航各两个方向的接管检测键。

  只支持单键，不支持组合键和 F1–F10，下次启动生效。详见 [自定义键位](CUSTOM-KEYBINDINGS.md)。
- **修复：过场后模组相机／HUD 不恢复**：进入过场或在 C 键环视中切入过场时，某一帧相机数据（FOV）校验失败曾导致模组相机与 HUD 在整局内停用。现在改为每 500 ms 受保护重试，过场结束后自动恢复，并已在试飞中实际验证。详见 [修复说明](CAMERA-FAULT-RECOVERY.md)。

从 v2.3.6 升级的用户，还会获得 v2.3.15-beta.1 的全部改进：原生 UMG 圆环、F1 面板与通知、游戏内灵敏度与放大调整、停用／恢复入口。详见 [v2.3.15-beta.1 说明](RELEASE-v2.3.15-beta.1.md)。

## 升级注意

- **默认飞控是 PEACE（CLASSIC 2.0）。** v2.3.15-beta.1 默认是 AGILE 2.1，所以按默认设置使用的用户，升级后的手感会更接近 CLASSIC；按 F4 可切换到 WAR。
- **旧设置自动迁移：** `control_mode=1`（AGILE）→ 0（PEACE），`2/4/5` → 3（WAR）。
- **键位：** 接管检测键默认仍是 W/S、A/D、Q/E。如果游戏里改过操纵绑定，请在"键位设置"中改成一致。

## 下载与安装

下载 **AC8-Integrated-v2.4.0-share.zip**。这是完整运行包；GitHub 自动生成的 Source code 不是安装包。完整解压到**游戏目录之外的新目录**，不要覆盖旧版。

1. 正常退出旧版本，保留控制台直到归档、清理完成。
2. 双击新包的 Start-GUI.cmd，核对游戏／Steam 路径，选择安装范围并保存；也可使用 Setup.cmd。
3. 把新包生成的 Steam-Launch-Option.txt 整行替换到 Steam → 游戏属性 → 启动选项。
4. 从 Steam 启动；游戏操纵类型选择 Expert／专家，使用第三人称视角。

适配 Windows x64、Steam 游戏 Build 25201480。包内保留游戏版本与载荷校验。安装包为中文界面。

## 回退到旧版本

1. 正常退出 2.4.0 并等待控制台清理完成。异常退出时，使用 **2.4.0 原目录内的 Cleanup-Offline.cmd**。
2. 下载需要的旧版：[v2.3.15-beta.1](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.15-beta.1) 或 [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6)，解压到另一个新目录。
3. 在旧版目录中运行 Start-GUI.cmd／Setup.cmd，并把 Steam 启动选项替换为**旧版生成的那一行**。

不要在版本之间复制 payload、DLL、sessions 或整份配置；无需回滚游戏存档。

## 验证状态与已知边界

- **测试：**
  - 原生测试全部通过（公开源码自带 23 项及 GPU HUD 测试），HUD／头瞄脚本另有离线测试；
  - WAR 已在 JAS39E、EA-36、苏-35、苏-57、F-22、F/A-18F、台风等机型上试飞；
  - 用全部试飞记录中的真实鼠标输入回放验证（549 个片段）；
  - 人在回路仿真覆盖跟踪、剪刀机动、快速指向、精确瞄准等场景。
- **WAR 回放结果**（与开发中的早期 WAR 相比，同一组 549 个片段）：
  - 带坡度小角度接近的弧线比例从 9.5% 降到 6.1%；
  - 光标移动中机头方向明显偏离（>30°）的路程比例从 14.1% 降到 12.5%；
  - 停鼠标到机翼水平的中位时间约 1.2 s。
- **未覆盖：** 不保证所有机型、任务、电脑和键位配置都已验收。
- **WAR 仍存在的现象：**
  - 偶尔仍会出现弧线；鼠标快速大幅甩动时，机头方向与光标方向可能有偏差；
  - 快速拉起时鼠标突然停住，机头可能越过光标几度再回来。控制器在停住前就已经反向打满，越过量来自游戏杆量爬升本身的速度（约 0.6 s）；
  - 改平途中再动鼠标会停在当前坡度；
  - 机头离光标 2.5° 以内才开始改平，大机动后会先对准再改平。
- **参考准星：** 十字表示机头和机炮轴线在 500 m 处的方向，不含提前量，不是射击提前量指示。
- **WAR 不改变飞机性能：** 失速、能量和过载仍由游戏决定。
- **其他：** 原生圆环仍不保证 120 Hz 更新，F1／通知也不保证零性能开销（同 v2.3.15-beta.1）。

反馈请注明游戏版本、机型／任务、安装范围、F2/F3/F4 状态（PEACE 或 WAR）以及复现步骤和大致时间点。请分享原始 ZIP；使用过的 sessions 可能包含个人存档备份，不要公开上传整份目录。

## 鸣谢

本项目基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展，感谢原作者及贡献者提供的基础工作。完整第三方署名和许可保留在 THIRD_PARTY_NOTICES.md 与 licenses 中。

## English summary

**Stable release. Offline single-player only; do not use online or in multiplayer.**

- **New WAR flight control.** It is built on a reconstructed model of AC8's own rotation law: it reads the aircraft's real rotation state every frame, accelerates time-optimally and brakes on time. Decisions follow a War Thunder-style mouse-instructor structure:
  - path lock gives straight nose paths (fewer L-shaped returns);
  - smooth, bank-scaled automatic levelling once the cursor rests near the nose, also after a keyboard roll;
  - cursor-motion-aware roll decisions near the nose for quicker reversals, without roll wobble when the cursor is far off-axis;
  - War Thunder-style keyboard takeover;
  - automatic fallback to WAR v11.1 if its data becomes invalid.

  See [WAR flight control](WAR-FLIGHT-CONTROL.md) (Chinese).
- **Modes.** F4 toggles PEACE (the unchanged CLASSIC 2.0, default) and WAR. AGILE 2.1 is retired; `control_mode` 1 migrates to PEACE and 2/4/5 to WAR. Users of the v2.3.15-beta.1 default (AGILE) will now start in PEACE.
- **Reference gun cross.** The native reticle sinks and does not match the gun line. A War Thunder-style cross with a centre gap now marks the nose/gun axis at 500 m; it sits in the ring once the nose has arrived and stays visible in C free look. It is not a lead indicator.
- **HMD.** The HMD ring now has four corner brackets. While holding C, target switching picks along the view direction, marked by the bracketed ring. Lock angle, range and lock time are unchanged.
- **From the 2.3.16 and 2.3.17 previews.** Frame-synchronous input. Configurable keys for free look, zoom and six manual-takeover directions (launcher → maintenance → key settings; single keys only).
- **Fix.** A rejected native-camera frame on a cutscene transition (an invalid FOV) no longer disables the mod camera and HUD for the rest of the session. The camera now retries every 500 ms under all existing guards; this was confirmed in a live flight.

Extract the complete share ZIP outside the game into a new folder, run Start-GUI.cmd or Setup.cmd, and replace the Steam launch option with the newly generated line. The installer UI is Chinese. To roll back, finish 2.4.0 cleanup, install v2.3.15-beta.1 or v2.3.6 in a separate directory and use its launch option. Do not copy DLLs, settings or sessions between versions.

Validation:
- All native test suites pass (23 ship with the public source, plus a GPU HUD test), as do offline tests of the HUD/HMD scripts.
- WAR has been flown on JAS39E, EA-36, Su-35, Su-57, F-22, F/A-18F and Typhoon.
- It was also checked by replaying recorded real-mouse input from all test flights (549 windows) and by a human-in-the-loop simulation.
- Not every aircraft, mission, PC or binding is covered.

Known WAR behaviour:
- occasional arcs on banked small approaches;
- a few degrees of overshoot if the mouse stops abruptly during a fast pull (limited by the game's stick ramp);
- levelling holds the current bank if the mouse moves again mid-way.

Based on FletcherMiya/AC8-Mouse-Aim, with thanks to the upstream author and contributors. Licenses and acknowledgments are preserved.
