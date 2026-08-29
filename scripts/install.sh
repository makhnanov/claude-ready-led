#!/usr/bin/env bash
#
# Установщик claude-ready-led: прописывает хуки Claude Code, которые зажигают
# светодиод на ESP8266, когда Claude заканчивает работать.
#
#   curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
#
# Переменные окружения:
#   CLAUDE_LED_URL  адрес устройства (по умолчанию http://claude-led.local)
#   INSTALL_DIR     куда клонировать репозиторий (по умолчанию ~/.claude-ready-led)

set -euo pipefail

REPO="https://github.com/makhnanov/claude-ready-led.git"
INSTALL_DIR="${INSTALL_DIR:-$HOME/.claude-ready-led}"
LED_URL="${CLAUDE_LED_URL:-http://claude-led.local}"
SETTINGS="$HOME/.claude/settings.json"
CONF="$HOME/.claude-led.conf"

say() { printf '\033[36m==>\033[0m %s\n' "$*"; }
die() { printf '\033[31mОшибка:\033[0m %s\n' "$*" >&2; exit 1; }

command -v python3 >/dev/null || die "нужен python3"

# --- 1. Найти или скачать led.sh ---------------------------------------------
self_dir="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || true)"
if [ -n "$self_dir" ] && [ -x "$self_dir/led.sh" ]; then
  LED_SH="$self_dir/led.sh"                      # запущено из клона репозитория
  say "использую локальный чекаут: $self_dir"
else
  command -v git >/dev/null || die "нужен git"
  if [ -d "$INSTALL_DIR/.git" ]; then
    say "обновляю $INSTALL_DIR"
    git -C "$INSTALL_DIR" pull --ff-only --quiet
  else
    say "клонирую в $INSTALL_DIR"
    git clone --quiet --depth 1 "$REPO" "$INSTALL_DIR"
  fi
  LED_SH="$INSTALL_DIR/scripts/led.sh"
fi
chmod +x "$LED_SH"
[ -x "$LED_SH" ] || die "не нашёл led.sh"

# --- 2. Запомнить адрес устройства -------------------------------------------
if [ -f "$CONF" ] && grep -q '^CLAUDE_LED_URL=' "$CONF"; then
  say "адрес уже записан в $CONF, не трогаю"
else
  printf 'CLAUDE_LED_URL=%s\n' "$LED_URL" >> "$CONF"
  say "адрес $LED_URL записан в $CONF"
fi

# --- 3. Вписать хуки в settings.json -----------------------------------------
mkdir -p "$HOME/.claude"
[ -f "$SETTINGS" ] || echo '{}' > "$SETTINGS"
cp "$SETTINGS" "$SETTINGS.bak"

LED_SH="$LED_SH" python3 <<'PY'
import json, os, pathlib

settings = pathlib.Path(os.path.expanduser("~/.claude/settings.json"))
led      = os.environ["LED_SH"]
data     = json.loads(settings.read_text() or "{}")
hooks    = data.setdefault("hooks", {})

# stdout хука UserPromptSubmit попадает в контекст модели — глушим
WANTED = {"UserPromptSubmit": "pulse", "Stop": "done"}

for event, action in WANTED.items():
    command = f"{led} {action} >/dev/null 2>&1"
    groups  = hooks.setdefault(event, [])
    # убрать прежние записи claude-ready-led, чтобы установка была идемпотентной
    for group in groups:
        group["hooks"] = [h for h in group.get("hooks", [])
                          if "led.sh" not in str(h.get("command", ""))]
    groups[:] = [g for g in groups if g.get("hooks")]
    groups.append({"hooks": [{"type": "command", "command": command, "timeout": 5}]})

settings.write_text(json.dumps(data, indent=2) + "\n")
print(f"  {'/'.join(WANTED)} -> {led}")
PY

python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$SETTINGS" \
  || die "settings.json получился битым, откати: mv $SETTINGS.bak $SETTINGS"

say "хуки прописаны в $SETTINGS (бэкап: $SETTINGS.bak)"

# --- 4. Проверить связь с устройством ----------------------------------------
if "$LED_SH" status | grep -q '"ip"'; then
  say "устройство отвечает:"
  "$LED_SH" status | sed 's/^/    /'
else
  printf '\033[33mВнимание:\033[0m устройство по адресу %s не отвечает.\n' "$LED_URL"
  printf '  Прошей ESP8266 (см. README) или поправь CLAUDE_LED_URL в %s\n' "$CONF"
fi

say "готово. Открой /hooks в Claude Code или перезапусти его, чтобы хуки подхватились."
