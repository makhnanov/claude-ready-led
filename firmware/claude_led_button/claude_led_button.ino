/*
 * claude-led-button — беспроводная кнопка для claude-led на ESP8266 (NodeMCU v3).
 *
 * Зачем: переключать светодиод основной платы без компьютера. Плата висит на
 * любой USB-зарядке, нужен только WiFi.
 *
 *   кнопка RESET — плата загружается, дёргает /toggle основной платы и остаётся в сети
 *   кнопка FLASH — мгновенный повторный /toggle, пока плата не перезагружалась
 *
 * Обратная связь синим светодиодом на самой плате (D4, горит инверсно):
 *   одна длинная вспышка  — основная плата ответила, светодиод переключён
 *   три быстрых вспышки   — не достучались: нет WiFi или плата не отвечает
 *
 * Адрес основной платы ищется по приоритету: кэш в RTC-памяти -> TARGET_HOST
 * из secrets.h -> имя claude-led.local через mDNS (lwIP умеет .local сам,
 * LWIP_DNS_SUPPORT_MDNS_QUERIES=1). Сработавший адрес и точка доступа
 * кладутся в RTC, поэтому следующее нажатие RESET не тратит время на поиск.
 *
 * Схема: ничего припаивать не нужно, используются кнопки и светодиод платы.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "secrets.h"

static const uint8_t LED_PIN    = D4;  // GPIO2, встроенный светодиод, инверсный
static const uint8_t BUTTON_PIN = D3;  // GPIO0, кнопка FLASH, замыкает на землю

// RTC-память переживает RESET и deep sleep, но не отключение питания.
// После переподключения платы кэш просто окажется пустым — не страшно,
// TARGET_HOST в secrets.h всё равно даёт готовый адрес.
static const uint32_t RTC_MAGIC = 0x4C454431;  // "LED1"

struct RtcState {
  uint32_t magic;
  uint32_t targetIp;    // последний адрес, который ответил
  uint8_t  bssid[6];    // точка доступа, к которой подключались
  uint8_t  channel;
  uint8_t  reserved;
};

RtcState rtc;
bool     rtcValid = false;

void ledOn()  { digitalWrite(LED_PIN, LOW);  }   // инверсный
void ledOff() { digitalWrite(LED_PIN, HIGH); }

void blinkLed(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    ledOn();  delay(ms);
    ledOff(); delay(ms);
  }
}

void rtcLoad() {
  if (!ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc, sizeof(rtc))) return;
  rtcValid = (rtc.magic == RTC_MAGIC);
}

void rtcSave() {
  rtc.magic = RTC_MAGIC;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc, sizeof(rtc));
}

// Подключение к WiFi. Если в RTC лежат канал и BSSID прошлой точки доступа,
// сначала пробуем прицельно: это экономит скан всех каналов, то есть пару секунд.
bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.hostname("claude-led-button");

  if (rtcValid && rtc.channel >= 1 && rtc.channel <= 14) {
    Serial.printf("WiFi: прицельно, канал %u\n", rtc.channel);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, rtc.channel, rtc.bssid, true);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 3000) delay(20);
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println("WiFi: прицельно не вышло, обычный скан");
    WiFi.disconnect();
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(50);
  return WiFi.status() == WL_CONNECTED;
}

// Один GET. true — основная плата ответила 200.
bool hit(IPAddress ip, const char* path) {
  if (ip == IPAddress((uint32_t)0)) return false;

  String url = "http://" + ip.toString() + path;
  if (strlen(API_TOKEN) > 0) {
    url += (strchr(path, '?') ? '&' : '?');
    url += "token=";
    url += API_TOKEN;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2000);
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  String body = (code == HTTP_CODE_OK) ? http.getString() : String();
  http.end();

  Serial.printf("GET %s -> %d %s", url.c_str(), code, body.c_str());
  if (code != HTTP_CODE_OK) Serial.println();
  return code == HTTP_CODE_OK;
}

// Адрес-кандидат из secrets.h: либо готовый IP, либо имя, которое надо резолвить.
IPAddress resolveTarget() {
  IPAddress ip;
  if (ip.fromString(TARGET_HOST)) return ip;       // записан IP — резолв не нужен

  Serial.printf("Резолвлю %s...\n", TARGET_HOST);
  if (WiFi.hostByName(TARGET_HOST, ip, 4000) == 1) {
    Serial.print("-> ");
    Serial.println(ip);
    return ip;
  }
  Serial.println("-> не резолвится");
  return IPAddress((uint32_t)0);
}

// Переключить светодиод основной платы, перебрав все способы найти её адрес.
bool toggleMainBoard() {
  // 1. Адрес, который сработал в прошлый раз
  if (rtcValid) {
    IPAddress cached(rtc.targetIp);
    if (hit(cached, "/toggle")) return true;
    Serial.println("Кэшированный адрес молчит");
  }

  // 2. TARGET_HOST из secrets.h
  IPAddress ip = resolveTarget();
  if (hit(ip, "/toggle")) {
    rtc.targetIp = (uint32_t)ip;
    return true;
  }

  // 3. mDNS-имя как последняя попытка — на случай, если в TARGET_HOST
  //    записан IP, а плата уехала на другой адрес
  IPAddress byName;
  if (WiFi.hostByName(MDNS_FALLBACK, byName, 4000) == 1 && hit(byName, "/toggle")) {
    rtc.targetIp = (uint32_t)byName;
    return true;
  }

  return false;
}

// Общий путь для RESET и FLASH: переключить и отчитаться светодиодом.
void doToggle() {
  uint32_t t0 = millis();
  bool ok = toggleMainBoard();

  if (ok) {
    memcpy(rtc.bssid, WiFi.BSSID(), 6);
    rtc.channel = WiFi.channel();
    rtcSave();
    rtcValid = true;
    Serial.printf("OK за %lu мс\n", millis() - t0);
    ledOn(); delay(400); ledOff();           // одна длинная — получилось
  } else {
    Serial.printf("Не вышло, %lu мс\n", millis() - t0);
    blinkLed(3, 80);                          // три быстрых — не получилось
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  ledOff();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("\n\nclaude-led-button");

  rtcLoad();
  Serial.printf("RTC-кэш: %s\n", rtcValid ? "есть" : "пусто");

  if (!connectWifi()) {
    Serial.println("WiFi не поднялся");
    blinkLed(3, 80);
    return;                                   // loop() попробует переподключиться
  }

  Serial.print("Мой IP: ");
  Serial.println(WiFi.localIP());

  doToggle();                                 // то самое, ради чего нажали RESET
}

// Плата остаётся в сети, поэтому кнопка FLASH переключает светодиод мгновенно,
// без перезагрузки и без повторного подключения к WiFi.
void loop() {
  static uint32_t lastPress = 0;

  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > 500) {
    lastPress = millis();
    if (WiFi.status() == WL_CONNECTED) {
      doToggle();
    } else {
      blinkLed(3, 80);
    }
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);   // ждём отпускания
    lastPress = millis();
  }

  delay(20);
}
