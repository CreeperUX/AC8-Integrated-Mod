# 任务退出对象查找修复（2026-10-05）

版本：2.3.6-hmd-preview.4。飞控原生 DLL 与 preview.3 相同，本次修改 Lua 生命周期管理。

## 证据范围

用户提供的崩溃摘要指向 UE4SS FindObject / ForEachUObject / GetOutermost。没有取得原始 minidump，不能独立复核地址映射或证明唯一触发者。代码核对确认旧版 mounted-watch 及延迟回调会按旧路径调用全局查找。

## 改动

- 外观巡检使用初始化取得的进程级 GameEngine，从当前 viewport/world/LocalPlayers 获取实时玩家，不保存场景 actor 包装器、不按旧路径全局查找。只有保留在 GameInstance 中的网格和类引用被缓存。
- 每次读取核对世界身份；世界变化停止巡检并增加 generation。LoadMap 清空状态；玩家或控制器 ReceiveEndPlay 使挂起回调失效。新的本地飞机 BeginPlay 可以重新启用同世界检查点恢复。
- 有限延迟任务只从当前玩家和武器数组查找匹配地址、路径的对象；无法确认时跳过，不回退全局查找。
- 检查点的网格重置、组件重建、缓存管理器替换仍由有限巡检恢复；稳定状态不重复写入。
- 类与网格只在准备阶段解析并由 GameInstance 持有。Prepare 内仍有一次 StaticFindObject；并未宣称整个 Mod 完全没有对象查找。
- 验收记录器改为读取 BeginPlay 的实时上下文，移除跨帧路径查询；因此过早尚未生成的装备信息可能不出现在该条记录中。
- 头盔瞄准直接读取当前飞机的 TargetSelectionComponent，移除采样中的类查找。

ReceiveEndPlay 使用 UE4SS RegisterHook；API 说明：https://docs.ue4ss.com/lua-api/global-functions/registerhook.html 。Blueprint 覆写可能影响事件覆盖，不能只依靠此事件，因此还保留实时世界身份与引用链检查。没有加入未确认存在的 RegisterEndPlayPreHook API。

## 验证

Lua 回归测试覆盖：运行期禁用全局查找、EndPlay 早于 LoadMap、退出后的旧任务、结束对象不得重新启用、同世界新玩家 BeginPlay、未收到 LoadMap 的世界切换、检查点网格/组件/管理器恢复、1000 次不变巡检零写入，以及三种安装范围和头盔目标选择。

这些是模拟生命周期测试，不是引擎 GC 压力测试。尚需游戏内完成退出任务、回机库、重新进任务和检查点重生验证。无法据此保证所有 UE4SS 原生崩溃均已消除。

旧 preview.3 分享包没有此修复，应使用新的 preview.4 包。原版包及本机设置保留用于回退。
