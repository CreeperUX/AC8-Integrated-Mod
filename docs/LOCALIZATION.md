# 英语软件界面准备 / English-interface preparation

状态：**文档双语已完成；软件本地化资源已准备，尚未接入运行时。**

English documentation is available. Paired UI/message resources are prepared, but the application does not consume them yet. The published v2.3.6 interface remains primarily Chinese. This work does not change or replace its release archive.

## 已落地

- `README.md` / `README.en.md` 双向切换。
- `docs/en/INSTALL.md`、`KEYBINDINGS.md`、`CLEANUP.md`、`LIMITATIONS.md`，与中文页互相链接。
- `localization/zh-CN.json`、`localization/en-US.json`：第一批稳定消息 ID，涵盖主界面、三模式、确认框、关键状态/错误及可选分析。
- `tools/check-localization.py`：重复键、语言键集合、空文案、占位符和公开文档本地链接检查。

这些资源是接入基础，不代表所有脚本提示已翻译。当前字体已包含拉丁字体和中文回退；英语文案仍需真实布局验证。

## 接入顺序

1. **语言选择与读取器。** GUI 支持 `auto / zh-CN / en-US`。自动模式读取 Windows UI 语言；中文使用 zh-CN，其余使用 en-US。提供可见的语言选择，手动选择保存到独立 UI 偏好文件，不混入游戏或导弹配置。只有接入并验证后才宣称支持英语界面。
2. **界面与辅助信息。** XAML 文本、按钮、路径提示、Tooltip、AutomationProperties、文件选择器、主题按钮、关闭提示和清理确认框使用稳定消息 ID。缺少英文翻译应在开发/发布检查时失败。
3. **后台操作。** 每次任务显式传递语言；工作线程返回状态 ID、错误代码与格式化参数，GUI 决定显示文本。不要靠替换中文异常字符串判断成功、权限或文件归属。原始系统异常保留在诊断记录中。
4. **全部命令行入口。** 覆盖 Setup、Choose-Features、Run-Console、启动、清理、环境定位、可选分析及隐藏 GUI 启动失败。前三种模式名称可翻译，配置值、文件名、就绪日志匹配和哈希不可翻译。
5. **英语布局与验收。** 覆盖两种主题、小窗口/高 DPI、长英语提示、候选列表、键盘焦点、清理确认、冷启动、重开保持语言、失败回滚与诊断。保留无主界面滚动条的要求，防止单纯缩小到不可读。
6. **新测试包。** 先生成新的中英测试包并验收，再按用户指示发布；不要替换已有 v2.3.6 资产。

## 不随语言改变的约定

- `guidance / full / none`、`missile_mode`、旧配置迁移、控制参数和游戏 Build/hash。
- F2/F3/F4 等固定快捷键；游戏按键名称可翻译显示，但底层标识保持不变。
- 清理的所有权规则、备份校验、运行中拒绝处理、未知文件保留。
- Steam 启动选项中的路径和 `%command%`；不得给生成的命令加入翻译文字。
- 游戏逻辑、原生 DLL、HUD 行为与目标选择判断。

## 文案与术语

| 概念 | English |
|---|---|
| 鼠标飞控 | Mouse-directed flight control |
| 仅比例引导 | Guidance only |
| 完整导弹强化 | Full missile enhancements |
| 仅鼠标飞控 | Flight control only |
| 头盔瞄准具 | HMD / cursor-prioritized target selection |
| 自由观察 | Free look |
| 备份并清理 | Back up and clean up |

使用普通、直接的操作语句。不要将 target selection 翻译成 weapon lock，也不要把 guidance-only 表述为性能增强。风险与确认提示应完整翻译，不能只翻译按钮而保留正文为另一语言。

当前目录采用命名占位符（如 `{path}`、`{mode}`）。未来读取器必须安全替换显示文本，不执行文案；不能直接把这些命名占位符交给仅接受位置编号的 PowerShell `-f`。

运行检查：`python -X utf8 tools/check-localization.py`。
