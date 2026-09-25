# claude-ready-led

ESP8266 につないだ LED が、**Claude Code が動き出すと消え**、**仕事を終えると光ります**。
おまけにシステム全体で効く `Ctrl+T` で手動切り替えもできます。

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
[العربية](README.ar.md) ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
**日本語** ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="Claude Code の作業中に呼吸するように光る LED" width="440">
  <img src="ui.jpg" alt="ボード自身が配信する Web インターフェース" width="277">
</p>

ハードウェア: NodeMCU v3 (ESP8266) と好きな LED 1 個。ソフトウェア: HTTP サーバー内蔵の
ファームウェア、Claude Code のフック、そして XFCE のグローバルショートカット。

---

## フックのインストール

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

リポジトリを `~/.claude-ready-led` にクローンし、フックを `~/.claude/settings.json` に
書き込み（`settings.json.bak` にバックアップを残します）、デバイスのアドレスを
`~/.claude-led.conf` に保存して、ボードが応答するか確認します。何度実行しても安全です。
自分が書いた項目だけを置き換え、ほかのフックには触れません。

mDNS 名が解決できない場合は、アドレスを直接渡してください:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

そのあと Claude Code で `/hooks` を開く（設定が読み直されます）か、再起動してください。

---

## 配線

`D6` は `GPIO12` です。

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                   アノード カソード
                  （長い足）（短い足、
                             縁が平らな側）
```

抵抗は必須です。ESP8266 のピンが流せるのは、せいぜい 12 mA 程度です。

---

## ファームウェア

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # SSID とパスワード。2.4 GHz のみ対応

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

`secrets.h` は `.gitignore` に入っているので、Wi-Fi のパスワードがリポジトリに入ることは
ありません。git が追跡するのはプレースホルダー入りの `secrets.h.example` だけです。

割り当てられた IP を調べるには:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # 起動時にボードが IP を表示します
ping claude-led.local                                    # または mDNS 経由で
```

---

## HTTP API

| リクエスト | 動作 |
|---|---|
| `GET /on` | 点灯したまま |
| `GET /off` | 消灯 |
| `GET /toggle` | 切り替え: 消えていれば点灯、それ以外はすべて消灯 |
| `GET /pulse` | なめらかな呼吸 — Claude が作業中 |
| `GET /blink?times=3&ms=200` | 点滅 |
| `GET /done` | 3 回すばやく点滅してから点灯を維持 — 作業完了 |
| `GET /status` | JSON: モード、IP、電波強度、稼働時間 |
| `GET /` | Web インターフェース（下記参照） |

`secrets.h` に `API_TOKEN` を設定した場合は、すべてのリクエストに `?token=...` が必要です。

## CLI

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
scripts/led.sh find     # ボードをネットワーク上で探してアドレスを覚える
scripts/led.sh where    # いま使っているアドレスを表示する
```

アドレスは `CLAUDE_LED_URL`（環境変数または `~/.claude-led.conf`）から、なければ
`~/.cache/claude-led/ip` にキャッシュされた IP から決まります。このキャッシュを書くのが
`find` で、覚えていたアドレスが応答しなくなるとスクリプトが自分で取り直します（最大でも
1 分に 1 回。`CLAUDE_LED_SCAN_COOLDOWN` を参照）。つまり DHCP のリースが変わっても
勝手に直り、設定ファイルをいじる必要はありません。

フックやホットキーから呼ばれたとき、つまり stdout が端末でないところでは、LED を操作する
コマンドはすぐに切り離されてネットワーク処理をバックグラウンドで行います。数ミリ秒で戻り、
終了コードは必ず 0 —— フックに掛けても安全なのはこのためです。端末で手打ちした場合は
これまでどおり前面に留まり、ボードの応答を表示します。`CLAUDE_LED_WAIT=1` で必ず待ち、
`CLAUDE_LED_WAIT=0` で必ず切り離します。

なお、フックの経路で `claude-led.local` が解決されることはありません。失敗する mDNS
問い合わせはまるまる 5 秒かかり、ボードの電源が入っていないときにフックのタイムアウトを
起こしていたのがまさにこれでした。名前解決は `find` の中だけで行われます。キャッシュした
アドレスが黙ったままなら、`find` はローカルの `/24` も舐めます —— ポート 80 だけ、
1 ホストにつき 1 回。それが好ましくないネットワークでは、`CLAUDE_LED_URL` で
アドレスを固定してください。

---

## Web インターフェース

ブラウザで `http://claude-led.local/` を開いてください。mDNS はローカルネットワーク全体で
効くので、パソコンからでもスマホからでも構いません。ボード自身が、TOGGLE / ON / OFF /
PULSE / BLINK / DONE のボタン、現在のモードを映す「ランプ」、そして IP・電波強度・稼働
時間というライブ状態を載せたページを配信します。

**ボタンを押してもページは再読み込みされません。** クリックで `fetch("/on")` が飛び、
JSON の応答が返ってきた瞬間に状態とランプが描き直されます。さらにページは 2 秒ごとに
`/status` を自分で取りに行くので、LED が Claude Code のフックやスマホから切り替えられても
タブが気づきます。タブが非表示のあいだはポーリングが止まり（`document.hidden`）、
ボードを無駄に叩きません。

接続が切れるとランプは赤くなり、「ボードに接続できません」と表示されます。ページが黙って
固まることはありません。

### ページのコードはどこにあるか

別ファイルはありません。ボードにはそもそもファイルシステムがないからです。ページ全体が
`firmware/claude_led/claude_led.ino` の中の 1 本の C++ 文字列です:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

`PROGMEM` がコードの隣のフラッシュに置き、`send_P` が RAM にコピーせずそこから直接
送り出します。状態を HTML に埋め込まず `/status` で別に取りに行くのは、まさにこのため
です。ページが静的なままなら、組み立てるための RAM が要らないのです。この方式に移した
ことで 80 KB のうち約 650 バイトが空きました。マイコンでは無視できない量です。

`secrets.h` に `API_TOKEN` を設定している場合は、
`http://claude-led.local/?token=xxxx` のように開いてください。アドレスバーからトークンを
拾い、自分が出すすべてのリクエストに付けてくれます。

---

## Claude Code のフック

`install.sh` が `~/.claude/settings.json` に追加する内容:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — プロンプトを送って Claude が動き出した瞬間: LED が消えます。
- `Stop` — Claude が回答を終えた瞬間: 3 回点滅してから点灯し続けます。

つまり **点いている = 結果が受け取れる、消えている = まだ考え中** です。作業中に消えるの
ではなく呼吸させたいなら、フックの `off` を `pulse` に変えてください。

出力を `/dev/null` に捨てているのは意図的です。そうしないと `UserPromptSubmit` フックの
標準出力がモデルのコンテキストに入ってしまいます。

確認や無効化は Claude Code の `/hooks` から行えます。

---

## Ctrl+T — LED を切り替える

```bash
scripts/hotkey-xfce.sh install    # 割り当てる
scripts/hotkey-xfce.sh status     # 確認する
scripts/hotkey-xfce.sh remove     # 外す
```

`xfconf` を使って XFCE のグローバルショートカットを登録します:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

消えていれば点き、点いている・呼吸している場合は消えます。システム全体で有効で、すぐに
反映され、再起動しても残ります。

**注意:** XFCE はこのキーをグローバルに横取りするため、`Ctrl+T` がアプリケーションまで
届かなくなります。ブラウザで新しいタブが開かなくなる、ということです。困る場合は
`remove` で外し、別の組み合わせを割り当ててください:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

`toggle` 以外の動作も同じ方法で指定できます:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

使える動作: `toggle`（既定）、`on`、`off`、`pulse`、`done`。

### XFCE ではない場合

`hotkey-xfce.sh` が扱うのは XFCE だけです。ほかの環境での相当する方法:

| デスクトップ | 割り当て方 |
|---|---|
| GNOME | 設定 → キーボード → 独自のショートカット、コマンドは `led.sh toggle` |
| KDE | システム設定 → ショートカット → 独自のショートカット |
| i3 / sway | 設定ファイルに `bindsym Control+t exec /path/to/led.sh toggle` |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| 素の X11 | `~/.xbindkeysrc` に `"led.sh toggle"` と `Control + t` を書いて `xbindkeys` |

---

## 構成

```
firmware/claude_led/claude_led.ino     ESP8266 のファームウェア
firmware/claude_led/secrets.h.example  Wi-Fi 設定のひな形（secrets.h は gitignore 済み）
scripts/led.sh                         CLI: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     Claude Code フックのインストーラ
scripts/hotkey-xfce.sh                 グローバル Ctrl+T ショートカット
docs/                                  画像と各言語の README
```
