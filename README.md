# claude-ready-led

Светодиод на ESP8266, который **дышит, пока Claude Code работает**, и **загорается
ровным светом, когда он закончил**. Плюс системный хоткей `Ctrl+T`, чтобы погасить.

Железо: NodeMCU v3 (ESP8266) + любой светодиод. Софт: прошивка с HTTP-сервером,
хуки Claude Code и глобальный хоткей XFCE.

---

## Установка хука

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Клонирует репозиторий в `~/.claude-ready-led`, вписывает хуки в `~/.claude/settings.json`
(с бэкапом в `settings.json.bak`), сохраняет адрес устройства в `~/.claude-led.conf`
и проверяет связь. Запускать можно повторно — прежние записи `led.sh` заменяются,
чужие хуки не трогаются.

Адрес устройства, если mDNS-имя не резолвится:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

После установки открой в Claude Code `/hooks` (это перечитывает конфиг) или перезапусти его.

---

## Схема

`D6` — это `GPIO12`.

```
D6 ──[ 220–330 Ом ]──▶| LED ──── GND
                    анод  катод
                  (длинная (короткая ножка,
                   ножка)   срез на ободке)
```

Резистор обязателен: ножка ESP8266 отдаёт максимум ~12 мА.

---

## Прошивка

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID и пароль, только 2.4 GHz

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` в `.gitignore` — пароль от WiFi в репозиторий не попадает.
В git лежит только `secrets.h.example` с плейсхолдерами.

Узнать выданный IP:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # плата печатает IP при старте
ping claude-led.local                                    # либо через mDNS
```

---

## HTTP API

| Запрос | Что делает |
|---|---|
| `GET /on` | ровный свет |
| `GET /off` | погасить |
| `GET /pulse` | плавное «дыхание» — Claude работает |
| `GET /blink?times=3&ms=200` | мигание |
| `GET /done` | 3 быстрых мигания и остаться гореть — работа закончена |
| `GET /status` | JSON: режим, IP, RSSI, аптайм |
| `GET /` | веб-страничка с кнопками |

Если в `secrets.h` задан `API_TOKEN`, к каждому запросу добавляется `?token=...`.

## CLI

```bash
scripts/led.sh on | off | pulse | done | blink 5 100 | status
```

Адрес берётся из `CLAUDE_LED_URL`, из `~/.claude-led.conf`, иначе `http://claude-led.local`.
Скрипт всегда выходит с кодом 0 и не висит дольше 2 секунд — поэтому его безопасно
вешать на хуки: выключенная или недоступная ESP не тормозит и не ломает Claude Code.

---

## Хуки Claude Code

Что `install.sh` дописывает в `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh pulse >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — ты отправил промпт, светодиод начинает дышать.
- `Stop` — Claude закончил отвечать, три мига и ровный свет.

Вывод глушится в `/dev/null` намеренно: stdout хука `UserPromptSubmit` иначе
попадает в контекст модели.

Посмотреть или отключить — команда `/hooks` в Claude Code.

---

## Ctrl+T — погасить светодиод

```bash
scripts/hotkey-xfce.sh install    # повесить
scripts/hotkey-xfce.sh status     # проверить
scripts/hotkey-xfce.sh remove     # снять
```

Вешает глобальный хоткей XFCE через `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh off
```

Работает во всей системе, применяется сразу и переживает перезагрузку.

**Важно:** XFCE перехватывает клавишу глобально, поэтому `Ctrl+T` перестаёт
доходить до приложений — в браузере больше не откроется новая вкладка, в Claude Code
не переключится панель задач. Если это мешает, сними хоткей командой `remove`
и повесь другую комбинацию:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

Другое действие вместо `off` — тоже переменной:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

### Не XFCE?

`hotkey-xfce.sh` работает только в XFCE. Аналоги для остальных:

| Окружение | Как повесить |
|---|---|
| GNOME | Настройки → Клавиатура → Дополнительные комбинации, команда `led.sh off` |
| KDE | Параметры системы → Комбинации клавиш → Особые команды |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh off` в конфиге |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh off` |
| голый X11 | `xbindkeys` с `"led.sh off"` + `Control + t` в `~/.xbindkeysrc` |

---

## Структура

```
firmware/claude_led/claude_led.ino   прошивка ESP8266
firmware/claude_led/secrets.h.example  шаблон WiFi-конфига (secrets.h в .gitignore)
scripts/led.sh                       CLI: on/off/pulse/blink/done/status
scripts/install.sh                   установщик хуков Claude Code
scripts/hotkey-xfce.sh               глобальный хоткей Ctrl+T
```
