# Contributing

请先阅读控制实现与已知限制。一次PR聚焦一个行为变化，并写明修改原因、验证方法以及哪些实机场景尚未验证。

- 保留机型分组、手动优先、数据失效回退、碰撞组件与无效UObject保护。
- 不绕过游戏版本、运行库哈希或安装归属检查。
- 原生测试运行 `native\test.cmd`；Lua测试分别执行 `python tests/test_visuals.py`、`python tests/test_integration.py`；模块安装测试运行 `tests/Test-Features.ps1`。
- 不提交DLL、游戏资源、存档、原始CSV、日志、凭据、本机路径或完整使用过的整合包。构建输出留在被忽略的目录。
- 分享问题证据时优先提供最小复现、版本/机型/档位/安装选项和脱敏日志片段。

GUI渲染测试需Windows硬件D3D设备，单独运行 `native\test-gpu.cmd`；它只渲染自己的隐藏窗口，不读取游戏画面。
