# claude-ready-led

Eine LED an einem ESP8266, die **ausgeht, sobald Claude Code zu arbeiten beginnt**, und
**angeht, wenn er fertig ist**. Dazu ein systemweites `Ctrl+T`, um sie von Hand
umzuschalten.

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
**Deutsch** ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="Die LED atmet, während Claude Code arbeitet" width="440">
  <img src="ui.jpg" alt="Weboberfläche, die das Board selbst ausliefert" width="277">
</p>

Hardware: NodeMCU v3 (ESP8266) und eine beliebige LED. Software: Firmware mit
eingebautem HTTP-Server, Claude-Code-Hooks und ein globales XFCE-Tastenkürzel.

---

## Hook installieren

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Klont das Repository nach `~/.claude-ready-led`, schreibt die Hooks in
`~/.claude/settings.json` (mit einer Sicherung in `settings.json.bak`), legt die
Geräteadresse in `~/.claude-led.conf` ab und prüft, ob das Board antwortet. Ein
erneuter Lauf ist unbedenklich: Das Skript ersetzt nur seine eigenen Einträge und
lässt fremde Hooks unberührt.

Falls der mDNS-Name nicht auflöst, die Adresse direkt übergeben:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

Danach in Claude Code `/hooks` öffnen (das lädt die Konfiguration neu) oder ihn neu
starten.

---

## Verdrahtung

`D6` ist `GPIO12`.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                  Anode  Kathode
              (langes Bein) (kurzes Bein,
                             abgeflachte Kante)
```

Der Widerstand ist Pflicht: Ein ESP8266-Pin liefert höchstens etwa 12 mA.

---

## Firmware

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID und Passwort, nur 2,4 GHz

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` steht in `.gitignore`, dein WLAN-Passwort landet also nie im Repository.
Versioniert ist nur `secrets.h.example` mit Platzhaltern.

Die zugewiesene IP herausfinden:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # das Board druckt seine IP beim Start
ping claude-led.local                                    # oder per mDNS
```

---

## HTTP-API

| Anfrage | Wirkung |
|---|---|
| `GET /on` | Dauerlicht |
| `GET /off` | ausschalten |
| `GET /toggle` | umschalten: aus → an, jeder andere Zustand → aus |
| `GET /pulse` | sanftes Atmen — Claude arbeitet |
| `GET /blink?times=3&ms=200` | blinken |
| `GET /done` | 3 kurze Blinker, dann Dauerlicht — Arbeit erledigt |
| `GET /status` | JSON: Modus, IP, RSSI, Laufzeit |
| `GET /` | Weboberfläche (siehe unten) |

Ist in `secrets.h` ein `API_TOKEN` gesetzt, braucht jede Anfrage `?token=...`.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
scripts/led.sh find     # Board im Netz finden und seine Adresse merken
scripts/led.sh where    # die gerade verwendete Adresse ausgeben
```

Die Adresse kommt aus `CLAUDE_LED_URL` (Umgebung oder `~/.claude-led.conf`), sonst aus
der zwischengespeicherten IP in `~/.cache/claude-led/ip` — die schreibt `find`, und das
Skript frischt sie von selbst auf, sobald die gemerkte Adresse nicht mehr antwortet
(höchstens einmal pro Minute, siehe `CLAUDE_LED_SCAN_COOLDOWN`). Ein Wechsel der
DHCP-Lease repariert sich also selbst; du musst keine Konfiguration anfassen.

Aus einem Hook oder von einer Tastenkombination aufgerufen — überall dort, wo stdout
kein Terminal ist — klinkt sich ein Befehl, der die LED schaltet, sofort aus und
erledigt die Netzarbeit im Hintergrund: Er kehrt nach wenigen Millisekunden zurück und
beendet sich immer mit 0. Genau deshalb darf er an einem Hook hängen. Von Hand im
Terminal getippt bleibt er im Vordergrund und gibt die Antwort des Boards aus wie
bisher. `CLAUDE_LED_WAIT=1` erzwingt das Warten, `CLAUDE_LED_WAIT=0` das Ausklinken.

Auf dem Hook-Pfad wird `claude-led.local` nie aufgelöst: Eine fehlschlagende
mDNS-Abfrage kostet volle 5 Sekunden und hat früher genau dann den Hook-Timeout
gesprengt, wenn das Board aus war. Namensauflösung passiert nur innerhalb von `find`.
Antwortet die gemerkte Adresse nicht mehr, tastet `find` zusätzlich dein lokales `/24`
ab — nur Port 80, eine Anfrage pro Host. In einem Netz, in dem das unerwünscht ist,
lege die Adresse lieber mit `CLAUDE_LED_URL` fest.

---

## Weboberfläche

Öffne `http://claude-led.local/` im Browser — vom Rechner oder vom Handy, denn mDNS
funktioniert im ganzen lokalen Netz. Das Board liefert eine Seite mit den Schaltflächen
TOGGLE / ON / OFF / PULSE / BLINK / DONE, einer „Lampe“, die den aktuellen Modus
spiegelt, und Live-Status: IP, Signalstärke, Laufzeit.

**Die Schaltflächen laden die Seite nicht neu.** Ein Klick schickt `fetch("/on")`, die
JSON-Antwort kommt zurück und zeichnet Status und Lampe sofort neu. Zusätzlich fragt
die Seite alle 2 Sekunden `/status` ab: Wird die LED von einem Claude-Code-Hook oder
vom Handy umgeschaltet, bekommt der Tab das mit. Solange der Tab verborgen ist, ruht
die Abfrage (`document.hidden`), damit das Board nicht grundlos belastet wird.

Bricht die Verbindung ab, wird die Lampe rot und „keine Verbindung zum Board“
erscheint — die Seite hängt nie stumm fest.

### Wo der Seitencode liegt

Es gibt keine eigene Datei: Das Board hat gar kein Dateisystem. Die ganze Seite ist ein
einziger C++-String in `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` legt sie neben dem Code in den Flash, `send_P` schickt sie direkt von dort,
ohne Kopie im RAM. Genau darum wird der Status separat über `/status` geholt statt ins
HTML eingesetzt: Die Seite bleibt statisch und braucht kein RAM zum Zusammenbauen. Der
Umstieg hat rund 650 Byte von 80 KB freigemacht — auf einem Mikrocontroller eine
spürbare Größe.

Ist in `secrets.h` ein `API_TOKEN` gesetzt, öffne die Seite als
`http://claude-led.local/?token=xxxx` — sie liest das Token aus der Adresszeile und
hängt es an alle eigenen Anfragen an.

---

## Claude-Code-Hooks

Was `install.sh` in `~/.claude/settings.json` einträgt:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — du hast einen Prompt abgeschickt und Claude legt los: Die LED geht aus.
- `Stop` — Claude ist mit der Antwort fertig: dreimal blinken, dann Dauerlicht.

Also: **leuchtet = dein Ergebnis ist da, dunkel = er denkt noch**. Wenn sie während der
Arbeit lieber atmen statt ausgehen soll, tausche im Hook `off` gegen `pulse`.

Die Ausgabe geht absichtlich nach `/dev/null`: Sonst landet die Standardausgabe eines
`UserPromptSubmit`-Hooks im Kontext des Modells.

Mit `/hooks` in Claude Code lassen sie sich ansehen oder abschalten.

---

## Ctrl+T — LED umschalten

```bash
scripts/hotkey-xfce.sh install    # belegen
scripts/hotkey-xfce.sh status     # prüfen
scripts/hotkey-xfce.sh remove     # lösen
```

Registriert über `xfconf` ein globales XFCE-Kürzel:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

Dunkel geht an, leuchtend oder atmend geht aus. Gilt systemweit, wirkt sofort und
übersteht einen Neustart.

**Achtung:** XFCE greift die Taste global ab, `Ctrl+T` erreicht die Anwendungen also
nicht mehr — dein Browser öffnet damit keinen neuen Tab mehr. Falls das stört, mit
`remove` lösen und eine andere Kombination belegen:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

Eine andere Aktion statt `toggle` geht genauso:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

Verfügbare Aktionen: `toggle` (Standard), `on`, `off`, `pulse`, `done`.

### Kein XFCE?

`hotkey-xfce.sh` deckt nur XFCE ab. Entsprechungen anderswo:

| Desktop | So wird belegt |
|---|---|
| GNOME | Einstellungen → Tastatur → Eigene Tastenkürzel, Befehl `led.sh toggle` |
| KDE | Systemeinstellungen → Kurzbefehle → Eigene Kurzbefehle |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` in der Konfiguration |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| nacktes X11 | `xbindkeys` mit `"led.sh toggle"` und `Control + t` in `~/.xbindkeysrc` |

---

## Aufbau

```
firmware/claude_led/claude_led.ino     ESP8266-Firmware
firmware/claude_led/secrets.h.example  Vorlage für die WLAN-Konfiguration (secrets.h ist gitignored)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     Installer für die Claude-Code-Hooks
scripts/hotkey-xfce.sh                 globales Ctrl+T
docs/                                  Bilder und übersetzte READMEs
```
