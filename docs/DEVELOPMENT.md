# 开发、构建与打包

## 工具链

- Windows x64；Visual Studio 2022 的 C++ x64 工具链与 Windows SDK。
- Python 3.13；Python/Lua测试与分析依赖见 `requirements-dev.txt`。
- 游戏不需要运行，原生单元测试也不安装或调用游戏。

```powershell
git clone https://github.com/CreeperUX/AC8-Integrated-Mod.git
cd AC8-Integrated-Mod
python -m pip install -r requirements-dev.txt
.\native\test.cmd
python -X utf8 tests/test_visuals.py
python -X utf8 tests/test_integration.py
powershell -NoProfile -ExecutionPolicy Bypass -File tests/Test-Features.ps1
```

`native/build.cmd` 自动通过 vswhere 定位 VS 工具链，产出 `native/build/ac8_mouse_aim_010.dll`。原生测试以延迟导入方式链接 UE4SS 接口，不需要复制运行库 DLL。GPU渲染测试需硬件D3D支持，单独运行 `native/test-gpu.cmd`。

## 目录

| 路径 | 内容 |
|---|---|
| `native/` | C++桥接、输入、三轴控制、在线学习、GPU GUI及单元测试 |
| `package-template/` | 便携安装/启动模板、功能选择、Lua运行模块；不含DLL |
| `tools/` | 可选离线记录分析 |
| `models/` | 派生标准/研究模型参数，无原始试飞CSV |
| `tests/` | Lua回归与安装选项测试 |
| `runtime-dependencies.json` | 固定运行库路径、长度、SHA256 |

当前包版本为2.3.0，原生飞控仍是2.2实现；2.3的变化集中在可选导弹安装流程。游戏内`PROFILE 2.2.0`与包版本2.3.0并不矛盾。

## 生成可运行ZIP

源码不提交预编译DLL。先下载本仓库已发布的便携包，将其解压到游戏目录以外，取其中 `payload` 作为固定UE4SS运行库输入。构建器只读取清单中两个运行库文件，并校验SHA256；不会读取其中的会话、存档或其它文件。

```powershell
python scripts/build-package.py --runtime-root "D:\Mods\AC8-Integrated-v2.3.1-share\payload"
```

产物在 `dist/`，包含运行DLL、源码、许可证、文档和两种安装选择。`--native-dll` 可指定你构建的原生DLL，`--output` 可指定一个尚不存在的输出目录。这个引导依赖已经发布的运行库包；脚本不会自动寻找或下载未知来源DLL。若需要从上游重建UE4SS，需要维持本项目所用ABI及明确更新运行库哈希，不能直接替换成最新DLL。

打包过程生成 `payload-manifest.json`、`FILES.sha256` 和 ZIP SHA256。运行时特性选择只影响暂存到游戏目录的模块，原始完整payload保持可校验。

## 修改导弹设置

当前导弹参数声明在 `package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8SourceInit/Scripts/source_spec.lua`。外观规则在同目录 `msl_visual_rules.lua`。发布前运行回归测试并重新生成包清单；不要让用户直接修改带完整性校验的payload来跳过验证。

不要将分析报告中的“字段已写入”当成命中、锁定、近炸、齐射或外观的实机证明。新增行为应区分代码测试与游戏内验证。

