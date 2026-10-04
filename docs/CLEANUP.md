# 历史加载器残留修复工具

**用于处理旧版 AC8 Integrated 清理不完整导致的启动冲突。** 下载 [AC8-Cleanup-v1.0.0.zip](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/download/cleanup-v1.0.0/AC8-Cleanup-v1.0.0.zip)，或打开 [清理工具发布页](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/cleanup-v1.0.0)。这是独立维护工具，不包含飞控、导弹模块或游戏运行库，不需要重新安装整合包或 Python。

## 哪些历史问题可以处理

旧版 `Cleanup-Offline.cmd` 依赖那个解压包的 `active-session.json`。换包、删除旧包、关闭启动控制台、部署或清理中途失败，都可能导致记录丢失或只剩部分文件。

典型表现是启动时报 `Existing loader conflict: dwmapi.dll`、`ue4ss` 或 `steam_appid.txt`；旧清理命令提示 `No active candidate session`，但游戏目录仍有这些项目。旧清理命令也可能因为 `ue4ss` 已删除、归属标记不存在而无法清掉剩余 DLL。

新版清理核心支持部分文件已删除后的重复执行；无记录但仍有残留时会明确报告未清理；启动器在复制 DLL 前记录部署意图；存档检查和分析失败不再阻止清理完成状态归档。**已下载的旧 ZIP 不会自动更新**，处理既有残留请使用此独立工具。

## 操作步骤

1. 正常退出游戏，关闭旧启动控制台。清理工具会拒绝在 AC8 运行时工作。
2. 将清理工具 ZIP 完整解压到游戏目录之外，例如 `D:\Mods\AC8-Cleanup`。不要直接在 ZIP 中运行。
3. 双击 `Recover-Cleanup.cmd`。
4. 粘贴 Steam → 游戏 → 管理 → 浏览本地文件打开的**游戏根目录**。该目录下应有 `Game\Binaries\Win64\AceCombat8.exe`；不要输入 `Win64` 或整合包目录。
5. 核对显示的路径。只有确认残留来自 AC8 Integrated 时，输入大写 `RECOVER` 回车。如果安装过其他 UE4SS Mod 或不清楚来源，取消并先检查归属。
6. 等待 `CLEANUP COMPLETE: all three deployed items are absent`。备份和 `cleanup-report.json` 位于工具目录的 `cleanup-backups` 子目录中，路径会在控制台显示。
7. 再从原整合包的 Steam 入口或 `Start.cmd` 启动。以后仍需保留启动控制台，正常退出后等待清理完成。

此工具只处理所选游戏目录 `Game\Binaries\Win64` 下的 `dwmapi.dll`、`ue4ss` 和 `steam_appid.txt`。它不会修改 Windows 系统 DLL、游戏 EXE、存档、Steam 启动选项或删除旧整合包。需要彻底停止使用 Mod 时，还应手动清除 Steam 中指向旧包的启动选项。

## 校验和备份

移除之前，工具会备份所有待处理内容，并逐文件校验 SHA-256。备份失败或校验不一致时停止，不进入删除步骤。删除中途失败时保留备份和报告，重新执行可继续处理剩余文件。

无会话记录的历史恢复仅接受本项目固定 UE4SS 加载器哈希、内容为 `2288340` 的 AppID 文件，以及带有效 `AC8SourceInit-owner.txt` GUID 标记的 UE4SS 目录。目录包含本包之外的 Mod 时拒绝恢复。文件名相同不代表来源相同，因此仍需要用户确认。

`cleanup-backups` 可能包含运行日志和个人试飞数据，不要整目录公开上传。保留备份直到确认游戏可以正常启动。如需恢复，只能在游戏关闭、目标位置没有新部署文件时，将对应备份中的项目复制回原 `Win64` 目录；不要覆盖已经安装的新版本。

## 工具拒绝处理时

| 提示 | 含义与处理 |
|---|---|
| `Ownership marker missing/mismatched` | UE4SS 目录无法确认归属；保留原文件，核对旧包及其他 Mod，不自动强删 |
| `Unknown dwmapi.dll hash` | DLL 不匹配固定运行库；可能属于其他 Mod 或已被修改，需要人工检查 |
| `Other UE4SS mods found` | 同一 UE4SS 目录还有其他模块，不能当作本包残留整体卸载 |
| `Filesystem link refused` | 目标或父目录是链接/联接点，工具拒绝跨越路径边界 |
| `Contents changed` / 访问被拒绝 | 文件变化、被占用或权限不足；先退出游戏，保留备份与完整报错，确认原因后再重试 |

不要通过删除 Windows 系统 DLL、关闭安全软件或随意取得目录所有权来绕过报错。

## 只读检查

可在 PowerShell 中运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Recover-Cleanup.ps1 -GameRoot "D:\SteamLibrary\steamapps\common\ACE COMBAT 8" -RecoverHistorical -CheckOnly
```

`-CheckOnly` 不创建备份、不删除文件；仍会验证路径和归属，异常时返回错误。脚本接口的 `-RecoverHistorical` 表示调用者已经确认历史残留归属；日常推荐使用带确认提示的 CMD。

## 验证范围

通过 Windows PowerShell 5.1 的隔离目录回归测试，覆盖正常清理、重复执行、无记录残留、部分删除、归属错误、未知 DLL、其他 Mod、只读检查、路径边界、游戏运行、备份失败、删除中断、缺少存档及目录链接。测试不启动游戏；朋友机器的具体残留来源仍需通过实际运行报告确认。
