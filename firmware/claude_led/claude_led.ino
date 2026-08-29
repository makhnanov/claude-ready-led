/*
 * claude-led — индикатор статуса Claude Code на ESP8266 (NodeMCU v3).
 *
 * Светодиод на D6 (GPIO12):
 *   /off      — погасить
 *   /on       — зажечь ровным светом
 *   /pulse    — плавное дыхание (Claude работает)
 *   /blink    — мигание, ?times=3&ms=200
 *   /done     — 3 быстрых мигания и остаться гореть (работа закончена)
 *   /status   — JSON с текущим состоянием
 *
 * Схема: D6 --[резистор 220..330 Ом]-- анод(+, длинная ножка) LED, катод -- GND.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "secrets.h"

static const uint8_t LED_PIN = D6;  // GPIO12

ESP8266WebServer server(80);

enum Mode { MODE_OFF, MODE_ON, MODE_PULSE, MODE_BLINK };

Mode     mode          = MODE_OFF;
uint16_t blinkPeriodMs = 200;
int16_t  blinksLeft    = 0;
Mode     modeAfterBlink = MODE_OFF;
uint32_t lastStep      = 0;
bool     blinkState    = false;

void setLevel(uint16_t duty) {  // 0..1023
  analogWrite(LED_PIN, duty);
}

void applyMode(Mode m) {
  mode = m;
  lastStep = millis();
  if (m == MODE_OFF) setLevel(0);
  if (m == MODE_ON)  setLevel(1023);
}

void startBlink(int16_t times, uint16_t periodMs, Mode after) {
  blinksLeft     = times * 2;   // считаем полупериоды
  blinkPeriodMs  = periodMs;
  modeAfterBlink = after;
  blinkState     = false;
  mode           = MODE_BLINK;
  lastStep       = 0;           // сработает сразу
}

bool authorized() {
  const char* token = API_TOKEN;
  if (token[0] == '\0') return true;
  if (server.hasArg("token") && server.arg("token") == token) return true;
  server.send(401, "text/plain; charset=utf-8", "unauthorized\n");
  return false;
}

String statusJson() {
  const char* names[] = {"off", "on", "pulse", "blink"};
  String s = "{\"mode\":\"";
  s += names[mode];
  s += "\",\"ip\":\"" + WiFi.localIP().toString();
  s += "\",\"rssi\":" + String(WiFi.RSSI());
  s += ",\"uptime_s\":" + String(millis() / 1000);
  s += "}\n";
  return s;
}

void reply() { server.send(200, "application/json; charset=utf-8", statusJson()); }

void handleRoot() {
  String html =
    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>claude-led</title>"
    "<style>body{font:16px system-ui;margin:0;display:grid;place-items:center;height:100vh;"
    "background:#14110f;color:#f0eee6}a{display:block;width:220px;margin:6px;padding:14px;text-align:center;"
    "border-radius:10px;background:#2a2521;color:#f0eee6;text-decoration:none}a:hover{background:#c96442}"
    "code{color:#b0aca4}</style><h2>claude-led</h2>"
    "<div><a href=/on>ON</a><a href=/off>OFF</a><a href=/pulse>PULSE</a>"
    "<a href=/blink>BLINK</a><a href=/done>DONE</a></div><code>" + statusJson() + "</code>";
  server.send(200, "text/html; charset=utf-8", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  analogWriteRange(1023);
  setLevel(0);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(MDNS_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("=== IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(" ===");
  Serial.print("=== http://");
  Serial.print(MDNS_HOSTNAME);
  Serial.println(".local/ ===");

  if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);

  server.on("/",      handleRoot);
  server.on("/on",     []{ if (!authorized()) return; applyMode(MODE_ON);    reply(); });
  server.on("/off",    []{ if (!authorized()) return; applyMode(MODE_OFF);   reply(); });
  server.on("/pulse",  []{ if (!authorized()) return; applyMode(MODE_PULSE); reply(); });
  server.on("/status", []{ if (!authorized()) return; reply(); });
  server.on("/blink",  []{
    if (!authorized()) return;
    int t  = server.hasArg("times") ? server.arg("times").toInt() : 3;
    int ms = server.hasArg("ms")    ? server.arg("ms").toInt()    : 200;
    startBlink(constrain(t, 1, 100), constrain(ms, 30, 5000), MODE_OFF);
    reply();
  });
  server.on("/done",   []{
    if (!authorized()) return;
    startBlink(3, 120, MODE_ON);   // помигал и остался гореть
    reply();
  });
  server.onNotFound([]{ server.send(404, "text/plain; charset=utf-8", "no such endpoint\n"); });

  server.begin();
  startBlink(2, 100, MODE_OFF);    // «я загрузился и в сети»
}

void loop() {
  MDNS.update();
  server.handleClient();

  uint32_t now = millis();

  if (mode == MODE_BLINK) {
    if (now - lastStep >= blinkPeriodMs / 2) {
      lastStep   = now;
      blinkState = !blinkState;
      setLevel(blinkState ? 1023 : 0);
      if (--blinksLeft <= 0) applyMode(modeAfterBlink);
    }
  } else if (mode == MODE_PULSE) {
    // треугольная волна 2 c, гамма-коррекция для ровного на глаз дыхания
    float phase = (now % 2000) / 1000.0f;          // 0..2
    float x     = phase < 1.0f ? phase : 2.0f - phase;
    setLevel((uint16_t)(x * x * 1023));
  }
}
