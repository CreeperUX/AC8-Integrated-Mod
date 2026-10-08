# v2.4.2 — 键盘接管修复

**正式版。仅限离线单人，禁止带 Mod 进入线上或多人模式。** 本版是修复包，包含 v2.4.1 的全部内容。

## 更新内容

- **修复：WAR 键盘接管迟滞**
  - 之前按住俯仰、滚转或偏航键后，要等原版按键输入爬升到约 80% 才接管（0.3–0.5 s），接管后指令还要从教官当时的值重新爬升，按键常常近 1 秒才明显生效。
  - 现在按住按键约 0.1–0.15 s 即接管，并接着按键的爬升继续，响应与不装 Mod 时的按键基本相同。
  - 按住按键直接反向（例如右滚直接换左滚）时，不再被鼠标飞控中途接回。
  - 轻点按键仍由鼠标飞控处理；松开后约 0.1 s 交回鼠标。俯仰／偏航键接管三轴、只按滚转键只接管滚转，这些规则不变。
- **修复：启动控制台的版本显示**：控制台窗口之前一直显示旧的"AC8 2.3.5 … F4 CLASSIC/AGILE"和"候选版"字样，现在按安装包实际的版本和状态显示，并列出 F4 PEACE/WAR 与 Alt+F7。
- 鼠标飞控的控制律与 v2.4.1 相同。

## 升级

与 v2.4.1 相同：
- 下载完整的 **AC8-Integrated-v2.4.2-share.zip**，解压到游戏目录之外的新目录；
- 运行 Start-GUI.cmd 或 Setup.cmd，并把新生成的 Steam 启动选项替换进去；
- 不要复制旧版的 DLL、设置或 sessions。

完整的功能说明见 [v2.4.1 说明](RELEASE-v2.4.1.md) 和 [WAR 飞控说明](WAR-FLIGHT-CONTROL.md)。

## English summary

**Stable release. Offline single-player only.** A fix release that contains everything from v2.4.1.

- **Fix: keyboard-takeover lag in WAR.**
  - Before, a held pitch, roll or yaw key took over only once the stock key input had ramped to about 80% (0.3-0.5 s), and the command then re-ramped from the instructor's last value, so keys often needed close to a second to bite.
  - A held key now takes over after about 0.1-0.15 s and continues along the key ramp, responding essentially like the stock keys.
  - Reversing a held key (e.g. right roll straight to left roll) is no longer taken back by the mouse instructor midway.
  - Short taps stay with the mouse instructor, and it takes back control about 0.1 s after release. Pitch/yaw keys still take all three axes and a roll key alone takes roll only.
- **Fix: launch console version.** The console window showed an old fixed "AC8 2.3.5 ... F4 CLASSIC/AGILE" candidate text; it now shows the package's actual version and status and lists F4 PEACE/WAR and Alt+F7.
- Mouse flight control is unchanged from v2.4.1.

Install as before: extract the complete AC8-Integrated-v2.4.2-share.zip to a new folder outside the game, run Start-GUI.cmd or Setup.cmd and replace the Steam launch option. Do not copy DLLs, settings or sessions between versions.
