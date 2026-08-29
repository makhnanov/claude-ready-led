#!/usr/bin/env bash
# Управление светодиодом claude-led.
# Использование: led.sh on|off|pulse|done|status|blink [times] [ms]
#
# Адрес берётся из (по приоритету):
#   1) переменная окружения CLAUDE_LED_URL
#   2) ~/.claude-led.conf  (строки вида CLAUDE_LED_URL=http://192.168.1.50)
#   3) http://claude-led.local
#
# Скрипт никогда не возвращает ошибку и не висит дольше 2 секунд —
# чтобы его можно было безопасно повесить на хук Claude Code.

set -u
[ -f "$HOME/.claude-led.conf" ] && . "$HOME/.claude-led.conf"
URL="${CLAUDE_LED_URL:-http://claude-led.local}"
TOKEN="${CLAUDE_LED_TOKEN:-}"

cmd="${1:-status}"
case "$cmd" in
  on|off|pulse|done|status) path="/$cmd" ;;
  blink) path="/blink?times=${2:-3}&ms=${3:-200}" ;;
  *) echo "usage: $(basename "$0") on|off|pulse|blink|done|status" >&2; exit 2 ;;
esac

[ -n "$TOKEN" ] && path="$path$([[ $path == *\?* ]] && echo '&' || echo '?')token=$TOKEN"

curl -fsS --max-time 2 --connect-timeout 1 "$URL$path" 2>/dev/null || true
exit 0
