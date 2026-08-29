#!/usr/bin/env bash
#
# Системный хоткей XFCE: Ctrl+T переключает светодиод (погашен -> зажечь, иначе погасить).
#
#   hotkey-xfce.sh install     повесить Ctrl+T
#   hotkey-xfce.sh remove      снять
#   hotkey-xfce.sh status      показать текущее состояние
#
# Ключ и действие можно переопределить:
#   KEY='<Primary><Alt>l' ACTION=off ./hotkey-xfce.sh install
#
# ВНИМАНИЕ: XFCE перехватывает хоткей глобально. Ctrl+T перестанет открывать
# новую вкладку в браузере и новый терминал в приложениях, которые его слушают.

set -euo pipefail

KEY="${KEY:-<Primary>t}"
ACTION="${ACTION:-toggle}"
CHANNEL="xfce4-keyboard-shortcuts"
PROP="/commands/custom/$KEY"
LED_SH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/led.sh"
CMD="$LED_SH $ACTION"

command -v xfconf-query >/dev/null || { echo "Нужен xfconf-query (XFCE)." >&2; exit 1; }

case "${1:-status}" in
  install)
    existing="$(xfconf-query -c "$CHANNEL" -p "$PROP" 2>/dev/null || true)"
    if [ -n "$existing" ] && [ "$existing" != "$CMD" ]; then
      echo "Внимание: $KEY уже занят -> $existing" >&2
      echo "Перезаписываю. Прежнее значение вернёшь так:" >&2
      echo "  xfconf-query -c $CHANNEL -p '$PROP' -n -t string -s '$existing'" >&2
    fi
    xfconf-query -c "$CHANNEL" -p "$PROP" -n -t string -s "$CMD" 2>/dev/null \
      || xfconf-query -c "$CHANNEL" -p "$PROP" -t string -s "$CMD"
    echo "Готово: $KEY -> $CMD"
    echo "Работает сразу, во всей системе, и переживает перезагрузку."
    ;;
  remove)
    xfconf-query -c "$CHANNEL" -p "$PROP" -r 2>/dev/null && echo "Снято: $KEY" \
      || echo "$KEY и так не был назначен"
    ;;
  status)
    cur="$(xfconf-query -c "$CHANNEL" -p "$PROP" 2>/dev/null || true)"
    [ -n "$cur" ] && echo "$KEY -> $cur" || echo "$KEY не назначен"
    ;;
  *)
    echo "usage: $(basename "$0") install|remove|status" >&2; exit 2 ;;
esac
