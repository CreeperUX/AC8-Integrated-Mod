# 安装版（2.5 起）

[返回说明首页](../README.md) · [安装与三种范围](INSTALL.md) · [其他 UE4SS Mod](EXTRA-MODS.md) · [已知限制](LIMITATIONS.md)

2.5 起提供 Windows 安装程序。安装版与便携版的游戏内功能完全相同，另外可以：
- 从 GitHub 一键更新，每天自动检查新版本；
- 每次启动前自动核对程序文件，损坏时自动恢复；“检查并修复”处理更多问题；
- 设置和记录集中保存，Steam 启动选项以后不用再改；
- 回退到上一版本，在 Windows“应用”中卸载。

## 安装

1. 从 GitHub 发布页下载 **AC8-Integrated-Setup-v2.5.3.exe**。安装程序没有代码签名证书，Windows 可能提示“Windows 已保护你的电脑”，点击“更多信息”→“仍要运行”。
2. 退出游戏后运行。程序安装到当前用户的 `%LOCALAPPDATA%\Programs\AC8 Integrated`，不需要管理员权限，不改动游戏目录和 Steam 设置。
3. 正在使用便携版时，安装程序会根据 Steam 启动选项找到它，询问是否导入设置、键位、安装范围、游戏和 Steam 路径以及附加 Mod。便携版本身不会被改动。
4. 把安装程序显示的一行（已复制到剪贴板）粘贴到 Steam → AC8 → 属性 → 通用 → 启动选项，替换原有内容。形式为：
   `"<安装目录>\Start-AC8-From-Steam.cmd" %command%`
   安装程序显示的是完整路径。这一行以后更新都不用再改。
5. 从开始菜单打开 **AC8 Integrated**（独立应用，不会出现命令行窗口；任务栏显示 AC8 图标，可以固定到任务栏）。没有导入便携版时，点击“自动定位”，再点击“保存设置”。

之后照常从 Steam 启动游戏。

## 更新

- **检查更新**（启动器右上角）：发现新版本后确认，启动器会关闭，控制台中下载、校验并安装，完成后自动重新打开启动器。
- **每天自动检查更新**（维护与恢复，默认开启）：打开启动器时最多每天检查一次，有新版本时版本标签显示“可更新到 …”。
- 只从本项目的 GitHub 正式版下载。安装前核对更新清单的数字签名（RSA-3072）和安装包的 SHA-256，并逐个核对包内文件；只接受比当前更新的版本。游戏运行中或上一次会话没有清理完时不会更新。
- 设置、键位、附加 Mod、飞行记录和存档备份都会保留。程序保留当前和上一个版本。
- 只读取 GitHub 发布信息和下载安装包，不上传任何数据。

## 检查并修复

启动器“维护与恢复”→“检查并修复”（安装版）：

| 检查 | 处理 |
|---|---|
| 程序文件是否完整 | 从本地已校验的安装包恢复；本地没有时从 GitHub 重新下载同一版本 |
| 设置、键位、安装范围能否读取 | 备份原文件，恢复默认，并保留仍然有效的设置 |
| 上一次游戏会话是否清理完 | 自动完成清理，游戏目录恢复原版 |
| 游戏目录是否有加载器残留 | 提示用“备份并清理”处理 |
| Steam 启动选项是否指向安装版 | 显示正确的一行并复制到剪贴板 |
| 开始菜单和卸载项 | 缺失时恢复 |

检查结束后可以输入 R 回退到上一版本。设置会保留；新版本写入、旧版本不认识的设置会先备份再整理。

每次从 Steam 启动时也会先核对程序文件，发现损坏直接从本地已校验的安装包恢复。启动时如果安全软件隔离了刚部署的 DLL，控制台会在启动游戏前说明；出现 `[NOT READY]` 时，下一行会给出原因（UE4SS 没有启动、导弹模块初始化失败、仍在加载等）。

## 数据位置

安装目录下的 `data` 文件夹（启动器的“诊断文件夹”“附加 Mod”按钮会打开对应位置）：

- `MouseAim-Settings.ini`、`Keybindings.ini`、`features.ini`：设置；
- `ue4ss-mods`：附加 UE4SS Mod（在启动器“附加 Mod”中管理），`ue4ss-mods-removed`：删除或替换下来的 Mod；
- `sessions`：每次运行的日志、飞行记录和存档备份（含个人数据，不要直接分享）；
- `cleanup-backups`、`diagnostics`、`repair-backups`：清理、错误诊断和修复前的备份；
- `update-cache`：已校验的安装包，用于离线修复。

## 停用 Mod 与卸载

- **原版启动**：启动器“停用 Mod／恢复原版启动”。停用后 Steam 启动选项不用删除，入口会直接启动原版游戏；“恢复离线 Mod”即可恢复。
- **卸载**：Windows 设置 → 应用 → 已安装的应用 → AC8 Integrated → 卸载。会先确认游戏目录已恢复原版；`data` 默认保留，也可以选择一并删除。最后删除 Steam 启动选项。

## 便携版

share.zip 便携版继续提供，设置保存在包内。双击包里的 **AC8 Integrated.exe** 打开启动器（`Start-GUI.cmd` 仍然可用）。便携版也可以在启动器中“检查更新”，新版本需要手动下载。

## English summary

From 2.5 a Windows setup program installs AC8 Integrated per user in `%LOCALAPPDATA%\Programs\AC8 Integrated` (no administrator rights; the game folder and Steam settings are not changed). It can import the settings of the portable package your Steam launch option points to. Paste the shown line into Steam's launch options once; it stays valid across updates. The launcher (Start menu: AC8 Integrated) checks GitHub for updates (manually, or at most once a day), verifies the RSA-3072 signed update manifest, the package SHA-256 and every file, installs only newer stable releases and keeps the previous version for rollback. Before every launch the program files are checked and restored from the cached verified package; "检查并修复" (check and repair) also rebuilds unreadable settings with a backup, finishes an unfinished session, and reports loader leftovers and the Steam launch option. Player data lives in `data`. Uninstall from Windows Settings > Apps, then remove the Steam launch option. The portable share.zip remains available. The setup program is not code-signed, so SmartScreen may ask for confirmation.
