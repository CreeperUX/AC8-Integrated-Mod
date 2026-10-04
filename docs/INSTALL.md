# 安装、模块选择与退出

## 选择仅鼠标飞控或完整安装

**此功能从 v2.3.0 整合包开始提供。v2.0.0 等旧包没有 `Choose-Features.cmd`，需要先换用新版。**请从 [v2.3.2 Release](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.2) 下载 `AC8-Integrated-v2.3.2-share.zip`。源码 ZIP 中的 `package-template` 是构建模板，缺少运行 DLL，不能直接当作安装包启动。

### 首次安装

1. 完整解压 v2.3.2 安装包到游戏目录之外。
2. 双击包根目录的 **`Setup.cmd`**，输入游戏目录。
3. 出现以下功能菜单时，输入 **`2`** 选择仅鼠标飞控，或输入 **`1`** 选择完整安装，再按回车：

```text
1. Mouse flight + customized missile enhancements / 鼠标飞控 + 导弹强化
2. Mouse flight only; original game missiles / 仅鼠标飞控，保留原版导弹
Choose 1 or 2 (Enter keeps 1):
```

4. 按 Setup 后续提示设置 Steam 启动选项，再启动游戏。

直接回车保留原选择，括号内数字以实际提示为准；全新包初始为 `1`，即完整安装。

### 已经安装后更改

1. 正常退出游戏，等待旧启动控制台完成清理。
2. 双击**同一安装包根目录**的 **`Choose-Features.cmd`**。
3. 输入 `2` 改为仅鼠标飞控，或输入 `1` 改为完整安装，按回车保存。
4. 通过原 Steam 入口或该包的 `Start.cmd` 再次启动。不需要重新设置 Steam 启动选项。

也可以关闭游戏后手动编辑同目录 `features.ini`：

| 目标 | 配置内容 |
|---|---|
| 仅鼠标飞控 | `missile_enhancement=0` |
| 完整安装 | `missile_enhancement=1` |

注意：**菜单输入 `2`，配置保存为 `0`**。配置只接受 `0/1`。不能在飞行中热卸载导弹模块；F4/F8不是这个安装开关。

## 第一次使用

从 [v2.3.2 Release](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.2) 下载 `AC8-Integrated-v2.3.2-share.zip`，解压到例如 `D:\Mods\AC8-Integrated`。不要直接从 ZIP 运行，也不要放进游戏目录。

运行 `Setup.cmd`。它会验证游戏 EXE、定位 Steam、询问安装内容，并在包内生成路径文件和 `Steam-Launch-Option.txt`。Setup 不修改游戏文件、Steam 设置或存档。

将生成的一整行粘贴到 Steam → 游戏属性 → 通用 → 启动选项。形式如下，使用你自己生成的实际路径：

```text
"D:\Mods\AC8-Integrated\Start-AC8-From-Steam.cmd" %command%
```

日常从 Steam 或 `Start.cmd` 启动，不直接运行游戏 EXE。游戏操纵类型选择 Expert。若移动 Mod 目录，重新运行 Setup 并更新 Steam 启动选项。

## 导弹模块开关

Setup 提供两个选择：

1. 鼠标飞控＋导弹强化。
2. 仅鼠标飞控，使用原版导弹。

默认保留包内现有选择（初始为完整功能）。以后可在**游戏关闭并完成清理后**运行 `Choose-Features.cmd`；也可编辑 `features.ini`：

```ini
missile_enhancement=0
```

`0` 不安装 `AC8SourceInit`，包括其性能、近炸、外观及检查点维护功能；`1` 安装整个导弹模块。当前没有将近炸、伤害和外观分别拆成开关，也没有“仅导弹”安装选项。飞行中不能热卸载已应用的导弹参数，F4/F8/F10 都不切换此安装选项。

启动控制台会显示 `mouse flight only` 或 `mouse flight + missile enhancement/cosmetics`。关闭模块时以鼠标模块加载日志判断就绪，不再等待 375 项导弹字段初始化。

## 飞控设置

关闭游戏后编辑 `MouseAim-Settings.ini`，下次启动应用：

| 参数 | 默认 | 含义 |
|---|---:|---|
| `control_mode` | 1 | 0=CLASSIC 2.0，1=AGILE 2.1；F4 可在飞行中临时切换 |
| `sensitivity` | 0.10 | 鼠标方向灵敏度 |
| `model_assist` | 1 | 0 回到旧基础控制器，1 启用三轴模型控制 |
| `model_assist_strength` | 0.20 | 已验证学习参数的混合尺度，不是操纵增益旋钮；0 仍保留标准模型控制 |
| `hud_fps` | 120 | GUI 调度目标，不保证实际显示帧率 |
| `follow_rate` / `level_rate` | 8 / 3 | 镜头跟随与地平线回正 |

F10 重载的是游戏目录中**本次运行副本**，并会恢复该副本的 `control_mode` 配置值。根目录设置通常在下次启动才应用。GDI 备用 GUI 不显示档位文字，切换仍记录在控制台/日志。

## 退出、更新与云同步

正常退出后保留控制台，等待日志/存档备份归档和临时 Mod 文件清理，然后等待 Steam 云同步。云冲突不会被自动消除；根据实际最新进度选择同步方向。不要为了启动 Mod 关闭 Steam 云。

升级时先退出旧版本并完成清理，将新包解压到新目录，重新 Setup 和更新启动选项。不要同时运行两个整合包。

需要恢复原版时，退出并清理，然后移除本包的 Steam 启动选项。F8 只关鼠标辅助，不能代替卸载全部 Mod。

`Existing loader conflict` 表示已有加载器：先核实来源。属于旧包时使用那个旧包的 `Cleanup-Offline.cmd`，不要直接删除来源不明的 `ue4ss` 目录。

## 可选分析

普通游玩不需要 Python。自动离线报告需要 Python 和 NumPy。缺少分析依赖时不会阻止游戏清理。`sessions` 中含日志、试飞数据和个人存档备份，**不要直接转发**。分享干净 Release ZIP 即可。

