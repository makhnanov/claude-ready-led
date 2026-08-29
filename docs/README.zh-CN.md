# claude-ready-led

一个接在 ESP8266 上的 LED：**Claude Code 开始干活时熄灭**，**干完活时点亮**。
另外还有一个全系统生效的 `Ctrl+T` 快捷键，可以手动切换。

[English](../README.md) ·
[Русский](README.ru.md) ·
**简体中文** ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="Claude Code 工作时 LED 呼吸闪烁" width="440">
  <img src="ui.jpg" alt="开发板自己提供的网页界面" width="277">
</p>

硬件：NodeMCU v3（ESP8266）加任意一颗 LED。软件：内置 HTTP 服务器的固件、
Claude Code 钩子，以及一个全局 XFCE 快捷键。

---

## 安装钩子

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

脚本会把仓库克隆到 `~/.claude-ready-led`，把钩子写进 `~/.claude/settings.json`
（并在 `settings.json.bak` 留一份备份），把设备地址存到 `~/.claude-led.conf`，
最后检查开发板是否响应。可以重复运行：它只会替换自己写入的条目，不动其他钩子。

如果 mDNS 名字解析不了，就直接指定地址：

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

装完以后在 Claude Code 里打开 `/hooks`（这会重新加载配置），或者干脆重启它。

---

## 接线

`D6` 就是 `GPIO12`。

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                    正极   负极
                  （长脚）（短脚，
                           外圈有一个平边）
```

电阻是必须的：ESP8266 的引脚最多只能输出约 12 mA。

---

## 固件

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # 填 SSID 和密码，只支持 2.4 GHz

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` 已经写进 `.gitignore`，所以你的 Wi-Fi 密码不会进仓库。
版本库里只有带占位符的 `secrets.h.example`。

查看分配到的 IP：

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # 开发板启动时会打印 IP
ping claude-led.local                                    # 或者走 mDNS
```

---

## HTTP API

| 请求 | 作用 |
|---|---|
| `GET /on` | 常亮 |
| `GET /off` | 熄灭 |
| `GET /toggle` | 切换：灭的就点亮，其他情况一律熄灭 |
| `GET /pulse` | 平滑呼吸 —— Claude 正在干活 |
| `GET /blink?times=3&ms=200` | 闪烁 |
| `GET /done` | 快闪 3 下后保持常亮 —— 活干完了 |
| `GET /status` | JSON：模式、IP、信号强度、运行时长 |
| `GET /` | 网页界面（见下文） |

如果在 `secrets.h` 里设置了 `API_TOKEN`，每个请求都要带上 `?token=...`。

## 命令行

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
```

地址依次取自 `CLAUDE_LED_URL`、`~/.claude-led.conf`，最后回退到
`http://claude-led.local`。脚本永远以 0 退出，而且最多阻塞 2 秒 —— 正因为这样才
敢挂在钩子上：开发板拔了电或者连不上，都不会拖慢或者搞坏 Claude Code。

---

## 网页界面

在浏览器里打开 `http://claude-led.local/` —— 电脑或手机都行，因为 mDNS 在整个局域网
里都生效。开发板会自己提供一个页面，上面有 TOGGLE / ON / OFF / PULSE / BLINK / DONE
六个按钮、一个跟随当前模式的「灯泡」，还有实时状态：IP、信号强度、运行时长。

**按钮不会刷新页面。** 点击会发出 `fetch("/on")`，JSON 响应回来后立刻重绘状态和灯泡。
另外页面每 2 秒自己拉一次 `/status`，所以哪怕 LED 是被 Claude Code 的钩子或者手机
切换的，这个标签页也能看到。标签页不在前台时轮询会停下（`document.hidden`），
免得白白折腾开发板。

如果连接断了，灯泡会变红并显示「连不上开发板」—— 页面不会就这么默默卡住。

### 页面代码在哪

没有单独的文件：开发板根本没有文件系统。整个页面就是
`firmware/claude_led/claude_led.ino` 里的一个 C++ 字符串：

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` 把它和代码一起放进 flash，`send_P` 直接从那里发出去，不往 RAM 里复制。
这也正是状态要用 `/status` 单独取、而不是拼进 HTML 的原因：页面保持静态，就不需要
用 RAM 去拼装。换成这个方案省下了 80 KB 中的大约 650 字节 —— 在单片机上这不是小数目。

如果在 `secrets.h` 里设置了 `API_TOKEN`，就这样打开页面：
`http://claude-led.local/?token=xxxx` —— 它会从地址栏里取出 token，并加到自己发出的
所有请求上。

---

## Claude Code 钩子

`install.sh` 往 `~/.claude/settings.json` 里加的内容：

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` —— 你提交了提示词，Claude 开始干活：LED 熄灭。
- `Stop` —— Claude 回答完毕：闪三下然后常亮。

也就是**亮着 = 结果可以拿了，灭着 = 还在想**。如果你更希望它在工作期间呼吸而不是
熄灭，把钩子里的 `off` 改成 `pulse` 就行。

输出被特意丢进 `/dev/null`：否则 `UserPromptSubmit` 钩子的 stdout 会进到模型的上下文里。

想查看或者关掉它们，在 Claude Code 里用 `/hooks`。

---

## Ctrl+T —— 切换 LED

```bash
scripts/hotkey-xfce.sh install    # 绑定
scripts/hotkey-xfce.sh status     # 查看
scripts/hotkey-xfce.sh remove     # 解绑
```

通过 `xfconf` 注册一个全局 XFCE 快捷键：

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

灭的点亮，亮着或者在呼吸的就熄灭。全系统生效，立即生效，重启也还在。

**注意：** XFCE 是全局抢占这个键的，所以 `Ctrl+T` 就到不了应用程序里了 ——
浏览器里再按它不会开新标签页。要是觉得碍事，用 `remove` 解绑，然后换一个组合：

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

换个动作也是同样的方式：

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

可用动作：`toggle`（默认）、`on`、`off`、`pulse`、`done`。

### 不用 XFCE？

`hotkey-xfce.sh` 只管 XFCE。其他环境的等价做法：

| 桌面环境 | 怎么绑定 |
|---|---|
| GNOME | 设置 → 键盘 → 自定义快捷键，命令写 `led.sh toggle` |
| KDE | 系统设置 → 快捷键 → 自定义快捷键 |
| i3 / sway | 配置里写 `bindsym Control+t exec /path/to/led.sh toggle` |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| 裸 X11 | 用 `xbindkeys`，在 `~/.xbindkeysrc` 里写 `"led.sh toggle"` 加 `Control + t` |

---

## 目录结构

```
firmware/claude_led/claude_led.ino     ESP8266 固件
firmware/claude_led/secrets.h.example  Wi-Fi 配置模板（secrets.h 已被 gitignore）
scripts/led.sh                         命令行：on/off/toggle/pulse/blink/done/status
scripts/install.sh                     Claude Code 钩子安装脚本
scripts/hotkey-xfce.sh                 全局 Ctrl+T 快捷键
docs/                                  图片和各语言 README
```
