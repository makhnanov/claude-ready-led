# claude-ready-led

Светодиод на ESP8266, который **гаснет, когда Claude Code начал работать**, и
**загорается, когда он закончил**. Плюс системный хоткей `Ctrl+T`, чтобы переключить
его вручную.

[English](../README.md) ·
**Русский** ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="Светодиод дышит, пока работает Claude Code" width="440">
  <img src="ui.jpg" alt="Веб-интерфейс, который отдаёт сама плата" width="277">
</p>

Железо: NodeMCU v3 (ESP8266) и любой светодиод. Софт: прошивка с HTTP-сервером,
хуки Claude Code и глобальный хоткей XFCE.

---

## Установка хука

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Клонирует репозиторий в `~/.claude-ready-led`, вписывает хуки в `~/.claude/settings.json`
(с бэкапом в `settings.json.bak`), сохраняет адрес устройства в `~/.claude-led.conf`
и проверяет связь. Запускать можно повторно — прежние записи заменяются, чужие хуки
не трогаются.

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
| `GET /toggle` | переключить: погашен → зажечь, иначе погасить |
| `GET /pulse` | плавное «дыхание» — Claude работает |
| `GET /blink?times=3&ms=200` | мигание |
| `GET /done` | 3 быстрых мигания и остаться гореть — работа закончена |
| `GET /status` | JSON: режим, IP, RSSI, аптайм |
| `GET /` | веб-интерфейс (см. ниже) |

Если в `secrets.h` задан `API_TOKEN`, к каждому запросу добавляется `?token=...`.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
```

Адрес берётся из `CLAUDE_LED_URL`, из `~/.claude-led.conf`, иначе `http://claude-led.local`.
Скрипт всегда выходит с кодом 0 и не висит дольше 2 секунд — поэтому его безопасно
вешать на хуки: выключенная или недоступная ESP не тормозит и не ломает Claude Code.

---

## Веб-интерфейс

Открой в браузере `http://claude-led.local/` — с компьютера или с телефона,
mDNS работает по всей локальной сети. Плата сама отдаёт страницу с кнопками
TOGGLE / ON / OFF / PULSE / BLINK / DONE, «лампочкой», повторяющей текущий режим,
и живым статусом: IP, уровень сигнала, аптайм.

**Кнопки не перезагружают страницу.** Клик отправляет `fetch("/on")`, ответ
приходит JSON-ом и тут же перерисовывает статус и лампочку. Плюс раз в 2 секунды
страница сама подтягивает `/status`, так что если светодиод переключили хуком
Claude Code или с телефона — вкладка это увидит. Опрос останавливается, когда
вкладка не активна (`document.hidden`), чтобы не долбить плату впустую.

Если связь пропала, лампочка становится красной и появляется «нет связи с платой» —
страница не зависает и не молчит.

### Где лежит код страницы

Отдельного файла нет: у платы нет файловой системы. Вся страница — это одна
строка C++ в `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` кладёт её во flash рядом с кодом, а `send_P` отдаёт прямо оттуда, не
копируя в оперативку. Именно поэтому статус берётся отдельным запросом `/status`,
а не подставляется в HTML: страница остаётся статической и ей не нужна RAM на сборку.
Переход на эту схему освободил около 650 байт из 80 КБ — на микроконтроллере это
заметная величина.

Если в `secrets.h` задан `API_TOKEN`, открывай страницу как
`http://claude-led.local/?token=xxxx` — она подхватит токен из адресной строки
и добавит его ко всем своим запросам.

---

## Хуки Claude Code

Что `install.sh` дописывает в `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — ты отправил промпт, Claude начал работать: светодиод гаснет.
- `Stop` — Claude закончил отвечать: три мига и ровный свет.

То есть **горит = можно забирать результат, погашен = ещё думает**. Если хочется,
чтобы во время работы он не гас, а плавно дышал, поменяй в хуке `off` на `pulse`.

Вывод глушится в `/dev/null` намеренно: stdout хука `UserPromptSubmit` иначе
попадает в контекст модели.

Посмотреть или отключить — команда `/hooks` в Claude Code.

---

## Ctrl+T — переключить светодиод

```bash
scripts/hotkey-xfce.sh install    # повесить
scripts/hotkey-xfce.sh status     # проверить
scripts/hotkey-xfce.sh remove     # снять
```

Вешает глобальный хоткей XFCE через `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

Погашен — зажжётся, горит или дышит — погаснет. Работает во всей системе,
применяется сразу и переживает перезагрузку.

**Важно:** XFCE перехватывает клавишу глобально, поэтому `Ctrl+T` перестаёт
доходить до приложений — в браузере больше не откроется новая вкладка. Если это
мешает, сними хоткей командой `remove` и повесь другую комбинацию:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

Другое действие вместо `toggle` — тоже переменной:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

Доступные действия: `toggle` (по умолчанию), `on`, `off`, `pulse`, `done`.

### Не XFCE?

`hotkey-xfce.sh` работает только в XFCE. Аналоги для остальных:

| Окружение | Как повесить |
|---|---|
| GNOME | Настройки → Клавиатура → Дополнительные комбинации, команда `led.sh toggle` |
| KDE | Параметры системы → Комбинации клавиш → Особые команды |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` в конфиге |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| голый X11 | `xbindkeys` с `"led.sh toggle"` + `Control + t` в `~/.xbindkeysrc` |

---

## Структура

```
firmware/claude_led/claude_led.ino     прошивка ESP8266
firmware/claude_led/secrets.h.example  шаблон WiFi-конфига (secrets.h в .gitignore)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     установщик хуков Claude Code
scripts/hotkey-xfce.sh                 глобальный хоткей Ctrl+T
docs/                                  картинки и переводы README
```
