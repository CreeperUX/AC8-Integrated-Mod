# AC8 Integrated Mod — v2.3.15-beta.1

**可选 Beta 测试版。推荐正式版仍为 [v2.3.6](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6)；出现问题可按下方说明回退。仅限离线单人，禁止用于线上或多人游戏。**

此发布分支包含当前原生 HUD 候选的完整源码。圆环、HMD 标记和方向刻度使用原生 UMG；F1 面板及通知按需使用外部 GPU 界面。圆环放大并修正快照混用与布局跳变，实际顺滑度仍待更广泛实测。保留原有三轴模型飞控、三种安装范围及原作者署名。

- [Beta 完整更新、安装及回退说明](docs/RELEASE-v2.3.15-beta.1.md)
- [安装与三种范围](docs/INSTALL.md) · [游戏内调整](docs/IN-GAME-SETTINGS.md) · [快捷停用](docs/MOD-DISABLE.md)
- [原生 HUD 实现及限制](docs/NATIVE-UI-STYLE.md) · [按键参考](docs/KEYBINDINGS.md) · [控制原理](docs/CONTROL.md)
- [GitHub Beta 下载](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.15-beta.1) · [2.3.6 正式版与回退下载](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/v2.3.6)

下载完整 share.zip，不要用 Source code ZIP 安装。解压到游戏目录之外的新文件夹，运行 Start-GUI.cmd／Setup.cmd，保存后将新生成的 Steam 启动选项替换进去。退出时等待控制台清理。回退需先完成 Beta 清理，再安装 2.3.6 并更换启动选项。

**不保证原生圆环达到 120 Hz，也不保证 F1／通知对所有设备零性能影响。** 面板与圆环绘制路径分开，但仍共享硬件与部分状态。当前原生圆环不使用旧实验中的插值或预测补帧。

基于 [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim) 发展，感谢原作者及贡献者。完整署名和许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

[English Beta overview](README.en.md)
