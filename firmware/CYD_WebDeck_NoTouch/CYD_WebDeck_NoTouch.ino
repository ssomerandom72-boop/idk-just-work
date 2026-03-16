// CYD WebDeck (No-Touch) Arduino sketch
// NOTE: Do not paste README markdown into this file.

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

#if __has_include(<ESPAsyncWebServer.h>) && __has_include(<AsyncTCP.h>)
  #include <ESPAsyncWebServer.h>
  #define CYD_USE_ASYNC_WEB 1
#else
  #include <WebServer.h>
  #define CYD_USE_ASYNC_WEB 0
#endif

// ===== User config =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ===== Web services =====
#if CYD_USE_ASYNC_WEB
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
#else
WebServer server(80);
#endif
WebSocketsServer legacyWs(81);  // Fallback/compatibility websocket channel.

// ===== Display =====
TFT_eSPI tft = TFT_eSPI();

// ===== Mode model =====
enum class DisplayMode {
  STANDBY,
  WEATHER,
  CRYPTO,
  SYSTEM_STATS,
  TERMINAL,
  NOTIFICATION,
  SLIDESHOW
};

DisplayMode currentMode = DisplayMode::STANDBY;
String notificationText = "";
String terminalLine = "> ready";
uint8_t brightness = 180;  // Wire this to backlight PWM pin on real hardware.

// ===== Timers =====
unsigned long bootMs = 0;
unsigned long lastModeInteractionMs = 0;
constexpr unsigned long AUTO_RETURN_MS = 120000;  // 2 min

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>CYD WebDeck</title>
  <style>
    body { font-family: sans-serif; margin: 1rem; max-width: 640px; }
    button { margin: .2rem; padding: .6rem 1rem; }
    input { margin: .2rem 0; width: 100%; }
    pre { background: #111; color: #8f8; padding: .6rem; min-height: 80px; }
  </style>
</head>
<body>
  <h2>CYD CONTROL PANEL (No Touch)</h2>
  <p id="state">Loading...</p>

  <h3>Display Mode</h3>
  <button onclick="setMode('clock')">Clock</button>
  <button onclick="setMode('weather')">Weather</button>
  <button onclick="setMode('crypto')">Crypto</button>
  <button onclick="setMode('stats')">System Stats</button>
  <button onclick="setMode('terminal')">Terminal Viewer</button>
  <button onclick="setMode('slideshow')">Image Slideshow</button>

  <h3>Actions</h3>
  <input id="msg" placeholder="Send notification text" />
  <button onclick="notifyMsg()">Send Message</button>

  <input id="bright" type="range" min="0" max="255" value="180" />
  <button onclick="setBrightness()">Brightness</button>

  <pre id="log"></pre>

<script>
const log = (v) => document.getElementById('log').textContent += v + '\n';

async function setMode(mode) {
  await fetch('/api/mode', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({mode})});
}

async function notifyMsg() {
  const text = document.getElementById('msg').value;
  await fetch('/api/notify', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({text})});
}

async function setBrightness() {
  const value = Number(document.getElementById('bright').value);
  await fetch('/api/brightness', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({value})});
}

async function refreshState() {
  const data = await (await fetch('/api/state')).json();
  document.getElementById('state').textContent = `IP: ${data.ip} | Mode: ${data.mode} | Uptime: ${data.uptime_s}s | WiFi RSSI: ${data.rssi}`;
}

refreshState();
setInterval(refreshState, 3000);

const socket = new WebSocket(`ws://${location.host}/ws`);
socket.onmessage = (ev) => log(ev.data);
socket.onerror = () => log('WebSocket /ws unavailable (sync web fallback active).');
</script>
</body>
</html>
)HTML";

String modeToString(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::STANDBY: return "clock";
    case DisplayMode::WEATHER: return "weather";
    case DisplayMode::CRYPTO: return "crypto";
    case DisplayMode::SYSTEM_STATS: return "stats";
    case DisplayMode::TERMINAL: return "terminal";
    case DisplayMode::NOTIFICATION: return "notification";
    case DisplayMode::SLIDESHOW: return "slideshow";
  }
  return "unknown";
}

DisplayMode parseMode(const String& mode) {
  if (mode == "weather") return DisplayMode::WEATHER;
  if (mode == "crypto") return DisplayMode::CRYPTO;
  if (mode == "stats") return DisplayMode::SYSTEM_STATS;
  if (mode == "terminal") return DisplayMode::TERMINAL;
  if (mode == "notification") return DisplayMode::NOTIFICATION;
  if (mode == "slideshow") return DisplayMode::SLIDESHOW;
  return DisplayMode::STANDBY;
}

void drawMode() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("CYD WebDeck");

  tft.setTextSize(1);
  tft.setCursor(10, 45);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  tft.setCursor(10, 60);
  tft.print("Mode: ");
  tft.println(modeToString(currentMode));

  tft.setCursor(10, 75);
  tft.print("WiFi RSSI: ");
  tft.println(WiFi.RSSI());

  tft.setCursor(10, 90);
  tft.print("Uptime(s): ");
  tft.println((millis() - bootMs) / 1000);

  if (currentMode == DisplayMode::NOTIFICATION) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 120);
    tft.println("ALERT:");
    tft.setCursor(10, 140);
    tft.println(notificationText);
  } else if (currentMode == DisplayMode::TERMINAL) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 120);
    tft.println("Terminal");
    tft.setCursor(10, 140);
    tft.println(terminalLine);
  }
}

void broadcastState(const String& reason = "update") {
  StaticJsonDocument<256> doc;
  doc["type"] = reason;
  doc["mode"] = modeToString(currentMode);
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime_s"] = (millis() - bootMs) / 1000;

  String payload;
  serializeJson(doc, payload);

#if CYD_USE_ASYNC_WEB
  ws.textAll(payload);
#endif
  legacyWs.broadcastTXT(payload);
}

void handleModeChange(const String& modeValue) {
  currentMode = parseMode(modeValue);
  lastModeInteractionMs = millis();
  drawMode();
  broadcastState("mode");
}

void handleNotification(const String& text) {
  notificationText = text;
  currentMode = DisplayMode::NOTIFICATION;
  lastModeInteractionMs = millis();
  drawMode();
#if CYD_USE_ASYNC_WEB
  ws.textAll("notification:" + notificationText);
#endif
  legacyWs.broadcastTXT("notification:" + notificationText);
}

void handleBrightness(uint8_t value) {
  brightness = value;
  // TODO: apply to PWM backlight output pin.
#if CYD_USE_ASYNC_WEB
  ws.textAll("brightness:" + String(brightness));
#endif
  legacyWs.broadcastTXT("brightness:" + String(brightness));
}

void setupRoutes() {
#if CYD_USE_ASYNC_WEB
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<256> doc;
    doc["mode"] = modeToString(currentMode);
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_s"] = (millis() - bootMs) / 1000;
    doc["brightness"] = brightness;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/api/mode", HTTP_POST,
    [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"ok\":true}"); },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<200> doc;
      if (deserializeJson(doc, data, len)) {
        request->send(400, "application/json", "{\"ok\":false}");
        return;
      }
      handleModeChange(String((const char*)doc["mode"]));
    }
  );

  server.on("/api/notify", HTTP_POST,
    [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"ok\":true}"); },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<300> doc;
      if (deserializeJson(doc, data, len)) {
        request->send(400, "application/json", "{\"ok\":false}");
        return;
      }
      handleNotification(String((const char*)doc["text"]));
    }
  );

  server.on("/api/brightness", HTTP_POST,
    [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"ok\":true}"); },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<100> doc;
      if (deserializeJson(doc, data, len)) {
        request->send(400, "application/json", "{\"ok\":false}");
        return;
      }
      handleBrightness(doc["value"] | brightness);
    }
  );

  ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void*, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("connected");
      return;
    }

    if (type == WS_EVT_DATA && len > 0) {
      String inbound;
      inbound.reserve(len);
      for (size_t i = 0; i < len; i++) inbound += (char)data[i];

      if (inbound.startsWith("log:")) {
        terminalLine = inbound.substring(4);
        currentMode = DisplayMode::TERMINAL;
        lastModeInteractionMs = millis();
        drawMode();
        ws.textAll(inbound);
        legacyWs.broadcastTXT(inbound);
      }
    }
  });

  server.addHandler(&ws);
#else
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", HTTP_GET, []() {
    StaticJsonDocument<256> doc;
    doc["mode"] = modeToString(currentMode);
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_s"] = (millis() - bootMs) / 1000;
    doc["brightness"] = brightness;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  server.on("/api/mode", HTTP_POST, []() {
    StaticJsonDocument<200> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json", "{\"ok\":false}");
      return;
    }
    handleModeChange(String((const char*)doc["mode"]));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/notify", HTTP_POST, []() {
    StaticJsonDocument<300> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json", "{\"ok\":false}");
      return;
    }
    handleNotification(String((const char*)doc["text"]));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/brightness", HTTP_POST, []() {
    StaticJsonDocument<100> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json", "{\"ok\":false}");
      return;
    }
    handleBrightness(doc["value"] | brightness);
    server.send(200, "application/json", "{\"ok\":true}");
  });
#endif

  server.begin();
  legacyWs.begin();
  legacyWs.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (type == WStype_CONNECTED) {
      legacyWs.sendTXT(num, "connected");
      return;
    }

    if (type == WStype_TEXT && len > 0) {
      String inbound((const char*)payload);
      if (inbound.startsWith("log:")) {
        terminalLine = inbound.substring(4);
        currentMode = DisplayMode::TERMINAL;
        lastModeInteractionMs = millis();
        drawMode();
        legacyWs.broadcastTXT(inbound);
      }
    }
  });
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000) {
    delay(300);
  }
}

void setup() {
  bootMs = millis();
  lastModeInteractionMs = bootMs;

  tft.init();
  tft.setRotation(1);

  connectWifi();
  drawMode();
  setupRoutes();
}

void loop() {
#if !CYD_USE_ASYNC_WEB
  server.handleClient();
#endif
  legacyWs.loop();

  static unsigned long lastTick = 0;
  if (millis() - lastTick > 1000) {
    lastTick = millis();

    // Auto-return to standby for no-touch usability.
    if (currentMode != DisplayMode::STANDBY && millis() - lastModeInteractionMs > AUTO_RETURN_MS) {
      currentMode = DisplayMode::STANDBY;
      drawMode();
      broadcastState("auto-standby");
    }

    // Lightweight live refresh in current mode.
    drawMode();
  }
}
