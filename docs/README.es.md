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

O deja que lo haga el script: instala el core, crea `secrets.h` a partir del ejemplo y
encuentra el puerto por su cuenta:

```bash
scripts/flash.sh              # placa principal
scripts/flash.sh button       # el botón inalámbrico (más abajo)
scripts/flash.sh button -m    # y abrir después el monitor serie
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
scripts/led.sh find     # localizar la placa en la red y guardar su dirección
scripts/led.sh where    # mostrar la dirección que se está usando
```

La dirección se toma de `CLAUDE_LED_URL` (entorno o `~/.claude-led.conf`) y, si no, de
la IP cacheada en `~/.cache/claude-led/ip`: la escribe `find`, y el script la refresca
solo en cuanto la dirección guardada deja de responder (como mucho una vez por minuto,
ver `CLAUDE_LED_SCAN_COOLDOWN`). Así, un cambio de concesión DHCP se arregla solo y no
hay que tocar ninguna configuración.

Llamado desde un hook o desde un atajo de teclado —en cualquier sitio donde stdout no
sea un terminal—, un comando que actúa sobre el LED se desengancha de inmediato y hace
el trabajo de red en segundo plano: vuelve en unos milisegundos y siempre sale con
código 0, que es lo que permite colgarlo de un hook sin peligro. Escrito a mano en un
terminal se queda en primer plano e imprime la respuesta de la placa como antes.
`CLAUDE_LED_WAIT=1` fuerza la espera; `CLAUDE_LED_WAIT=0`, el desenganche.

Fíjate en que `claude-led.local` nunca se resuelve en la ruta del hook: una consulta
mDNS fallida cuesta 5 segundos enteros y era justo lo que reventaba el tiempo límite
del hook cuando la placa estaba apagada. La resolución de nombres solo ocurre dentro de
`find`. Si la dirección cacheada calla, `find` además recorre tu `/24` local —solo el
puerto 80, una sonda por host—, así que en una red donde eso no siente bien, fija mejor
la dirección con `CLAUDE_LED_URL`.

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

## Botón inalámbrico

Una segunda NodeMCU se convierte en un interruptor físico para el LED: sin ordenador de por
medio, solo Wi-Fi y cualquier cargador USB. No hace falta soldar nada: usa los botones y el LED
que la placa ya trae.

| Botón | Qué ocurre |
|---|---|
| `RESET` | la placa arranca, envía `GET /toggle` a la placa principal y parpadea una vez — ≈2,4 s |
| `FLASH` | conmutación inmediata mientras la placa siga alimentada, sin reiniciar — ≈50 ms |
| externo en `D7` | lo mismo que `FLASH`, pero con un botón con cable — ≈50 ms |

Tres parpadeos rápidos en lugar de uno largo significan que no alcanzó la placa principal:
no hay Wi-Fi, o la placa del LED está apagada.

### Botón externo

Un botón con cable es más cómodo que estirarse hasta la placa. Se suelda un pulsador
momentáneo, normalmente abierto, a cualquiera de los dos pares de pines:

```
D7  ──[ botón ]── GND     conmutación inmediata sin reiniciar — se lee igual que FLASH
RST ──[ botón ]── GND     lo mismo pasando por un reinicio, ≈2,4 s, aunque no haya Wi-Fi
```

No hace falta resistencia: los dos pines van a un pull-up interno y el antirrebote está en
el firmware (500 ms más la espera a que lo suelten). `D7` es `GPIO13` y no tiene ningún
papel en el arranque, así que el cable puede ser largo — al contrario que `D3`/`GPIO0` bajo
el botón `FLASH`, que puesto a masa al encender mete al ESP8266 en modo de programación. Un
interruptor con enclavamiento no sirve en ninguno de los dos: un `RST` cerrado mantiene la
placa en reset.

```bash
cp firmware/claude_led_button/secrets.h.example firmware/claude_led_button/secrets.h
$EDITOR firmware/claude_led_button/secrets.h   # la misma Wi-Fi, más la dirección de la placa principal

scripts/flash.sh button
```

`TARGET_HOST` indica a dónde va el toggle: una IP (`192.168.1.3`) no necesita resolución y es
lo más rápido; un nombre mDNS (`claude-led.local`) sobrevive a un cambio de DHCP. La dirección
se busca en este orden: caché en RTC → `TARGET_HOST` → `claude-led.local`. Ese último paso no
necesita ninguna biblioteca extra: el core de ESP8266 compila lwIP con
`LWIP_DNS_SUPPORT_MDNS_QUERIES`, así que `WiFi.hostByName()` envía la consulta multicast por
su cuenta.

De esos 2,4 segundos, 0,4 s son el arranque, 1,8 s el Wi-Fi con DHCP y 0,26 s la petición. Para
que no sea más, la placa guarda en memoria RTC la dirección, el canal Wi-Fi y el BSSID, de modo
que un `RESET` posterior se reconecta directamente al punto de acceso conocido en vez de escanear
todos los canales. La memoria RTC sobrevive a un `RESET` pero no a un corte de alimentación, así
que el primer arranque tras volver a enchufarla va por el camino largo de `TARGET_HOST`.

**Darle corriente cuenta como pulsación:** el toggle vive en `setup()`, así que enchufar la placa
a un cargador conmuta el LED una vez. Quita la llamada a `doToggle()` de `setup()` si solo
quieres el botón `FLASH`.

---

## Estructura

```
firmware/claude_led/claude_led.ino                firmware del ESP8266
firmware/claude_led/secrets.h.example             plantilla de configuración Wi-Fi (secrets.h está en gitignore)
firmware/claude_led_button/claude_led_button.ino  firmware del botón: RESET conmuta el LED
firmware/claude_led_button/secrets.h.example      plantilla del botón (Wi-Fi + dirección de la placa)
scripts/led.sh                                    CLI: on/off/toggle/pulse/blink/done/status
scripts/flash.sh                                  graba cualquiera de las dos placas con arduino-cli
scripts/install.sh                                instalador de los hooks de Claude Code
scripts/hotkey-xfce.sh                            atajo global Ctrl+T
docs/                                             imágenes y READMEs traducidos
```
