#!/usr/bin/env bash
# Прошивка плат claude-led через arduino-cli.
#
# Использование:
#   flash.sh              — прошить основную плату со светодиодом
#   flash.sh led          — то же самое явно
#   flash.sh button       — прошить плату-кнопку
#   flash.sh both         — обе по очереди, с паузой на пересадку кабеля
#
#   -p /dev/ttyUSB1       — порт вручную (по умолчанию ищется сам)
#   -m                    — после прошивки открыть монитор serial
#   -c                    — только собрать, не заливать
#
# Скрипт сам доустановит ядро esp8266 и создаст secrets.h из примера,
# если его ещё нет — но заполнять креды придётся руками.

set -eu

FQBN="esp8266:esp8266:nodemcuv2"
BAUD=115200
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

target=""
port=""
monitor=0
compile_only=0

while [ $# -gt 0 ]; do
  case "$1" in
    led|main)   target="claude_led";        shift ;;
    button|btn) target="claude_led_button"; shift ;;
    both)       target="both";              shift ;;
    -p|--port)  port="${2:-}"; shift 2 ;;
    -m|--monitor)      monitor=1;      shift ;;
    -c|--compile-only) compile_only=1; shift ;;
    -h|--help) awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "$0"
               exit 0 ;;
    *) echo "неизвестный аргумент: $1" >&2
       echo "usage: $(basename "$0") [led|button|both] [-p ПОРТ] [-m] [-c]" >&2
       exit 2 ;;
  esac
done
[ -z "$target" ] && target="claude_led"

need_cli() {
  command -v arduino-cli >/dev/null 2>&1 && return 0
  echo "нет arduino-cli. Поставь его так:" >&2
  echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh" >&2
  exit 1
}

# Ядро esp8266 ставится один раз; проверка дешевле, чем падение компиляции.
need_core() {
  arduino-cli core list 2>/dev/null | grep -q '^esp8266:esp8266' && return 0
  echo "== ставлю ядро esp8266 (это один раз и небыстро)"
  arduino-cli core update-index
  arduino-cli core install esp8266:esp8266
}

# Платы на CH340/CP2102/FTDI поднимаются как ttyUSB*. ttyACM* — это ардуины,
# их не трогаем. Если подключено больше одной платы, выбрать должен человек.
find_port() {
  local found
  found=$(ls /dev/ttyUSB* 2>/dev/null || true)
  if [ -z "$found" ]; then
    echo "плата не найдена: нет ни одного /dev/ttyUSB*" >&2
    echo "проверь кабель (нужен data, не только зарядный) и права: id -nG | grep dialout" >&2
    exit 1
  fi
  if [ "$(echo "$found" | wc -l)" -gt 1 ]; then
    echo "подключено несколько плат:" >&2
    echo "$found" | sed 's/^/  /' >&2
    echo "укажи нужную: $(basename "$0") $1 -p /dev/ttyUSB0" >&2
    exit 1
  fi
  echo "$found"
}

# secrets.h в .gitignore, поэтому у свежего клона его нет.
need_secrets() {
  local dir="$ROOT/firmware/$1"
  [ -f "$dir/secrets.h" ] && return 0
  cp "$dir/secrets.h.example" "$dir/secrets.h"
  echo "создан $dir/secrets.h из примера." >&2
  echo "впиши туда SSID и пароль (2.4 GHz) и запусти снова." >&2
  exit 1
}

flash_one() {
  local sketch="$1" p
  need_secrets "$sketch"

  if [ "$compile_only" = 1 ]; then
    echo "== собираю $sketch"
    arduino-cli compile --fqbn "$FQBN" "$ROOT/firmware/$sketch"
    echo "== собрано, заливать не просили"
    return 0
  fi

  p="${port:-$(find_port "$sketch")}"
  echo "== прошиваю $sketch -> $p"
  arduino-cli compile --fqbn "$FQBN" --upload -p "$p" "$ROOT/firmware/$sketch"
  echo "== готово: $sketch"

  if [ "$monitor" = 1 ]; then
    echo "== монитор $p (Ctrl+C чтобы выйти)"
    arduino-cli monitor -p "$p" -c baudrate=$BAUD
  fi
}

need_cli
need_core

if [ "$target" = "both" ]; then
  flash_one claude_led
  echo
  read -r -p "переподключи вторую плату и нажми Enter... " _
  port=""           # порт мог поменяться
  flash_one claude_led_button
else
  flash_one "$target"
fi
