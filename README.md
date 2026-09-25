# claude-ready-led

An LED on an ESP8266 that **goes dark when Claude Code starts working** and
**lights up when it's done**. Plus a system-wide `Ctrl+T` hotkey to toggle it by hand.

**English** ·
[Русский](docs/README.ru.md) ·
[简体中文](docs/README.zh-CN.md) ·
[Español](docs/README.es.md) ·
[हिन्दी](docs/README.hi.md) ·
[العربية](docs/README.ar.md) ·
[Português](docs/README.pt-BR.md) ·
[Français](docs/README.fr.md) ·
[Deutsch](docs/README.de.md) ·
[日本語](docs/README.ja.md) ·
[한국어](docs/README.ko.md)

<p align="center">
  <img src="docs/demo.gif" alt="The LED breathing while Claude Code works" width="440">
  <img src="docs/ui.jpg" alt="Web interface served by the board itself" width="277">
</p>

Hardware: NodeMCU v3 (ESP8266) and any LED. Software: firmware with a built-in HTTP
server, Claude Code hooks, and a global XFCE hotkey.

---

## Install the hook

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Clones the repo into `~/.claude-ready-led`, writes the hooks into
`~/.claude/settings.json` (keeping a backup at `settings.json.bak`), stores the device
address in `~/.claude-led.conf`, and checks that the board answers. Safe to re-run:
it replaces its own entries and leaves any other hooks alone.

If the mDNS name doesn't resolve, pass the address explicitly:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

Afterwards open `/hooks` in Claude Code (that reloads the config) or restart it.

---

## Wiring

`D6` is `GPIO12`.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                  anode  cathode
                 (long leg) (short leg,
                             flat edge on the rim)
```

The resistor is mandatory: an ESP8266 pin sources about 12 mA at most.

---

## Firmware

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID and password, 2.4 GHz only

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` is in `.gitignore`, so your Wi-Fi password never reaches the repository.
Only `secrets.h.example` with placeholders is tracked.

Find the assigned IP:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # the board prints its IP on boot
ping claude-led.local                                    # or via mDNS
```

---

## HTTP API

| Request | Effect |
|---|---|
| `GET /on` | steady light |
| `GET /off` | turn off |
| `GET /toggle` | toggle: dark → light, anything else → dark |
| `GET /pulse` | smooth breathing — Claude is working |
| `GET /blink?times=3&ms=200` | blink |
| `GET /done` | 3 quick blinks, then stay lit — work finished |
| `GET /status` | JSON: mode, IP, RSSI, uptime |
| `GET /` | web interface (see below) |

If `API_TOKEN` is set in `secrets.h`, every request needs `?token=...`.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
scripts/led.sh find     # locate the board on the network and cache its address
scripts/led.sh where    # print the address currently in use
```

The address comes from `CLAUDE_LED_URL` (env or `~/.claude-led.conf`), then the cached
IP in `~/.cache/claude-led/ip`, which `find` writes and which the script refreshes on
its own whenever the cached address stops answering (at most once a minute — see
`CLAUDE_LED_SCAN_COOLDOWN`). So a DHCP lease change fixes itself; you do not have to
edit any config.

Run from a hook or a hotkey — anywhere stdout is not a terminal — a command that acts
on the LED detaches immediately and does the network work in the background: it returns
in a few milliseconds and always exits 0, which is what makes it safe to attach to
hooks. Typed by hand in a terminal it stays in the foreground and prints the board's
reply as before. `CLAUDE_LED_WAIT=1` forces waiting, `CLAUDE_LED_WAIT=0` forces
detaching.

Note that `claude-led.local` is never resolved on the hook path: a failing mDNS lookup
costs a full 5 seconds, which used to blow the hook timeout whenever the board was off.
Name resolution happens only inside `find`. If the cached address stops answering,
`find` also sweeps your local `/24` — port 80 only, one probe per host — so on a network
where that is unwelcome, pin the address with `CLAUDE_LED_URL` instead.

---

## Web interface

Open `http://claude-led.local/` in a browser — from your computer or your phone, since
mDNS works across the whole local network. The board serves a page with TOGGLE / ON /
OFF / PULSE / BLINK / DONE buttons, a "lamp" mirroring the current mode, and live
status: IP, signal strength, uptime.

**Buttons don't reload the page.** A click sends `fetch("/on")`, the JSON response
comes back and immediately repaints the status and the lamp. On top of that the page
polls `/status` every 2 seconds, so if the LED is switched by a Claude Code hook or
from a phone, the tab notices. Polling stops while the tab is hidden
(`document.hidden`) so the board isn't hammered for nothing.

If the connection drops, the lamp turns red and "no connection to the board" appears —
the page never just hangs silently.

### Where the page lives

There is no separate file: the board has no filesystem. The whole page is a single C++
string in `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` puts it in flash next to the code and `send_P` streams it straight from
there without copying into RAM. That is exactly why the status is fetched separately
via `/status` instead of being interpolated into the HTML: the page stays static and
needs no RAM to be assembled. Moving to this scheme freed about 650 bytes out of
80 KB — a meaningful amount on a microcontroller.

If `API_TOKEN` is set in `secrets.h`, open the page as
`http://claude-led.local/?token=xxxx` — it picks the token up from the query string
and appends it to all of its own requests.

---

## Claude Code hooks

What `install.sh` adds to `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — you submitted a prompt and Claude started working: the LED goes dark.
- `Stop` — Claude finished answering: three blinks and a steady light.

So **lit = your result is ready, dark = still thinking**. If you'd rather have it
breathe during work instead of going dark, change `off` to `pulse` in the hook.

Output is deliberately sent to `/dev/null`: stdout from a `UserPromptSubmit` hook
otherwise ends up in the model's context.

Use `/hooks` in Claude Code to inspect or disable them.

---

## Ctrl+T — toggle the LED

```bash
scripts/hotkey-xfce.sh install    # bind it
scripts/hotkey-xfce.sh status     # check
scripts/hotkey-xfce.sh remove     # unbind
```

Registers a global XFCE hotkey through `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

Dark turns on, lit or breathing turns off. Works system-wide, applies immediately and
survives a reboot.

**Heads up:** XFCE grabs the key globally, so `Ctrl+T` stops reaching applications —
your browser will no longer open a new tab with it. If that bothers you, `remove` it
and bind a different combination:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

A different action instead of `toggle` works the same way:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

Available actions: `toggle` (default), `on`, `off`, `pulse`, `done`.

### Not on XFCE?

`hotkey-xfce.sh` only handles XFCE. Equivalents elsewhere:

| Desktop | How to bind |
|---|---|
| GNOME | Settings → Keyboard → Custom Shortcuts, command `led.sh toggle` |
| KDE | System Settings → Shortcuts → Custom Shortcuts |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` in the config |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| bare X11 | `xbindkeys` with `"led.sh toggle"` + `Control + t` in `~/.xbindkeysrc` |

---

## Layout

```
firmware/claude_led/claude_led.ino     ESP8266 firmware
firmware/claude_led/secrets.h.example  Wi-Fi config template (secrets.h is gitignored)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     Claude Code hook installer
scripts/hotkey-xfce.sh                 global Ctrl+T hotkey
docs/                                  images and translated READMEs
```
