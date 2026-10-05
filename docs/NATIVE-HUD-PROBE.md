> 历史实验记录，不作为当前 Beta 操作说明。当前行为见 [Beta 发布说明](RELEASE-v2.3.15-beta.1.md)。

# 原生 HUD 最小验证版

版本：2.3.12-nativehud-preview.1。控制基线来自2.3.11/2.3.9的世界方向控制逻辑。默认 `hud_renderer=2`。

## 本次验证范围

仅尝试在游戏 ReceiveDrawHUD 事件中绘制一枚24段圆环；Canvas及HUD对象只在回调期间使用，不缓存到后续帧。先注册引擎HUD事件；若当前HUD实例没有触发基础回调，再有限次数尝试其实际类的同名事件。

原生验证模式的外部叠加线程在创建窗口、GPU设备和交换链之前直接退出。不会自动降级到外部窗口，避免出现“看见圆环却并未接入原生HUD”的误判。实际游戏是否调用此事件、UE4SS参数封送是否支持该调用，仍待这次试飞验证。

F1菜单、连线和切换文字通知暂不绘制；F2/F3/F4等操作继续可用。F7开关验证圆环，F8开关鼠标飞控。目标在屏幕外或暂停/过场/原生注视时不绘制。显示预测不用于这个原生圆环。

## 测试

1. 正常退出旧版并等待清理，使用原Steam入口启动。
2. 进入任务，观察是否出现随鼠标目标移动的圆环。
3. 在相同场景按F7比较显示/隐藏圆环前后的帧时间与手感。
4. 做一次暂停、检查点重生或退出任务；正常退出并等待归档。

若没有圆环，不需要反复尝试：日志可以区分“挂钩注册失败”“事件没有调用”“Canvas/玩家/投影条件未满足”“绘制调用发生错误”。飞控本身仍运行；可按F8关闭辅助。使用旧包恢复即可。

## 记录解释

- `[AC8CanvasHUD] HOOK ... registered=true` 只证明注册成功。
- `CALLBACK observed` 证明进入事件回调。
- `DRAW_SUBMITTED` 证明24次绘制调用返回成功，不代表屏幕实际可见。
- `CANVAS_HUD` 每10秒记录 callbacks、eligible、drawn、draw_hz、平均/最大CPU耗时及错误数；draw_hz不等于GPU呈现FPS。
- `external overlay thread exits ...` 证明本模式没有创建外部图形叠加资源。

## 验证与限制

本地通过模拟事件的Canvas有效期、门控、错误停用、重复继承挂钩保护，以及原生投影/身份/过期检查。没有在无人确认的情况下启动游戏，也没有把回调注册成功当作游戏接入成功。24段线调用含Lua/UE4SS封送开销，这是路由验证，不能用其结果直接推断完整GUI最终性能。

官方接口依据：
- https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AHUD/ReceiveDrawHUD
- https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AHUD
- https://docs.ue4ss.com/dev/lua-api/global-functions/registerhook.html

若当前HUD不走该事件，则需研究真实的DrawHUD/PostRender入口；此版不会猜测虚表槽位或强行替换游戏HUD。
