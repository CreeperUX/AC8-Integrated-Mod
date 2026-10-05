# 游戏内 CreeperUX UI Kit

2.3.15-beta.1 延续 F1 面板及 F2/F3/F4/灵敏度通知统一为 CreeperUX 深色样式。

- 颜色来自 `ui/creeperux/tokens.json`：panel、raised、line、boundary、中性文字与金色 accent-soft/action。信息通知使用 info，HMD 不可用使用 warn。
- 标题 Chakra Petch，数值与键帽 Share Tech Mono，正文 Segoe UI。字体从随包文件建立 DirectWrite 私有字体集合，不安装到系统。无法加载时回退系统字体。
- 字号、圆角、通知退出时间由 `scripts/generate-hud-theme.py` 生成到原生 header；构建与打包核验其与 kit 一致。UI 不在渲染循环读取 JSON 或字体文件。
- 分组设置、细分隔线、键帽、当前值与启动值分列；F1 即时开关，通知保留 2.5 秒期限，最后 90 ms 淡出。没有位移或弹跳。
- 面板按画面尺寸缩放并限制在视口内。面板与通知同时显示时，通知放在余下区域。低分辨率可能使文字较小。
- 默认适配范围是按需 GPU 文字面板和通知；圆环及方向刻度使用原生UMG。混合模式不自动回退到第二套外部圆环。

操作方式仍是 F1 开关面板、Ctrl+PageUp/PageDown/Home 调整本次灵敏度及原有 F2–F10/C 操作。没有自动校准、鼠标可点击菜单或永久保存。见 IN-GAME-SETTINGS.md。

验证包含私有字体实际加载、各通知分支、硬件绘制及 640×360、1280×720、1920×1080、3840×2160 分辨率。隐藏窗口测试不是实际游戏画面或显示帧率证明。
