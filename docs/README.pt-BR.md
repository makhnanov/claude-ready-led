# claude-ready-led

Um LED ligado a um ESP8266 que **apaga quando o Claude Code começa a trabalhar** e
**acende quando ele termina**. E mais: um atalho global `Ctrl+T` para alterná-lo na mão.

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
**Português** ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="O LED respirando enquanto o Claude Code trabalha" width="440">
  <img src="ui.jpg" alt="Interface web servida pela própria placa" width="277">
</p>

Hardware: NodeMCU v3 (ESP8266) e qualquer LED. Software: firmware com servidor HTTP
embutido, hooks do Claude Code e um atalho global do XFCE.

---

## Instalar o hook

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

Clona o repositório em `~/.claude-ready-led`, escreve os hooks em
`~/.claude/settings.json` (guardando um backup em `settings.json.bak`), salva o
endereço do dispositivo em `~/.claude-led.conf` e verifica se a placa responde. Pode
rodar de novo sem medo: ele substitui apenas as próprias entradas e não mexe nos
outros hooks.

Se o nome mDNS não resolver, passe o endereço explicitamente:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

Depois abra `/hooks` no Claude Code (isso recarrega a configuração) ou reinicie-o.

---

## Ligação

`D6` é o `GPIO12`.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                   ânodo  cátodo
                (perna longa) (perna curta,
                               chanfro na borda)
```

O resistor é obrigatório: um pino do ESP8266 fornece no máximo uns 12 mA.

---

## Firmware

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID e senha, somente 2,4 GHz

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

O `secrets.h` está no `.gitignore`, então sua senha de Wi-Fi nunca chega ao
repositório. No git fica só o `secrets.h.example` com valores de exemplo.

Para descobrir o IP atribuído:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # a placa imprime o IP ao ligar
ping claude-led.local                                    # ou via mDNS
```

---

## API HTTP

| Requisição | Efeito |
|---|---|
| `GET /on` | luz fixa |
| `GET /off` | apagar |
| `GET /toggle` | alternar: apagado → acende; qualquer outro estado → apaga |
| `GET /pulse` | respiração suave — o Claude está trabalhando |
| `GET /blink?times=3&ms=200` | piscar |
| `GET /done` | 3 piscadas rápidas e fica aceso — trabalho concluído |
| `GET /status` | JSON: modo, IP, RSSI, tempo ligado |
| `GET /` | interface web (veja abaixo) |

Se `API_TOKEN` estiver definido em `secrets.h`, toda requisição precisa de `?token=...`.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
```

O endereço vem de `CLAUDE_LED_URL`, depois de `~/.claude-led.conf` e por fim
`http://claude-led.local`. O script sempre sai com código 0 e nunca trava por mais de
2 segundos — é por isso que dá para pendurá-lo num hook: uma placa desligada ou
inacessível não deixa o Claude Code lento nem o quebra.

---

## Interface web

Abra `http://claude-led.local/` no navegador — do computador ou do celular, já que o
mDNS funciona em toda a rede local. A placa serve uma página com os botões TOGGLE / ON /
OFF / PULSE / BLINK / DONE, uma "lâmpada" que espelha o modo atual e status ao vivo:
IP, força do sinal e tempo ligado.

**Os botões não recarregam a página.** Um clique dispara `fetch("/on")`, a resposta
JSON volta e redesenha na hora o status e a lâmpada. Além disso a página consulta
`/status` a cada 2 segundos, então se o LED for trocado por um hook do Claude Code ou
pelo celular, a aba percebe. A consulta para enquanto a aba está oculta
(`document.hidden`), para não martelar a placa à toa.

Se a conexão cair, a lâmpada fica vermelha e aparece "sem conexão com a placa" — a
página nunca simplesmente trava em silêncio.

### Onde fica o código da página

Não existe arquivo separado: a placa não tem sistema de arquivos. A página inteira é
uma única string C++ em `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

O `PROGMEM` a coloca na flash junto do código e o `send_P` a envia direto de lá, sem
copiar para a RAM. É exatamente por isso que o status é buscado à parte via `/status`
em vez de ser interpolado no HTML: a página continua estática e não precisa de RAM
para ser montada. Migrar para esse esquema liberou cerca de 650 bytes de 80 KB — num
microcontrolador isso pesa.

Se `API_TOKEN` estiver definido em `secrets.h`, abra a página como
`http://claude-led.local/?token=xxxx` — ela pega o token da barra de endereços e o
acrescenta a todas as próprias requisições.

---

## Hooks do Claude Code

O que o `install.sh` acrescenta em `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — você enviou um prompt e o Claude começou a trabalhar: o LED apaga.
- `Stop` — o Claude terminou de responder: três piscadas e luz fixa.

Ou seja: **aceso = seu resultado está pronto, apagado = ainda pensando**. Se preferir
que ele respire durante o trabalho em vez de apagar, troque `off` por `pulse` no hook.

A saída vai de propósito para `/dev/null`: caso contrário o stdout de um hook
`UserPromptSubmit` acaba no contexto do modelo.

Use `/hooks` no Claude Code para inspecionar ou desativar.

---

## Ctrl+T — alternar o LED

```bash
scripts/hotkey-xfce.sh install    # vincular
scripts/hotkey-xfce.sh status     # conferir
scripts/hotkey-xfce.sh remove     # desvincular
```

Registra um atalho global do XFCE via `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

Apagado acende; aceso ou respirando apaga. Funciona no sistema todo, vale na hora e
sobrevive a um reboot.

**Atenção:** o XFCE captura a tecla globalmente, então `Ctrl+T` deixa de chegar aos
aplicativos — seu navegador não vai mais abrir uma aba nova com ele. Se isso incomodar,
remova com `remove` e vincule outra combinação:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

Outra ação no lugar de `toggle` funciona igual:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

Ações disponíveis: `toggle` (padrão), `on`, `off`, `pulse`, `done`.

### Não usa XFCE?

O `hotkey-xfce.sh` cobre só o XFCE. Equivalentes em outros ambientes:

| Ambiente | Como vincular |
|---|---|
| GNOME | Configurações → Teclado → Atalhos personalizados, comando `led.sh toggle` |
| KDE | Configurações do sistema → Atalhos → Atalhos personalizados |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` na configuração |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| X11 puro | `xbindkeys` com `"led.sh toggle"` e `Control + t` no `~/.xbindkeysrc` |

---

## Estrutura

```
firmware/claude_led/claude_led.ino     firmware do ESP8266
firmware/claude_led/secrets.h.example  modelo de configuração Wi-Fi (secrets.h está no gitignore)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     instalador dos hooks do Claude Code
scripts/hotkey-xfce.sh                 atalho global Ctrl+T
docs/                                  imagens e READMEs traduzidos
```
