#!/usr/bin/env bash
# Управление светодиодом claude-led.
#
# Использование:
#   led.sh on|off|toggle|pulse|done|status|blink [times] [ms]
#   led.sh find    — найти плату в сети и запомнить её адрес
#   led.sh where   — показать текущий адрес и откуда он взялся
#
# Адрес берётся по приоритету:
#   1) CLAUDE_LED_URL — из окружения или из ~/.claude-led.conf
#   2) кэш последнего найденного IP (~/.cache/claude-led/ip)
#   3) поиск в сети: сначала mDNS, потом опрос локальных /24
#
# Два свойства, ради которых всё это написано именно так:
#   * команда-действие не блокирует вызывающего — сетевая работа уходит в фон,
#     скрипт возвращается за миллисекунды и всегда с кодом 0;
#   * имя claude-led.local никогда не резолвится в горячем пути: неудачный
#     mDNS-запрос стоит 5 секунд, и раньше именно он ронял хук по таймауту.

set -u

CONF="$HOME/.claude-led.conf"
[ -f "$CONF" ] && . "$CONF"

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/claude-led"
IP_CACHE="$CACHE_DIR/ip"
SCAN_STAMP="$CACHE_DIR/last-scan"

HOST_NAME="${CLAUDE_LED_HOST:-claude-led}"
TOKEN="${CLAUDE_LED_TOKEN:-}"
# как часто фоновая перепроверка имеет право сканировать сеть, секунд
COOLDOWN="${CLAUDE_LED_SCAN_COOLDOWN:-60}"

cmd="${1:-status}"
case "$cmd" in
  on|off|toggle|pulse|done|status) path="/$cmd" ;;
  blink) path="/blink?times=${2:-3}&ms=${3:-200}" ;;
  find|where) path="/status" ;;
  *) echo "usage: $(basename "$0") on|off|toggle|pulse|blink|done|status|find|where" >&2
     exit 2 ;;
esac

# --- уход в фон -------------------------------------------------------------
# Вызов из хука или из горячей клавиши не должен ждать сеть — такой вызов
# мгновенно отцепляется. А вот когда команду набрали руками в терминале, ответ
# платы хочется увидеть, поэтому при stdout-терминале остаёмся на переднем плане.
# CLAUDE_LED_WAIT=1 заставляет ждать всегда, CLAUDE_LED_WAIT=0 — не ждать никогда.
if [ "${CLAUDE_LED_FG:-}" != 1 ]; then
  case "$cmd" in
    status|find|where) ;;
    *)
      [ "${CLAUDE_LED_WAIT:-}" = 1 ] && wait_for_it=1 || wait_for_it=0
      [ -z "${CLAUDE_LED_WAIT:-}" ] && [ -t 1 ] && wait_for_it=1
      if [ "$wait_for_it" = 1 ]; then
        CLAUDE_LED_FG=1 exec "$0" "$@"
      fi
      if command -v setsid >/dev/null 2>&1; then
        CLAUDE_LED_FG=1 setsid "$0" "$@" >/dev/null 2>&1 </dev/null &
      else
        CLAUDE_LED_FG=1 "$0" "$@" >/dev/null 2>&1 </dev/null &
      fi
      exit 0 ;;
  esac
fi

mkdir -p "$CACHE_DIR" 2>/dev/null || true

add_token() {
  [ -z "$TOKEN" ] && { printf '%s' "$1"; return; }
  case "$1" in
    *\?*) printf '%s&token=%s' "$1" "$TOKEN" ;;
    *)    printf '%s?token=%s' "$1" "$TOKEN" ;;
  esac
}

is_ip() { case "$1" in ''|*[!0-9.]*) return 1 ;; *) return 0 ;; esac; }

url_host() { local h="${1#*://}"; h="${h%%/*}"; printf '%s' "${h%%:*}"; }

# Базовый URL, который гарантированно не потребует разрешения имени.
# Пусто — значит адреса пока нет и нужен поиск.
fast_url() {
  if [ -n "${CLAUDE_LED_URL:-}" ] && is_ip "$(url_host "$CLAUDE_LED_URL")"; then
    printf '%s' "${CLAUDE_LED_URL%/}"
  elif [ -s "$IP_CACHE" ]; then
    printf 'http://%s' "$(cat "$IP_CACHE")"
  fi
}

hit() { timeout -k 1 3 curl -fsS --max-time 2 --connect-timeout 1 "$1$(add_token "$2")" 2>/dev/null; }

# Наша ли это плата: /status отвечает JSON-ом с полем mode.
probe() {
  timeout -k 1 2 curl -fsS --max-time 1 --connect-timeout 1 \
    "http://$1$(add_token /status)" 2>/dev/null | grep -q '"mode"'
}

remember() { printf '%s\n' "$1" > "$IP_CACHE"; }

# Локальные подсети /24 — только они и сканируются; docker-мосты (/16) отсеиваются.
local_nets() {
  ip -4 -o addr show scope global 2>/dev/null \
    | awk '{print $4}' | grep '/24$' | cut -d/ -f1 \
    | awk -F. '{print $1"."$2"."$3}' | sort -u
}

discover() {
  local ip name net i found tmp

  # 1. mDNS — здесь ждать до нескольких секунд не страшно, мы уже в фоне
  name="$(url_host "${CLAUDE_LED_URL:-}")"
  [ -n "$name" ] && is_ip "$name" && name=""
  [ -z "$name" ] && name="$HOST_NAME.local"
  if command -v avahi-resolve >/dev/null 2>&1; then
    ip="$(timeout -k 1 5 avahi-resolve -4 -n "$name" 2>/dev/null | awk '{print $2}')"
    if [ -n "${ip:-}" ] && probe "$ip"; then remember "$ip"; printf '%s\n' "$ip"; return 0; fi
  else
    # avahi нет — пробуем системный резолвер (он же и .local через nss-mdns)
    ip="$(timeout -k 1 5 getent ahostsv4 "$name" 2>/dev/null | awk 'NR==1{print $1}')"
    if [ -n "${ip:-}" ] && probe "$ip"; then remember "$ip"; printf '%s\n' "$ip"; return 0; fi
  fi

  # 2. Опрос подсети: плата отвечает на /status быстрее, чем за секунду
  tmp="$(mktemp)"
  for net in $(local_nets); do
    for i in $(seq 1 254); do
      ( probe "$net.$i" && printf '%s\n' "$net.$i" >> "$tmp" ) &
    done
    wait
    found="$(head -n1 "$tmp" 2>/dev/null)"
    if [ -n "$found" ]; then
      rm -f "$tmp"; remember "$found"; printf '%s\n' "$found"; return 0
    fi
  done
  rm -f "$tmp"
  return 1
}

# Поиск с ограничением частоты: плата может быть просто выключена, и незачем
# сканировать сеть на каждый промпт.
discover_throttled() {
  local now last
  now="$(date +%s)"
  last=0
  [ -f "$SCAN_STAMP" ] && last="$(cat "$SCAN_STAMP" 2>/dev/null || echo 0)"
  [ $(( now - last )) -lt "$COOLDOWN" ] && return 1
  printf '%s\n' "$now" > "$SCAN_STAMP"
  discover >/dev/null
}

case "$cmd" in
  where)
    if [ -n "${CLAUDE_LED_URL:-}" ] && is_ip "$(url_host "$CLAUDE_LED_URL")"; then
      echo "${CLAUDE_LED_URL%/}  (CLAUDE_LED_URL)"
    elif [ -s "$IP_CACHE" ]; then
      echo "http://$(cat "$IP_CACHE")  (кэш: $IP_CACHE)"
    else
      echo "адрес неизвестен — запусти: $(basename "$0") find"
    fi
    exit 0 ;;
  find)
    ip="$(discover)" && { echo "найдено: http://$ip (записано в $IP_CACHE)"; exit 0; }
    echo "плата не найдена: её нет в сети $(local_nets | tr '\n' ' ')" >&2
    exit 1 ;;
esac

base="$(fast_url)"
if [ -n "$base" ] && hit "$base" "$path"; then exit 0; fi

# Адрес протух или его не было — ищем заново и повторяем один раз
discover_throttled || exit 0
base="$(fast_url)"
[ -n "$base" ] && hit "$base" "$path"
exit 0
