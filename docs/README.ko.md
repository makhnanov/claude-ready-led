# claude-ready-led

ESP8266에 물린 LED가 **Claude Code가 일을 시작하면 꺼지고**, **일을 끝내면 켜집니다**.
여기에 시스템 전역에서 먹히는 `Ctrl+T` 단축키로 직접 껐다 켤 수도 있습니다.

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
**한국어**

<p align="center">
  <img src="demo.gif" alt="Claude Code가 작업하는 동안 숨 쉬듯 빛나는 LED" width="440">
  <img src="ui.jpg" alt="보드가 직접 제공하는 웹 인터페이스" width="277">
</p>

하드웨어: NodeMCU v3 (ESP8266)와 아무 LED 하나. 소프트웨어: HTTP 서버가 내장된 펌웨어,
Claude Code 훅, 그리고 XFCE 전역 단축키.

---

## 훅 설치

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

저장소를 `~/.claude-ready-led`에 클론하고, 훅을 `~/.claude/settings.json`에 기록하며
(`settings.json.bak`에 백업을 남깁니다), 장치 주소를 `~/.claude-led.conf`에 저장한 뒤
보드가 응답하는지 확인합니다. 다시 실행해도 안전합니다. 자기가 넣은 항목만 교체하고
다른 훅은 건드리지 않습니다.

mDNS 이름이 풀리지 않으면 주소를 직접 넘기세요:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

설치 후에는 Claude Code에서 `/hooks`를 열거나(설정을 다시 읽습니다) 재시작하세요.

---

## 배선

`D6`가 곧 `GPIO12`입니다.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                   애노드  캐소드
                 (긴 다리) (짧은 다리,
                            테두리가 평평한 쪽)
```

저항은 필수입니다. ESP8266 핀이 낼 수 있는 전류는 많아야 12 mA 정도입니다.

---

## 펌웨어

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID와 비밀번호, 2.4 GHz만 지원

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h`는 `.gitignore`에 들어 있어서 Wi-Fi 비밀번호가 저장소로 새어 나가지 않습니다.
git이 추적하는 건 자리표시자만 들어 있는 `secrets.h.example`뿐입니다.

할당된 IP 확인:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # 부팅할 때 보드가 IP를 출력합니다
ping claude-led.local                                    # 또는 mDNS로
```

---

## HTTP API

| 요청 | 동작 |
|---|---|
| `GET /on` | 계속 켜짐 |
| `GET /off` | 끄기 |
| `GET /toggle` | 토글: 꺼져 있으면 켜고, 나머지 상태는 모두 끄기 |
| `GET /pulse` | 부드러운 호흡 — Claude가 작업 중 |
| `GET /blink?times=3&ms=200` | 깜빡임 |
| `GET /done` | 빠르게 3번 깜빡인 뒤 계속 켜짐 — 작업 완료 |
| `GET /status` | JSON: 모드, IP, 신호 세기, 가동 시간 |
| `GET /` | 웹 인터페이스(아래 참고) |

`secrets.h`에 `API_TOKEN`을 설정했다면 모든 요청에 `?token=...`이 필요합니다.

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
scripts/led.sh find     # 네트워크에서 보드를 찾아 주소를 기억
scripts/led.sh where    # 지금 쓰고 있는 주소를 출력
```

주소는 `CLAUDE_LED_URL`(환경 변수 또는 `~/.claude-led.conf`)에서, 없으면
`~/.cache/claude-led/ip`에 캐시된 IP에서 결정됩니다. 이 캐시를 쓰는 것이 `find`이고,
기억해 둔 주소가 응답을 멈추면 스크립트가 알아서 다시 찾습니다(많아야 1분에 한 번,
`CLAUDE_LED_SCAN_COOLDOWN` 참고). 그래서 DHCP 임대가 바뀌어도 저절로 고쳐지고, 설정을
손댈 필요가 없습니다.

훅이나 단축키에서 부를 때, 즉 stdout이 터미널이 아닌 곳에서는 LED를 조작하는 명령이
곧바로 떨어져 나가 네트워크 작업을 백그라운드에서 합니다. 몇 밀리초 만에 돌아오고 언제나
0으로 종료하니, 훅에 걸어도 안전한 이유가 바로 이것입니다. 터미널에서 직접 입력하면
예전처럼 앞에 남아 보드의 응답을 찍어 줍니다. `CLAUDE_LED_WAIT=1`은 항상 기다리게,
`CLAUDE_LED_WAIT=0`은 항상 떨어져 나가게 합니다.

훅 경로에서는 `claude-led.local`을 아예 해석하지 않습니다. 실패하는 mDNS 질의는 꼬박
5초가 걸리고, 보드가 꺼져 있을 때 훅 제한 시간을 터뜨리던 것이 바로 이것이었습니다. 이름
해석은 `find` 안에서만 일어납니다. 캐시된 주소가 잠잠하면 `find`는 로컬 `/24`도 훑습니다
— 포트 80만, 호스트당 한 번씩. 그런 동작이 달갑지 않은 네트워크라면 `CLAUDE_LED_URL`로
주소를 고정하세요.

---

## 웹 인터페이스

브라우저에서 `http://claude-led.local/`을 열어 보세요. mDNS는 로컬 네트워크 전체에서
동작하니 컴퓨터든 휴대폰이든 상관없습니다. 보드가 직접 TOGGLE / ON / OFF / PULSE /
BLINK / DONE 버튼과, 현재 모드를 그대로 비추는 "전구", 그리고 IP·신호 세기·가동 시간
같은 실시간 상태가 담긴 페이지를 내려줍니다.

**버튼을 눌러도 페이지가 새로고침되지 않습니다.** 클릭하면 `fetch("/on")`이 나가고,
JSON 응답이 돌아오는 즉시 상태와 전구가 다시 그려집니다. 게다가 페이지는 2초마다
스스로 `/status`를 가져오기 때문에, LED를 Claude Code 훅이나 휴대폰에서 바꿔도 탭이
알아챕니다. 탭이 가려져 있는 동안에는 폴링이 멈춰서(`document.hidden`) 보드를 괜히
괴롭히지 않습니다.

연결이 끊기면 전구가 빨갛게 변하고 "보드와 연결되지 않음"이 뜹니다. 페이지가 아무 말
없이 멈춰 있는 일은 없습니다.

### 페이지 코드는 어디에 있나

별도 파일은 없습니다. 보드에는 애초에 파일 시스템이 없으니까요. 페이지 전체가
`firmware/claude_led/claude_led.ino` 안의 C++ 문자열 하나입니다:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM`이 코드 옆 플래시에 넣어 두고, `send_P`가 RAM으로 복사하지 않은 채 거기서
바로 내보냅니다. 상태를 HTML에 끼워 넣지 않고 `/status`로 따로 받아 오는 이유가 정확히
이것입니다. 페이지가 정적으로 남으면 조립할 RAM이 필요 없으니까요. 이 방식으로 바꾸면서
80 KB 중 약 650바이트가 남았습니다. 마이크로컨트롤러에서는 무시할 수 없는 양입니다.

`secrets.h`에 `API_TOKEN`을 설정했다면 페이지를
`http://claude-led.local/?token=xxxx` 형태로 여세요. 주소창에서 토큰을 집어다가 자기가
보내는 모든 요청에 붙여 줍니다.

---

## Claude Code 훅

`install.sh`가 `~/.claude/settings.json`에 추가하는 내용:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — 프롬프트를 보내 Claude가 일을 시작한 순간: LED가 꺼집니다.
- `Stop` — Claude가 답변을 마친 순간: 세 번 깜빡인 뒤 계속 켜집니다.

즉 **켜져 있으면 결과를 가져가도 좋다는 뜻이고, 꺼져 있으면 아직 생각 중**입니다. 작업
중에 꺼지는 대신 숨 쉬게 하고 싶다면 훅에서 `off`를 `pulse`로 바꾸면 됩니다.

출력을 `/dev/null`로 보내는 건 일부러 그런 것입니다. 그러지 않으면
`UserPromptSubmit` 훅의 표준 출력이 모델의 컨텍스트로 들어갑니다.

살펴보거나 끄려면 Claude Code에서 `/hooks`를 쓰세요.

---

## Ctrl+T — LED 토글

```bash
scripts/hotkey-xfce.sh install    # 등록
scripts/hotkey-xfce.sh status     # 확인
scripts/hotkey-xfce.sh remove     # 해제
```

`xfconf`를 통해 XFCE 전역 단축키를 등록합니다:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

꺼져 있으면 켜지고, 켜져 있거나 숨 쉬는 중이면 꺼집니다. 시스템 전역에서 동작하고,
즉시 적용되며, 재부팅해도 남아 있습니다.

**주의:** XFCE가 이 키를 전역으로 가로채기 때문에 `Ctrl+T`가 애플리케이션까지 가지
않습니다. 브라우저에서 새 탭이 열리지 않게 된다는 뜻입니다. 거슬리면 `remove`로 풀고
다른 조합을 쓰세요:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

`toggle` 대신 다른 동작을 지정하는 것도 같은 방식입니다:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

가능한 동작: `toggle`(기본값), `on`, `off`, `pulse`, `done`.

### XFCE가 아니라면

`hotkey-xfce.sh`는 XFCE만 다룹니다. 다른 환경에서의 대응 방법:

| 데스크톱 | 등록 방법 |
|---|---|
| GNOME | 설정 → 키보드 → 사용자 지정 바로 가기, 명령은 `led.sh toggle` |
| KDE | 시스템 설정 → 단축키 → 사용자 지정 단축키 |
| i3 / sway | 설정 파일에 `bindsym Control+t exec /path/to/led.sh toggle` |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| 순수 X11 | `~/.xbindkeysrc`에 `"led.sh toggle"`과 `Control + t`를 넣고 `xbindkeys` |

---

## 구성

```
firmware/claude_led/claude_led.ino     ESP8266 펌웨어
firmware/claude_led/secrets.h.example  Wi-Fi 설정 템플릿 (secrets.h는 gitignore 처리됨)
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     Claude Code 훅 설치 스크립트
scripts/hotkey-xfce.sh                 전역 Ctrl+T 단축키
docs/                                  이미지와 번역된 README
```
