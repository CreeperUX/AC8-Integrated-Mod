# 原生 UMG 圆环验证

2.3.13-umg-preview.1。由离线元数据和HUD蓝图确认：LiveMainHUDParent4K_C继承LiveHUD；LiveHUDBase及LiveHUD持有实际UMG容器；目标指示部件继承LiveTargetSelectionDraw，没有Blueprint OnPaint实现。因此本版采用直接向现有CanvasPanel附加一个UImage，由游戏UMG/Slate负责绘制。

仅引用游戏现有的圆环纹理，不分发游戏资源。Image及CanvasPanelSlot由当前HUD容器持有；Mod只保存身份与位置数值，更新时沿当前玩家HUD重新取得对象。原父容器销毁后由游戏释放控件，绘制失败会移除当前确认归属的Image并停止本模块。

圆环尺寸、位置使用当前容器几何和原有世界目标投影。F7隐藏/显示；F8仍只关闭飞控。没有外部叠加窗口，F1菜单和切换文字暂不绘制。飞控语义与缩放过渡沿用前版。

## 测试

沿用Steam入口进入任务，观察圆环是否出现、是否跟随原目标，并用F7比较帧时间。正常退出等待日志归档即可。若没有圆环，请提供日志，不必重复试飞。

AC8UMGHUD ATTACHED表示创建并挂载，POSITION_UPDATED表示布局写入返回成功；均不等于屏幕可见。UMG_HUD记录检查、有效投影、布局更新和CPU时间，不能当作GPU显示FPS。

本地模拟测试覆盖单Image创建、稳定位置不重复设置布局、F7及无玩家隐藏、组件更换和错误后清理。实际UE对象创建、纹理显示和帧率收益仍需游戏验证。

仅限离线单人。参与线上游戏前退出并运行Disable-Mod.cmd，详情见MOD-DISABLE.md。
