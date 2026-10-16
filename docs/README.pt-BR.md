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

Ou deixe o script fazer isso: ele instala o core, cria o `secrets.h` a partir do exemplo e
encontra a porta sozinho:

```bash
scripts/flash.sh              # placa principal
scripts/flash.sh button       # o botão sem fio (veja abaixo)
scripts/flash.sh button -m    # e depois abrir o monitor serial
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
scripts/led.sh find     # achar a placa na rede e guardar o endereço dela
scripts/led.sh where    # mostrar o endereço em uso no momento
```

O endereço vem de `CLAUDE_LED_URL` (ambiente ou `~/.claude-led.conf`) e, se não, do IP
em cache em `~/.cache/claude-led/ip`: quem o escreve é o `find`, e o script atualiza
sozinho assim que o endereço guardado para de responder (no máximo uma vez por minuto,
veja `CLAUDE_LED_SCAN_COOLDOWN`). Assim, troca de concessão DHCP se conserta sozinha —
não precisa mexer em configuração nenhuma.

Chamado de um hook ou de um atalho de teclado — em qualquer lugar onde stdout não seja
um terminal — um comando que age no LED se desprende na hora e faz o trabalho de rede
em segundo plano: volta em alguns milissegundos e sempre sai com código 0, que é o que
permite pendurá-lo num hook. Digitado à mão num terminal, ele fica em primeiro plano e
imprime a resposta da placa como antes. `CLAUDE_LED_WAIT=1` força a espera;
`CLAUDE_LED_WAIT=0`, o desprendimento.

Repare que `claude-led.local` nunca é resolvido no caminho do hook: uma consulta mDNS
que falha custa 5 segundos inteiros, e era justamente isso que estourava o tempo limite
do hook quando a placa estava desligada. A resolução de nomes só acontece dentro do
`find`. Se o endereço em cache emudecer, o `find` ainda varre a sua `/24` local — só a
porta 80, uma sondagem por host —, então numa rede em que isso não cai bem, fixe o
endereço com `CLAUDE_LED_URL`.

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

## Botão sem fio

Uma segunda NodeMCU se torna um interruptor físico para o LED: sem computador no caminho,
só Wi-Fi e qualquer carregador USB. Nada precisa ser soldado: ela usa os botões e o LED que a
placa já tem.

| Botão | O que acontece |
|---|---|
| `RESET` | a placa inicia, envia `GET /toggle` para a placa principal e pisca uma vez — ≈2,4 s |
| `FLASH` | alternância imediata enquanto a placa estiver energizada, sem reiniciar — ≈50 ms |
| externo no `D7` | o mesmo que `FLASH`, mas com um botão com fio — ≈50 ms |

Três piscadas rápidas em vez de uma longa significam que não alcançou a placa principal:
sem Wi-Fi, ou a placa do LED está desligada.

### Botão externo

Um botão com fio é mais cômodo do que alcançar a placa. Solda-se um botão momentâneo,
normalmente aberto, em um dos dois pares de pinos:

```
D7  ──[ botão ]── GND     alternância imediata sem reiniciar — lido como o FLASH
RST ──[ botão ]── GND     o mesmo passando por um reinício, ≈2,4 s, até sem Wi-Fi
```

Resistor não é preciso: os dois pinos têm pull-up interno e o antirrepique está no firmware
(500 ms mais a espera pela soltura). `D7` é o `GPIO13` e não tem papel no boot, então o fio
pode ser longo — ao contrário do `D3`/`GPIO0` sob o botão `FLASH`, que, puxado ao terra na
energização, joga o ESP8266 no modo de gravação. Chave com trava não serve em nenhum dos
dois: um `RST` fechado mantém a placa em reset.

```bash
cp firmware/claude_led_button/secrets.h.example firmware/claude_led_button/secrets.h
$EDITOR firmware/claude_led_button/secrets.h   # a mesma Wi-Fi, mais o endereço da placa principal

scripts/flash.sh button
```

`TARGET_HOST` diz para onde vai o toggle: um IP (`192.168.1.3`) não precisa de resolução e é
o mais rápido; um nome mDNS (`claude-led.local`) sobrevive a uma troca de DHCP. O endereço é
procurado nesta ordem: cache na RTC → `TARGET_HOST` → `claude-led.local`. Esse último passo
não precisa de biblioteca extra — o core do ESP8266 compila a lwIP com
`LWIP_DNS_SUPPORT_MDNS_QUERIES`, então `WiFi.hostByName()` envia a consulta multicast por
conta própria.

Desses 2,4 segundos, 0,4 s é o boot, 1,8 s é o Wi-Fi com DHCP e 0,26 s é a requisição. Para
não demorar mais, a placa guarda na memória RTC o endereço, o canal Wi-Fi e o BSSID, de modo
que um `RESET` seguinte se reconecta direto ao ponto de acesso conhecido em vez de varrer
todos os canais. A memória RTC sobrevive a um `RESET`, mas não à falta de energia — por isso
o primeiro boot depois de religar vai pelo caminho longo, via `TARGET_HOST`.

**Ligar na energia conta como um aperto:** o toggle vive no `setup()`, então plugar a placa num
carregador alterna o LED uma vez. Remova a chamada `doToggle()` do `setup()` se quiser apenas
o botão `FLASH`.

---

## Estrutura

```
firmware/claude_led/claude_led.ino                firmware do ESP8266
firmware/claude_led/secrets.h.example             modelo de configuração Wi-Fi (secrets.h está no gitignore)
firmware/claude_led_button/claude_led_button.ino  firmware do botão: RESET alterna o LED
firmware/claude_led_button/secrets.h.example      modelo de configuração do botão (Wi-Fi + endereço da placa)
scripts/led.sh                                    CLI: on/off/toggle/pulse/blink/done/status
scripts/flash.sh                                  grava qualquer das duas placas via arduino-cli
scripts/install.sh                                instalador dos hooks do Claude Code
scripts/hotkey-xfce.sh                            atalho global Ctrl+T
docs/                                             imagens e READMEs traduzidos
```
