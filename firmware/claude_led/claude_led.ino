/*
 * claude-led — индикатор статуса Claude Code на ESP8266 (NodeMCU v3).
 *
 * Светодиод на D6 (GPIO12):
 *   /off      — погасить
 *   /toggle   — переключить: погашен -> зажечь, иначе погасить
 *   /on       — зажечь ровным светом
 *   /pulse    — плавное дыхание (Claude работает)
 *   /blink    — мигание, ?times=3&ms=200
 *   /done     — 3 быстрых мигания и остаться гореть (работа закончена)
 *   /status   — JSON с текущим состоянием
 *   /         — веб-страничка: кнопки шлют fetch, ответ рисуется без перезагрузки
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

// Страничка целиком лежит во flash (PROGMEM) и отдаётся через send_P —
// в RAM она не копируется. Статус страница добирает сама через fetch("/status").
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>claude-led</title>
<style>
:root{--bg:#14110f;--fg:#f0eee6;--dim:#8f8880;--card:#221e1b;--accent:#c96442;
      --err:#e06c5a;--off:#2a2521;--edge:#3a332e;--glow:#ffd479}
@media(prefers-color-scheme:light){
  :root{--bg:#faf9f5;--fg:#1a1815;--dim:#6b645c;--card:#ebe8e1;--off:#e2ded6;--edge:#cdc7bd}
}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;display:flex;flex-direction:column;align-items:center;
     justify-content:center;gap:22px;padding:24px;background:var(--bg);color:var(--fg);
     font:16px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}
h1{margin:0;font-size:19px;font-weight:600}
#lamp{width:88px;height:88px;border-radius:50%;background:var(--off);
      border:2px solid var(--edge);transition:background .25s,box-shadow .25s}
#lamp.on{background:var(--glow);border-color:var(--glow);box-shadow:0 0 34px 6px #ffd47990}
#lamp.pulse{animation:br 2s ease-in-out infinite}
#lamp.blink{animation:bl .24s steps(1) infinite}
#lamp.err{background:#3a1f1c;border-color:var(--err);box-shadow:none}
@keyframes br{0%,100%{background:var(--off);box-shadow:none}
              50%{background:var(--glow);box-shadow:0 0 34px 6px #ffd47990}}
@keyframes bl{0%{background:var(--glow);box-shadow:0 0 30px 6px #ffd47980}
              50%{background:var(--off);box-shadow:none}}
.btns{display:grid;grid-template-columns:repeat(3,minmax(94px,1fr));gap:10px;
      width:100%;max-width:340px}
button{font:inherit;font-weight:600;padding:14px 6px;border:0;border-radius:10px;
       background:var(--card);color:var(--fg);cursor:pointer;transition:background .15s}
button:hover:not(:disabled){background:var(--accent);color:#fff}
button:disabled{opacity:.45;cursor:progress}
#out{min-height:3.4em;text-align:center;color:var(--dim);
     font:13px/1.7 ui-monospace,SFMono-Regular,Menlo,monospace}
#out b{color:var(--fg)} #out .e{color:var(--err)}
</style>
<h1>claude-led</h1>
<div id="lamp"></div>
<div class="btns">
  <button data-a="toggle">TOGGLE</button><button data-a="on">ON</button>
  <button data-a="off">OFF</button><button data-a="pulse">PULSE</button>
  <button data-a="blink">BLINK</button><button data-a="done">DONE</button>
</div>
<div id="out">&#8230;</div>
<script>
var lamp=document.getElementById('lamp'),out=document.getElementById('out'),
    btns=document.querySelectorAll('button'),busy=false,
    tok=new URLSearchParams(location.search).get('token');

function url(a){return '/'+a+(tok?'?token='+encodeURIComponent(tok):'')}

function human(s){var h=s/3600|0,m=s%3600/60|0;
  return h?h+' ч '+m+' мин':(m?m+' мин '+(s%60)+' с':s+' с')}

function draw(d,ms){
  lamp.className=d.mode;
  out.innerHTML='режим <b>'+d.mode+'</b> &middot; ответ за '+ms+' мс<br>'+
                d.ip+' &middot; сигнал '+d.rssi+' dBm &middot; в сети '+human(d.uptime_s);
}

async function call(a,quiet){
  if(busy)return; busy=true;
  if(!quiet)btns.forEach(function(b){b.disabled=true});
  var t0=performance.now();
  try{
    var r=await fetch(url(a),{cache:'no-store'});
    if(!r.ok)throw new Error('HTTP '+r.status);
    draw(await r.json(),Math.round(performance.now()-t0));
  }catch(e){
    lamp.className='err';
    out.innerHTML='<span class="e">нет связи с платой</span><br>'+e.message;
  }finally{
    busy=false; btns.forEach(function(b){b.disabled=false});
  }
}

btns.forEach(function(b){b.addEventListener('click',function(){call(b.dataset.a)})});
call('status',true);
setInterval(function(){if(!document.hidden)call('status',true)},2000);
</script>
)HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
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
  // погашен -> зажечь, в любом другом режиме -> погасить
  server.on("/toggle", []{ if (!authorized()) return;
                           applyMode(mode == MODE_OFF ? MODE_ON : MODE_OFF); reply(); });
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
