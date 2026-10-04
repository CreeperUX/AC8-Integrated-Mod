# AC8 Integrated Mod

> **图形界面本地预览：** 完整解压后双击 `Start-GUI.cmd`，通过按钮选择游戏目录、保存设置、复制启动选项、启动与清理。此预览未发布；[使用说明](docs/GUI-PREVIEW.md)。

> **v2.3.3 已撤回为草稿，功能仍在完善，暂不提供公开安装包。** 当前公开整合包为 v2.3.2。[待发布功能说明](docs/INSTALL-PREVIEW.md)

> **旧版清理后仍报加载器冲突？** 已发布独立 [历史残留清理工具 v1.0.0](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/cleanup-v1.0.0)，用于旧包会话记录丢失、只剩 DLL 或清理中断等问题。先备份校验再处理可确认归属的残留；[使用步骤与限制](docs/CLEANUP.md)。旧版整合包不会自动更新，工具不改变飞控版本。

为 **ACE COMBAT 8 Steam 版**提供鼠标飞控、在线响应模型学习和可选的导弹强化模块。

> **如何选择安装？** 使用 **v2.3.2 整合安装包**：运行 `Setup.cmd`，在功能菜单输入 **`2` 并回车＝仅鼠标飞控**，输入 **`1` 并回车＝完整安装（飞控＋导弹强化）**。已经安装过的用户，退出游戏并等待清理完成后运行 `Choose-Features.cmd`，同样输入 `1` 或 `2`，下次启动生效。[详细步骤](docs/INSTALL.md#选择仅鼠标飞控或完整安装)
>
> **版本提示：** v2.0.0 等旧安装包没有这个菜单。请在 [v2.3.2 Release](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.2) 下载 `AC8-Integrated-v2.3.2-share.zip`。GitHub 的 Code → Download ZIP 是源码，安装脚本位于 `package-template/`，源码本身不含运行 DLL。

## 项目基础与致谢

**本项目基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展而来。**原项目为 AC8 鼠标飞控的实现提供了重要基础；在此基础上，本项目进一步加入在线模型学习、双档控制切换、GUI 改进及可选导弹模块等功能。

**特别感谢原作者 FletcherMiya 及原项目贡献者的开发与分享。**本仓库保留上游署名和许可说明；这是独立维护的社区衍生项目，不代表原作者对本项目改动的背书。其他参考项目与导弹模块的来源见 [第三方署名与许可](THIRD_PARTY_NOTICES.md)。

鼠标指定世界方向，飞机自动追随；按住 C 自由观察，F4 在稳健与积极两档飞控间切换。**不需要导弹改动时，可以只安装鼠标飞控。**

当前版本：**v2.3.2 预发布**。本版修复 Setup 安装脚本的重复 UTF-8 编码标记，并修复任务 15 特殊 A-6E 的模型数据入口；沿用 v2.3.1 控制律。[更新说明](docs/V232-OBSERVATION.md)。适配游戏 Build **25201480**、Windows x64。启动器校验游戏文件和固定 UE4SS 运行库；其他游戏构建尚未适配。仅用于离线单人任务，Steam 客户端可保持在线进行云同步。

[安装包发布页](https://github.com/CreeperUX/AC8-Integrated-Mod/releases) · [完整安装说明](docs/INSTALL.md) · [控制原理](docs/CONTROL.md) · [开发与构建](docs/DEVELOPMENT.md) · [已知限制](docs/LIMITATIONS.md)

## 选择安装内容

在 `Setup.cmd` 或 `Choose-Features.cmd` 显示的菜单中，输入数字后按回车：

| 想安装什么 | 菜单输入 | 实际保存到 `features.ini` |
|---|---:|---|
| **仅鼠标飞控，保留原版导弹** | **`2`** | `missile_enhancement=0` |
| **完整安装：鼠标飞控＋导弹强化** | **`1`** | `missile_enhancement=1` |

直接回车会保留当前选择；全新安装包初始选择为完整安装。**菜单编号 `2` 对应配置值 `0`，不要把菜单编号直接写入配置。** F4 切换的是飞控策略，不是安装内容。

| 安装选项 | 包含内容 |
|---|---|
| **仅鼠标飞控** | 鼠标指定方向、三轴模型控制、在线学习、F4 双档切换、C 自由观察、GPU 圆环与方向刻度。使用游戏原版导弹 |
| **鼠标飞控＋导弹强化** | 上述功能，以及玩家导弹性能调整、近炸参数、普通 MSL 外观替换、检查点重生后的外观恢复 |

`Setup.cmd` 会询问是否启用导弹模块。之后也可在退出游戏并完成清理后运行 **`Choose-Features.cmd`** 更改，下次启动生效。关闭导弹模块时，启动器**不会把 `AC8SourceInit` 模块复制到游戏目录**，而不只是隐藏效果。该选择不影响 F4 的两档飞控。

## 快速开始

1. 从 [v2.3.2 Release](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.2) 下载整合安装包，完整解压到**游戏目录之外**。GitHub 的 Source code ZIP 是源码，不是可运行安装包。
2. 正常退出游戏；使用过旧包时，先等待旧控制台完成清理。
3. 运行 `Setup.cmd`，输入 Steam「管理 → 浏览本地文件」打开的游戏根目录；功能菜单输入 **`2` 仅鼠标飞控**，或 **`1` 完整安装**，按回车确认。
4. 将生成的 `Steam-Launch-Option.txt` 整行复制到 Steam 的游戏启动选项。
5. 从 Steam 或 `Start.cmd` 启动，游戏操纵类型选择 **Expert／专家**。
6. 保留启动控制台。退出后等待归档、清理和 Steam 云同步完成。

## 操作

| 按键 | 功能 |
|---|---|
| 鼠标 | 指定希望机头追随的世界方向 |
| **F4** | `CLASSIC 2.0` ↔ `AGILE 2.1`；默认积极档 |
| **F8** | 开关整个鼠标飞控，不卸载导弹模块 |
| F9 | 将目标方向回中 |
| 按住 C | 自由观察，保持原飞行目标方向 |
| W/S、A/D、Q/E | 手动操纵优先；W/S 同时让出自动滚转 |
| F7 | 开关 Mod 圆环 GUI |
| F10 | 重读本次运行副本配置并回中 |

GPU GUI 会短暂显示切换档位；过渡约 0.35 秒，已经学到的模型不会因 F4 切换清空。游戏原有键位如与 F4/C 冲突，需要自行改绑。

![独立渲染测试中的档位提示，非游戏截图](docs/assets/mode-switch-preview.png)

## 飞控特点

- 俯仰、滚转、偏航从标准模型起步，持续使用实际操纵和姿态反馈。
- 每架飞机独立学习增益、响应时间和延迟；候选通过后续验证后，平滑用于控制。
- **CLASSIC** 保留 v2.0 的限幅和阻尼；**AGILE** 在有足够制动余量时提高操纵权限，靠近目标时收紧。
- 原始机动性能仍由游戏决定。模型是独立轴响应近似，**不等同于完整气动模型或 War Thunder 的完整飞控**。
- 学习状态只在当前游戏进程内保存，退出后重置。不同速度区间的验证覆盖还有限制。

## 可选导弹模块

- 普通 MSL 使用增强机动/制导/伤害配置，保留空地目标能力与常规装填。
- 4AAM/6AAM 锁定距离配置约 10 km，LAAM 约 20 km；实际效果仍受游戏机制影响。
- 玩家空空导弹写入近炸相关字段；所有弹种实际触发尚未全面验收。
- `msl_a0/a1` 按规则使用 QAAM 外观；F-14、F-4、A-6、EA-6B 排除。台风与 JAS39E 使用 SASM 外观；`f0/r0/j0` 保留原模型。
- 挂载模型被检查点重置时，有限范围的引用检查触发恢复，不反复写入整套性能字段。

LAAM 的 4 发待发是配置目标，全部机型的实际槽位尚未确认。启用碰撞的已发射导弹实体仍可能保留原外观。详见[已知限制](docs/LIMITATIONS.md)。

## 验证状态与参与开发

已有多机型实飞记录确认三轴控制和部分学习参数实际生效；F4 切换及可选安装流程通过本地回归检查，完整游戏交互仍作为预发布验收内容。模拟测试结果不代表每架飞机都会改善。

源码仓库不含游戏资源、个人存档、原始试飞日志或预编译 DLL。可运行包中的 UE4SS 运行库固定版本并附许可证；构建流程见 [DEVELOPMENT.md](docs/DEVELOPMENT.md)。

欢迎提交问题和改进。反馈时请写明游戏 Build、机型、安装选项、飞控档位及复现步骤；不要上传完整 `sessions` 文件夹，其中可能含个人存档。

本项目继承并修改了 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim)，并使用 MouseFlight、MinHook、RE-UE4SS 的相关代码。署名和许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## English overview

This project is developed from [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim). Special thanks to FletcherMiya and the upstream contributors for their work and for sharing the original project. This is an independently maintained community derivative; upstream attribution and license notices are preserved.

Mouse-directed flight control with per-aircraft online response identification, runtime Classic/Agile switching, free look and an **optional** missile enhancement module. Windows x64 / Steam build 25201480 / offline single-player only. Download a release package, run `Setup.cmd`, choose features, and paste the generated Steam launch option. F4 selects the control policy; F8 toggles mouse assistance. See the linked documentation for limitations and source builds.
