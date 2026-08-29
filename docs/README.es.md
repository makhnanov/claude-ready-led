# claude-ready-led

Un LED conectado a un ESP8266 que **se apaga cuando Claude Code empieza a trabajar** y
**se enciende cuando termina**. Además, un atajo global `Ctrl+T` para alternarlo a mano.

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
**Español** ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="El LED respirando mientras Claude Code trabaja" width="440">
  <img src="ui.jpg" alt="Interfaz web servida por la propia placa" width="277">
</p>

Hardware: NodeMCU v3 (ESP8266) y cualquier LED. Software: firmware con servidor HTTP
incorporado, hooks de Claude Code y un atajo global de XFCE.

---

## Instalar el hook

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Clona el repositorio en `~/.claude-ready-led`, escribe los hooks en
`~/.claude/settings.json` (guardando una copia en `settings.json.bak`), almacena la
dirección del dispositivo en `~/.claude-led.conf` y comprueba que la placa responde.
Se puede volver a ejecutar sin problema: reemplaza sus propias entradas y deja
intactos los demás hooks.

Si el nombre mDNS no se resuelve, pasa la dirección explícitamente:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

Después abre `/hooks` en Claude Code (eso recarga la configuración) o reinícialo.

---

## Conexión

`D6` es `GPIO12`.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                   ánodo  cátodo
                 (pata larga) (pata corta,
                               borde plano en el reborde)
```

La resistencia es obligatoria: un pin del ESP8266 entrega como mucho unos 12 mA.

---

## Firmware

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID y contraseña, solo 2,4 GHz

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` está en `.gitignore`, así que tu contraseña de Wi-Fi nunca llega al
repositorio. Solo se versiona `secrets.h.example` con marcadores de posición.

Para averiguar la IP asignada:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # la placa imprime su IP al arrancar
ping claude-led.local                                    # o mediante mDNS
```

---

## API HTTP

| Petición | Efecto |
|---|---|
| `GET /on` | luz fija |
| `GET /off` | apagar |
| `GET /toggle` | alternar: apagado → encender, cualquier otro estado → apagar |
| `GET /pulse` | respiración suave: Claude está trabajando |
| `GET /blink?times=3&ms=200` | parpadeo |
| `GET /done` | 3 parpadeos rápidos y se queda encendido: trabajo terminado |
| `GET /status` | JSON: modo, IP, RSSI, tiempo encendida |
| `GET /` | interfaz web (ver más abajo) |

Si defines `API_TOKEN` en `secrets.h`, cada petición necesita `?token=...`.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
```

La dirección se toma de `CLAUDE_LED_URL`, luego de `~/.claude-led.conf` y por último
`http://claude-led.local`. El script siempre sale con código 0 y nunca se bloquea más
de 2 segundos: por eso es seguro colgarlo de un hook. Una placa desenchufada o
inalcanzable no ralentiza ni rompe Claude Code.

---

## Interfaz web

Abre `http://claude-led.local/` en un navegador, desde el ordenador o desde el móvil,
ya que mDNS funciona en toda la red local. La placa sirve una página con botones
TOGGLE / ON / OFF / PULSE / BLINK / DONE, una «bombilla» que refleja el modo actual y
estado en vivo: IP, intensidad de señal y tiempo encendida.

**Los botones no recargan la página.** Un clic envía `fetch("/on")`, la respuesta JSON
vuelve y repinta inmediatamente el estado y la bombilla. Además la página consulta
`/status` cada 2 segundos, así que si el LED lo cambia un hook de Claude Code o el
móvil, la pestaña se entera. El sondeo se detiene mientras la pestaña está oculta
(`document.hidden`) para no machacar la placa sin motivo.

Si se cae la conexión, la bombilla se pone roja y aparece «sin conexión con la placa»:
la página nunca se queda colgada en silencio.

### Dónde vive la página

No hay un archivo aparte: la placa no tiene sistema de archivos. Toda la página es una
sola cadena de C++ en `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` la coloca en la flash junto al código y `send_P` la envía directamente desde
ahí, sin copiarla a la RAM. Por eso justamente el estado se pide aparte con `/status`
en lugar de interpolarlo en el HTML: la página se mantiene estática y no necesita RAM
para montarse. Cambiar a este esquema liberó unos 650 bytes de 80 KB, una cantidad
nada despreciable en un microcontrolador.

Si defines `API_TOKEN` en `secrets.h`, abre la página como
`http://claude-led.local/?token=xxxx`: tomará el token de la barra de direcciones y lo
añadirá a todas sus propias peticiones.

---

## Hooks de Claude Code

Lo que `install.sh` añade a `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit`: enviaste un prompt y Claude empezó a trabajar, el LED se apaga.
- `Stop`: Claude terminó de responder, tres parpadeos y luz fija.

Es decir, **encendido = tu resultado está listo, apagado = todavía está pensando**. Si
prefieres que respire durante el trabajo en vez de apagarse, cambia `off` por `pulse`
en el hook.

La salida se manda a `/dev/null` a propósito: el stdout de un hook `UserPromptSubmit`
acabaría en el contexto del modelo.

Usa `/hooks` en Claude Code para revisarlos o desactivarlos.

---

## Ctrl+T — alternar el LED

```bash
scripts/hotkey-xfce.sh install    # asignar
scripts/hotkey-xfce.sh status     # comprobar
scripts/hotkey-xfce.sh remove     # quitar
```

Registra un atajo global de XFCE a través de `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

Apagado se enciende; encendido o respirando se apaga. Funciona en todo el sistema, se
aplica al momento y sobrevive a un reinicio.

**Ojo:** XFCE captura la tecla de forma global, así que `Ctrl+T` deja de llegar a las
aplicaciones: tu navegador ya no abrirá una pestaña nueva con él. Si te molesta,
quítalo con `remove` y asigna otra combinación:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

Otra acción en lugar de `toggle` funciona igual:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

Acciones disponibles: `toggle` (por defecto), `on`, `off`, `pulse`, `done`.

### ¿No usas XFCE?

`hotkey-xfce.sh` solo cubre XFCE. Equivalentes en otros entornos:

| Escritorio | Cómo asignarlo |
|---|---|
| GNOME | Configuración → Teclado → Atajos personalizados, comando `led.sh toggle` |
| KDE | Preferencias del sistema → Atajos → Atajos personalizados |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` en la configuración |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| X11 puro | `xbindkeys` con `"led.sh toggle"` y `Control + t` en `~/.xbindkeysrc` |

---

## Estructura

```
firmware/claude_led/claude_led.ino     firmware del ESP8266
firmware/claude_led/secrets.h.example  plantilla de configuración Wi-Fi (secrets.h está en gitignore)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     instalador de los hooks de Claude Code
scripts/hotkey-xfce.sh                 atajo global Ctrl+T
docs/                                  imágenes y READMEs traducidos
```
