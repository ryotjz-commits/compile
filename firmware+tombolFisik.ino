
/*
 ============================================================
                    NOVIA HOME V4.0
                 PREMIUM SMART HOME SYSTEM
 ============================================================

 BOARD       : ESP32 DEVKIT V1
 FLASH       : 4 MB
 IDE         : ArduinoDroid
 CONTROLLER  : BLYNK IoT
 WEB SERVER  : HTTP PORT 80
 WIFI        : AP + STA
 STORAGE     : Preferences
 RELAY       : ACTIVE HIGH

 FEATURES:
 - 4 Channel Relay
 - 4 Physical Toggle Buttons
 - Blynk IoT V0-V3
 - Web Dashboard Premium
 - Hybrid Dark / Light Theme
 - Digital Clock and Date
 - Relay Statistics
 - WiFi Scanner Asynchronous
 - AP + STA
 - WiFi Auto Reconnect
 - Blynk Auto Reconnect
 - WiFi Preferences
 - Real-time Device Status
 - Responsive Mobile Dashboard
 - All Relay OFF After Restart

 RELAY:
 CH1 = GPIO26
 CH2 = GPIO27
 CH3 = GPIO12
 CH4 = GPIO14

 BUTTON:
 CH1 = GPIO32
 CH2 = GPIO33
 CH3 = GPIO25
 CH4 = GPIO23

 BLYNK:
 V0 = CH1
 V1 = CH2
 V2 = CH3
 V3 = CH4

 ============================================================
*/

// ============================================================
// 1. BLYNK CONFIGURATION
// ============================================================

#define BLYNK_TEMPLATE_ID "TMPL6Yk8Hycr-"
#define BLYNK_TEMPLATE_NAME "Novia Home"
#define BLYNK_AUTH_TOKEN "uUhUdnlB22cqJfKinHQu8yGMXLRbgzf1"

#define BLYNK_PRINT Serial

// ============================================================
// 2. LIBRARIES
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>

// ============================================================
// 3. HARDWARE CONFIGURATION
// ============================================================

#define RELAY_ON  HIGH
#define RELAY_OFF LOW

#define RELAY_COUNT 4

const uint8_t relayPins[4] = {
  26, 27, 13, 14
};

const uint8_t buttonPins[4] = {
  32, 33, 25, 23
};

const char* channelNames[4] = {
  "Lampu Depan",
  "Lampu Tengah",
  "Lampu Kamar",
  "Kipas Angin"
};

// ============================================================
// 4. NETWORK CONFIGURATION
// ============================================================

const char* AP_SSID = "NOVIA-HOME";
const char* AP_PASS = "Noke0710";

WebServer server(80);
Preferences preferences;

// ============================================================
// 5. TIMING CONFIGURATION
// ============================================================

const unsigned long DEBOUNCE_TIME = 50;

const unsigned long WIFI_INTERVAL = 15000;

const unsigned long BLYNK_INTERVAL = 5000;

const unsigned long STATUS_INTERVAL = 1000;

unsigned long lastWiFiAttempt = 0;
unsigned long lastBlynkAttempt = 0;
unsigned long lastStatusLog = 0;

// ============================================================
// 6. CHANNEL VARIABLES
// ============================================================

bool channelState[4] = {
  false, false, false, false
};

// ============================================================
// 7. BUTTON VARIABLES
// ============================================================

bool buttonStable[4] = {
  HIGH, HIGH, HIGH, HIGH
};

bool buttonLast[4] = {
  HIGH, HIGH, HIGH, HIGH
};

unsigned long buttonTimer[4] = {
  0, 0, 0, 0
};

// ============================================================
// 8. WIFI VARIABLES
// ============================================================

String wifiSSID = "";
String wifiPASS = "";

bool wifiConfigured = false;

// ============================================================
// 9. SCANNER VARIABLES
// ============================================================

bool scanRunning = false;

// ============================================================
// 10. FUNCTION DECLARATIONS
// ============================================================

void applyChannel(uint8_t ch, bool state,
                  bool updateBlynk = true);

void syncAllChannels();

void handleButtons();

void loadWiFiConfig();

void saveWiFiConfig(String ssid, String pass);

void connectWiFi();

void handleWiFi();

void handleBlynk();

void setupWebServer();

void handleState();

void handleSetChannel();

void handleScanStart();

void handleScanStatus();

void handleSaveWiFi();

void handleRoot();

void handleStatusLog();

String jsonEscape(String value);

// ============================================================
// 11. CENTRAL CHANNEL CONTROLLER
// ============================================================

void applyChannel(uint8_t ch, bool state,
                  bool updateBlynk) {

  if (ch >= RELAY_COUNT) return;

  channelState[ch] = state;

  digitalWrite(
    relayPins[ch],
    state ? RELAY_ON : RELAY_OFF
  );

  Serial.print("[NOVIA] ");
  Serial.print(channelNames[ch]);
  Serial.print(" : ");
  Serial.println(state ? "ON" : "OFF");

  // Blynk synchronization

  if (updateBlynk && Blynk.connected()) {

    switch (ch) {

      case 0:
        Blynk.virtualWrite(V0, state ? 1 : 0);
        break;

      case 1:
        Blynk.virtualWrite(V1, state ? 1 : 0);
        break;

      case 2:
        Blynk.virtualWrite(V2, state ? 1 : 0);
        break;

      case 3:
        Blynk.virtualWrite(V3, state ? 1 : 0);
        break;
    }
  }
}

// ============================================================
// 12. SYNCHRONIZE ALL CHANNELS
// ============================================================

void syncAllChannels() {

  if (!Blynk.connected()) return;

  Blynk.virtualWrite(V0, channelState[0] ? 1 : 0);
  Blynk.virtualWrite(V1, channelState[1] ? 1 : 0);
  Blynk.virtualWrite(V2, channelState[2] ? 1 : 0);
  Blynk.virtualWrite(V3, channelState[3] ? 1 : 0);

  Serial.println("[NOVIA] ALL CHANNELS SYNCHRONIZED");
}

// ============================================================
// 13. PHYSICAL BUTTON HANDLER
// ============================================================

void handleButtons() {

  unsigned long now = millis();

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {

    bool reading = digitalRead(buttonPins[i]);

    if (reading != buttonLast[i]) {

      buttonTimer[i] = now;

      buttonLast[i] = reading;
    }

    if (now - buttonTimer[i] >= DEBOUNCE_TIME) {

      if (reading != buttonStable[i]) {

        buttonStable[i] = reading;

        // Button pressed

        if (buttonStable[i] == LOW) {

          bool nextState = !channelState[i];

          Serial.print("[BUTTON] CH");
          Serial.print(i + 1);
          Serial.println(" PRESSED");

          applyChannel(i, nextState, true);
        }
      }
    }
  }
}

// ============================================================
// 14. BLYNK CALLBACKS
// ============================================================

BLYNK_WRITE(V0) {

  applyChannel(0, param.asInt() == 1, false);
}

BLYNK_WRITE(V1) {

  applyChannel(1, param.asInt() == 1, false);
}

BLYNK_WRITE(V2) {

  applyChannel(2, param.asInt() == 1, false);
}

BLYNK_WRITE(V3) {

  applyChannel(3, param.asInt() == 1, false);
}

// ============================================================
// 15. BLYNK CONNECTED
// ============================================================

BLYNK_CONNECTED() {

  Serial.println("[NOVIA] BLYNK CONNECTED");

  // Device is authoritative after reconnection.

  syncAllChannels();
}

// ============================================================
// 16. WIFI CONFIGURATION LOAD
// ============================================================

void loadWiFiConfig() {

  preferences.begin("novia", true);

  wifiSSID = preferences.getString("ssid", "");
  wifiPASS = preferences.getString("pass", "");

  preferences.end();

  wifiConfigured = wifiSSID.length() > 0;

  Serial.println("[NOVIA] WIFI CONFIG LOADED");

  if (wifiConfigured) {

    Serial.print("[NOVIA] SSID: ");
    Serial.println(wifiSSID);
  }
}

// ============================================================
// 17. WIFI CONFIGURATION SAVE
// ============================================================

void saveWiFiConfig(String ssid, String pass) {

  preferences.begin("novia", false);

  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);

  preferences.end();

  wifiSSID = ssid;
  wifiPASS = pass;

  wifiConfigured = true;

  // Stop an existing scan before reconnecting.

  if (scanRunning) {

    WiFi.scanDelete();

    scanRunning = false;
  }

  WiFi.disconnect(false, false);

  lastWiFiAttempt = 0;

  Serial.println("[NOVIA] WIFI CONFIG SAVED");
}

// ============================================================
// 18. WIFI CONNECTION
// ============================================================

void connectWiFi() {

  if (!wifiConfigured) return;

  Serial.println("[NOVIA] CONNECTING WIFI");

  WiFi.begin(
    wifiSSID.c_str(),
    wifiPASS.c_str()
  );

  lastWiFiAttempt = millis();
}

// ============================================================
// 19. WIFI AUTO RECONNECT
// ============================================================

void handleWiFi() {

  if (!wifiConfigured) return;

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  // Avoid reconnect during scan.

  if (scanRunning) return;

  if (millis() - lastWiFiAttempt >= WIFI_INTERVAL) {

    Serial.println("[NOVIA] WIFI RECONNECTING");

    WiFi.begin(
      wifiSSID.c_str(),
      wifiPASS.c_str()
    );

    lastWiFiAttempt = millis();
  }
}

// ============================================================
// 20. BLYNK AUTO RECONNECT
// ============================================================

void handleBlynk() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (Blynk.connected()) {

    Blynk.run();

    return;
  }

  if (millis() - lastBlynkAttempt >= BLYNK_INTERVAL) {

    Serial.println("[NOVIA] CONNECTING BLYNK");

    lastBlynkAttempt = millis();

    // Short connection timeout.

    Blynk.connect(100);
  }
}

// ============================================================
// 21. JSON ESCAPE
// ============================================================

String jsonEscape(String value) {

  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "\\r");
  value.replace("\t", "\\t");

  return value;
}

// ============================================================
// 22. DEVICE STATE API
// ============================================================

void handleState() {

  String json = "{";

  json += "\"wifi\":";

  json += WiFi.status() == WL_CONNECTED
          ? "true" : "false";

  json += ",\"blynk\":";

  json += Blynk.connected()
          ? "true" : "false";

  json += ",\"ip\":\"";

  if (WiFi.status() == WL_CONNECTED) {

    json += WiFi.localIP().toString();

  } else {

    json += WiFi.softAPIP().toString();
  }

  json += "\",\"rssi\":";

  json += WiFi.status() == WL_CONNECTED
          ? String(WiFi.RSSI()) : "0";

  uint8_t active = 0;

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {

    if (channelState[i]) active++;
  }

  json += ",\"active\":";
  json += String(active);

  json += ",\"channels\":[";

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {

    json += channelState[i] ? "1" : "0";

    if (i < RELAY_COUNT - 1) {
      json += ",";
    }
  }

  json += "]}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// 23. CHANNEL CONTROL API
// ============================================================

void handleSetChannel() {

  if (!server.hasArg("ch") ||
      !server.hasArg("state")) {

    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"INVALID REQUEST\"}"
    );

    return;
  }

  String chArg = server.arg("ch");
  String stateArg = server.arg("state");

  if (chArg.length() != 1 ||
      stateArg.length() != 1) {

    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"INVALID VALUE\"}"
    );

    return;
  }

  int ch = chArg.toInt();
  int state = stateArg.toInt();

  if (ch < 1 || ch > 4 ||
      (state != 0 && state != 1)) {

    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"OUT OF RANGE\"}"
    );

    return;
  }

  applyChannel(ch - 1, state == 1, true);

  server.send(
    200,
    "application/json",
    "{\"ok\":true}"
  );
}

// ============================================================
// 24. ASYNCHRONOUS WIFI SCAN START
// ============================================================

void handleScanStart() {

  if (scanRunning) {

    server.send(
      409,
      "application/json",
      "{\"ok\":false,\"message\":\"SCAN RUNNING\"}"
    );

    return;
  }

  WiFi.scanDelete();

  int result = WiFi.scanNetworks(true, false);

  if (result == WIFI_SCAN_FAILED) {

    server.send(
      500,
      "application/json",
      "{\"ok\":false,\"message\":\"SCAN FAILED\"}"
    );

    return;
  }

  scanRunning = true;

  server.send(
    202,
    "application/json",
    "{\"ok\":true,\"scanning\":true}"
  );
}

// ============================================================
// 25. ASYNCHRONOUS WIFI SCAN STATUS
// ============================================================

void handleScanStatus() {

  if (!scanRunning) {

    server.send(
      200,
      "application/json",
      "{\"scanning\":false,\"networks\":[]}"
    );

    return;
  }

  int result = WiFi.scanComplete();

  if (result == WIFI_SCAN_RUNNING) {

    server.send(
      200,
      "application/json",
      "{\"scanning\":true,\"networks\":[]}"
    );

    return;
  }

  if (result == WIFI_SCAN_FAILED) {

    scanRunning = false;

    WiFi.scanDelete();

    server.send(
      500,
      "application/json",
      "{\"scanning\":false,\"error\":\"SCAN FAILED\"}"
    );

    return;
  }

  String json = "{\"scanning\":false,\"networks\":[";

  for (int i = 0; i < result; i++) {

    if (i > 0) json += ",";

    json += "{\"ssid\":\"";

    json += jsonEscape(WiFi.SSID(i));

    json += "\",\"rssi\":";

    json += String(WiFi.RSSI(i));

    json += ",\"secure\":";

    json += WiFi.encryptionType(i) == WIFI_AUTH_OPEN
            ? "false" : "true";

    json += "}";
  }

  json += "]}";

  WiFi.scanDelete();

  scanRunning = false;

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// 26. WIFI SAVE HANDLER
// ============================================================

void handleSaveWiFi() {

  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0 ||
      ssid.length() > 32 ||
      pass.length() > 63) {

    server.send(
      400,
      "text/plain",
      "INVALID WIFI CONFIGURATION"
    );

    return;
  }

  saveWiFiConfig(ssid, pass);

  server.send(
    200,
    "text/html",
    "<!DOCTYPE html><html lang='id'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<body style='background:#151a1d;color:white;font-family:Arial;padding:30px'>"
    "<h2>WiFi Berhasil Disimpan</h2>"
    "<p>ESP32 akan mencoba menyambungkan jaringan.</p>"
    "<a style='color:#8ed2a7' href='/'>Kembali ke NOVIA HOME</a>"
    "</body></html>"
  );
}

// ============================================================
// 27. HTML DASHBOARD
// ============================================================

const char INDEX_HTML[] PROGMEM = R"NOVIAHTML(

<!DOCTYPE html>
<html lang="id">

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width,initial-scale=1,maximum-scale=1">

<meta name="theme-color" content="#151a1d">

<title>NOVIA HOME V4</title>

<style>
:root {
  color-scheme: dark;

  --bg: #151a1d;
  --surface: #20282b;
  --surface2: #263034;

  --text: #f1f5f2;
  --muted: #91a39a;

  --accent: #83c99f;
  --accent-bg: #315b46;

  --border: rgba(255,255,255,.07);
  --danger: #df7777;

  --shadow: 0 8px 25px rgba(0,0,0,.12);
}

body.light {
  color-scheme: light;

  --bg: #f1f5f2;
  --surface: #ffffff;
  --surface2: #e9efeb;

  --text: #24332b;
  --muted: #74847a;

  --accent: #438e65;
  --accent-bg: #dcefe3;

  --border: rgba(20,60,35,.09);
  --danger: #c65c5c;

  --shadow: 0 8px 25px rgba(30,60,40,.06);
}


/* =====================================================
   02. GLOBAL
===================================================== */

*,
*::before,
*::after {
  box-sizing: border-box;
}

html {
  scroll-behavior: smooth;
}

body {
  margin: 0;
  padding: 18px 18px 35px;

  background: var(--bg);
  color: var(--text);

  font-family: Arial, sans-serif;

  transition:
    background .25s ease,
    color .25s ease;
}

button,
input,
select {
  font: inherit;
}

button {
  cursor: pointer;
}

.container {
  width: 100%;
  max-width: 850px;
  margin: 0 auto;
}


/* =====================================================
   03. HEADER
===================================================== */

.header {
  display: flex;
  align-items: center;
  justify-content: space-between;

  margin-bottom: 23px;
}

.brand {
  display: flex;
  align-items: center;
  gap: 14px;
}

.brand-icon {
  width: 46px;
  height: 46px;

  flex-shrink: 0;

  display: grid;
  place-items: center;

  border-radius: 12px;

  background: var(--accent-bg);
  color: var(--accent);

  font-size: 23px;
}

.brand strong {
  font-size: 19px;
  letter-spacing: 1px;
}

.brand small {
  display: block;
  margin-top: 5px;

  color: var(--muted);

  font-size: 10px;
  letter-spacing: 1.5px;
}

.icon-button {
  width: 43px;
  height: 43px;

  border: 1px solid var(--border);
  border-radius: 14px;

  background: var(--surface);
  color: var(--text);

  font-size: 20px;

  transition: .2s;
}

.icon-button:hover {
  border-color: var(--accent);
}


/* =====================================================
   04. HERO / CLOCK
===================================================== */

.hero {
  padding: 20px;
  margin-bottom: 15px;

  background: var(--surface);

  border: 1px solid var(--border);
  border-radius: 20px;

  box-shadow: var(--shadow);
}

.eyebrow {
  color: var(--muted);

  font-size: 11px;
  letter-spacing: 1.5px;
}

.clock {
  margin: 8px 0;

  font-size: 35px;
  font-weight: bold;
  letter-spacing: 1px;
}

.date {
  color: var(--muted);
  font-size: 13px;
}


/* =====================================================
   05. STATUS CARDS
===================================================== */

.status-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));

  gap: 12px;
  margin-bottom: 15px;
}

.status-card {
  padding: 15px;

  background: var(--surface);

  border: 1px solid var(--border);
  border-radius: 17px;
}

.status-top {
  display: flex;
  align-items: center;
  gap: 8px;

  color: var(--muted);
  font-size: 12px;
}

.dot {
  width: 8px;
  height: 8px;

  flex-shrink: 0;

  border-radius: 50%;
  background: var(--danger);
}

.dot.online {
  background: var(--accent);

  box-shadow: 0 0 9px var(--accent);
}

.status-value {
  display: block;
  margin-top: 9px;

  font-size: 14px;
  font-weight: bold;
}


/* =====================================================
   06. SECTION
===================================================== */

.section {
  margin-top: 24px;
}

.section-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;

  margin-bottom: 13px;
}

.section-heading h3 {
  margin: 0;

  font-size: 15px;
  font-weight: 700;
}

.section-heading small {
  color: var(--muted);
  font-size: 11px;
}


/* =====================================================
   07. STATISTICS
===================================================== */

.stat-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));

  gap: 10px;
}

.stat {
  padding: 15px 10px;

  background: var(--surface);

  border: 1px solid var(--border);
  border-radius: 15px;
}

.stat strong {
  display: block;

  font-size: 24px;
}

.stat span {
  display: block;
  margin-top: 6px;

  color: var(--muted);
  font-size: 10px;
}


/* =====================================================
   08. ROOM CONTROL
   MINIMALIST SMART HOME DESIGN
===================================================== */

.relay-grid {
  display: grid;

  grid-template-columns: repeat(2, minmax(0, 1fr));

  gap: 11px;
}

.relay-card {
  position: relative;

  min-width: 0;

  padding: 14px;

  background: var(--surface);

  border: 1px solid var(--border);
  border-radius: 15px;

  box-shadow: none;

  transition:
    background .2s ease,
    border-color .2s ease,
    transform .2s ease;
}

.relay-card.active {
  background: var(--surface);
  border-color: var(--accent);
}

/* DEVICE ICON */

.relay-icon {
  width: 34px;
  height: 34px;

  display: grid;
  place-items: center;

  margin-bottom: 12px;

  border-radius: 10px;

  background: var(--surface2);
  color: var(--muted);

  font-size: 18px;

  transition:
    background .2s ease,
    color .2s ease;
}

.relay-card.active .relay-icon {
  background: var(--accent-bg);
  color: var(--accent);
}

/* DEVICE NAME */

.relay-name {
  color: var(--text);

  font-size: 13px;
  font-weight: 600;
  letter-spacing: .1px;

  overflow-wrap: break-word;
}

.relay-description {
  margin-top: 5px;

  color: var(--muted);

  font-size: 9px;
  letter-spacing: 1px;
}

/* CONTROL AREA */

.relay-bottom {
  display: flex;
  align-items: center;
  justify-content: space-between;

  gap: 8px;

  margin-top: 15px;
  padding-top: 11px;

  border-top: 1px solid var(--border);
}

/* ON / OFF STATUS */

.state-label {
  color: var(--muted);

  font-size: 9px;
  font-weight: 700;
  letter-spacing: 1px;

  transition: color .2s ease;
}

.relay-card.active .state-label {
  color: var(--accent);
}

/* MINIMAL TOGGLE SWITCH */

.switch {
  position: relative;

  display: block;

  width: 38px;
  height: 22px;

  flex-shrink: 0;

  padding: 0;

  border: 0;
  border-radius: 30px;

  background: var(--surface2);

  transition: background .2s ease;
}

.switch::after {
  content: "";

  position: absolute;

  top: 2px;
  left: 2px;

  width: 18px;
  height: 18px;

  border-radius: 50%;

  background: var(--muted);

  transition:
    transform .2s ease,
    background .2s ease;
}

.switch.active {
  background: var(--accent-bg);
}

.switch.active::after {
  transform: translateX(16px);
  background: var(--accent);
}

.switch:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 3px;
}


/* =====================================================
   09. GENERAL PANEL
===================================================== */

.panel {
  padding: 18px;

  background: var(--surface);

  border: 1px solid var(--border);
  border-radius: 18px;
}

.panel h3 {
  margin: 0 0 15px;

  font-size: 15px;
}


/* =====================================================
   10. FORM CONTROLS
===================================================== */

.form-control {
  width: 100%;

  padding: 13px;
  margin-bottom: 10px;

  border: 1px solid var(--border);
  border-radius: 11px;

  outline: none;

  background: var(--bg);
  color: var(--text);

  transition: border-color .2s;
}

.form-control:focus {
  border-color: var(--accent);
}

.primary-button {
  width: 100%;

  padding: 13px;

  border: 0;
  border-radius: 11px;

  background: var(--accent-bg);
  color: var(--accent);

  font-weight: bold;

  transition: opacity .2s;
}

.primary-button:hover {
  opacity: .85;
}

.secondary-button {
  width: 100%;

  padding: 12px;
  margin-bottom: 10px;

  border: 1px solid var(--border);
  border-radius: 11px;

  background: var(--surface2);
  color: var(--text);
}


/* =====================================================
   11. WIFI NETWORK
===================================================== */

.network-list {
  margin-top: 10px;

  color: var(--muted);
  font-size: 12px;
}

.network-item {
  display: flex;
  align-items: center;
  justify-content: space-between;

  padding: 11px;
  margin-top: 7px;

  background: var(--surface2);

  border-radius: 10px;

  cursor: pointer;

  transition: outline .2s;
}

.network-item:hover {
  outline: 1px solid var(--accent);
}


/* =====================================================
   12. INFORMATION ROW
===================================================== */

.info-row {
  display: flex;
  align-items: center;
  justify-content: space-between;

  gap: 12px;

  padding: 12px 0;

  border-bottom: 1px solid var(--border);

  font-size: 12px;
}

.info-row:last-child {
  border-bottom: 0;
}

.info-row span {
  color: var(--muted);
}


/* =====================================================
   13. MESSAGE
===================================================== */

.message {
  display: none;

  padding: 12px;
  margin-top: 10px;

  background: var(--surface2);

  border-radius: 10px;

  font-size: 12px;
}


/* =====================================================
   14. FOOTER
===================================================== */

.footer {
  margin-top: 30px;

  color: var(--muted);

  text-align: center;

  font-size: 10px;
  letter-spacing: 1px;
}


/* =====================================================
   15. MOBILE RESPONSIVE
===================================================== */

@media (max-width: 380px) {

  body {
    padding: 12px 12px 25px;
  }

  .header {
    margin-bottom: 18px;
  }

  .brand {
    gap: 11px;
  }

  .brand-icon {
    width: 41px;
    height: 41px;

    font-size: 20px;
  }

  .brand strong {
    font-size: 17px;
  }

  .hero {
    padding: 16px;
    border-radius: 17px;
  }

  .clock {
    font-size: 30px;
  }

  .status-grid {
    gap: 9px;
  }

  .status-card {
    padding: 12px;
  }

  .relay-grid {
    gap: 8px;
  }

  .relay-card {
    padding: 12px;
    border-radius: 13px;
  }

  .relay-icon {
    width: 31px;
    height: 31px;

    margin-bottom: 10px;

    font-size: 16px;
  }

  .relay-name {
    font-size: 12px;
  }

  .relay-description {
    font-size: 8px;
  }

  .relay-bottom {
    margin-top: 12px;
    padding-top: 9px;
  }

  .state-label {
    font-size: 8px;
  }

  .section {
    margin-top: 20px;
  }

}


/* =====================================================
   16. EXTRA SMALL DEVICES
===================================================== */

@media (max-width: 320px) {

  .relay-grid {
    gap: 7px;
  }

  .relay-card {
    padding: 10px;
  }

  .relay-name {
    font-size: 11px;
  }

  .switch {
    width: 35px;
    height: 21px;
  }

  .switch::after {
    width: 17px;
    height: 17px;
  }

  .switch.active::after {
    transform: translateX(14px);
  }

}


/* =====================================================
   17. REDUCED MOTION
===================================================== */

@media (prefers-reduced-motion: reduce) {

  *,
  *::before,
  *::after {
    scroll-behavior: auto !important;
    transition-duration: .01ms !important;
    animation-duration: .01ms !important;
  }

}
</style>

</head>

<body>

<div class="container">

<!-- HEADER -->

<header class="header">

  <div class="brand">

    <div class="brand-icon">⌂</div>

    <div>
      <strong>NOVIA HOME</strong>
      <small>SMART HOME SYSTEM · V4.0</small>
    </div>

  </div>

  <button
    class="icon-button"
    id="themeButton"
    onclick="toggleTheme()"
    aria-label="Ganti tema">
    ☾
  </button>

</header>

<!-- CLOCK -->

<section class="hero">

  <div class="eyebrow">WELCOME HOME</div>

  <div class="clock" id="clock">--:--:--</div>

  <div class="date" id="date">Loading date...</div>

</section>

<!-- CONNECTION STATUS -->

<div class="status-grid">

  <div class="status-card">

    <div class="status-top">

      <span class="dot" id="wifiDot"></span>

      WIFI CONNECTION

    </div>

    <strong class="status-value" id="wifiStatus">
      CHECKING
    </strong>

  </div>

  <div class="status-card">

    <div class="status-top">

      <span class="dot" id="blynkDot"></span>

      BLYNK CLOUD

    </div>

    <strong class="status-value" id="blynkStatus">
      CHECKING
    </strong>

  </div>

</div>

<!-- STATISTICS -->

<section class="section">

  <div class="section-heading">

    <h3>HOME OVERVIEW</h3>

    <small>REAL-TIME STATUS</small>

  </div>

  <div class="stat-grid">

    <div class="stat">

      <strong id="activeCount">0</strong>

      <span>ACTIVE RELAY</span>

    </div>

    <div class="stat">

      <strong id="offCount">4</strong>

      <span>INACTIVE RELAY</span>

    </div>

    <div class="stat">

      <strong>4</strong>

      <span>TOTAL CHANNEL</span>

    </div>

  </div>

</section>

<!-- RELAY CONTROL -->

<section class="section">

  <div class="section-heading">

    <h3>ROOM CONTROL</h3>

    <small>4 CHANNELS</small>

  </div>

  <div class="relay-grid">

    <div class="relay-card" id="card0">

      <div class="relay-icon">✳</div>

      <div class="relay-name">Lampu Depan</div>

      <div class="relay-description">CHANNEL 01</div>

      <div class="relay-bottom">

        <span class="state-label" id="state0">OFF</span>

        <button
          class="switch"
          id="switch0"
          onclick="toggleRelay(0)"
          aria-label="Lampu Depan">
        </button>

      </div>

    </div>

    <div class="relay-card" id="card1">

      <div class="relay-icon">✳</div>

      <div class="relay-name">Lampu Tengah</div>

      <div class="relay-description">CHANNEL 02</div>

      <div class="relay-bottom">

        <span class="state-label" id="state1">OFF</span>

        <button
          class="switch"
          id="switch1"
          onclick="toggleRelay(1)"
          aria-label="Lampu Tengah">
        </button>

      </div>

    </div>

    <div class="relay-card" id="card2">

      <div class="relay-icon">✳</div>

      <div class="relay-name">Lampu Kamar</div>

      <div class="relay-description">CHANNEL 03</div>

      <div class="relay-bottom">

        <span class="state-label" id="state2">OFF</span>

        <button
          class="switch"
          id="switch2"
          onclick="toggleRelay(2)"
          aria-label="Lampu Kamar">
        </button>

      </div>

    </div>

    <div class="relay-card" id="card3">

      <div class="relay-icon">✳</div>

      <div class="relay-name">Kipas Angin</div>

      <div class="relay-description">CHANNEL 04</div>

      <div class="relay-bottom">

        <span class="state-label" id="state3">OFF</span>

        <button
          class="switch"
          id="switch3"
          onclick="toggleRelay(3)"
          aria-label="Kipas Angin">
        </button>

      </div>

    </div>

  </div>

</section>

<!-- WIFI MANAGEMENT -->

<section class="section">

  <div class="section-heading">

    <h3>NETWORK MANAGEMENT</h3>

    <small>WIFI SETTINGS</small>

  </div>

  <div class="panel">

    <h3>WiFi Scanner</h3>

    <button
      class="secondary-button"
      id="scanButton"
      onclick="scanWiFi()">
      SCAN AVAILABLE NETWORKS
    </button>

    <div class="network-list" id="networkList">
      Tekan tombol scan untuk mencari jaringan.
    </div>

    <form id="wifiForm">

      <input
        class="form-control"
        id="ssid"
        name="ssid"
        placeholder="Nama WiFi"
        maxlength="32"
        required>

      <input
        class="form-control"
        id="pass"
        name="pass"
        type="password"
        placeholder="Password WiFi"
        maxlength="63">

      <button
        class="primary-button"
        type="submit">
        SIMPAN KONFIGURASI WIFI
      </button>

    </form>

    <div class="message" id="wifiMessage"></div>

  </div>

</section>

<!-- DEVICE INFORMATION -->

<section class="section">

  <div class="section-heading">

    <h3>DEVICE INFORMATION</h3>

    <small>NOVIA V4</small>

  </div>

  <div class="panel">

    <div class="info-row">

      <span>Device</span>

      <strong>ESP32 DevKit V1</strong>

    </div>

    <div class="info-row">

      <span>Local IP</span>

      <strong id="deviceIP">-</strong>

    </div>

    <div class="info-row">

      <span>WiFi Signal</span>

      <strong id="wifiRSSI">-</strong>

    </div>

    <div class="info-row">

      <span>Access Point</span>

      <strong>NOVIA-HOME</strong>

    </div>

    <div class="info-row">

      <span>Firmware</span>

      <strong>V4.0 PREMIUM</strong>

    </div>

    <div class="info-row">

      <span>Storage</span>

      <strong>Preferences</strong>

    </div>

  </div>

</section>

<footer class="footer">

  NOVIA HOME V4.0 PREMIUM

  <br><br>

  SMART CONTROL · SMART LIVING

</footer>

</div>

<script>

// =================================================
// JAVASCRIPT CONFIGURATION
// =================================================

let states = [0,0,0,0];

let busy = [false,false,false,false];

let scanTimer = null;

// =================================================
// THEME MANAGEMENT
// =================================================

function setTheme(theme) {

  document.body.classList.toggle(
    'light',
    theme === 'light'
  );

  document.getElementById('themeButton').innerText =
    theme === 'light' ? '☀' : '☾';

  localStorage.setItem('noviaTheme',theme);

}

function toggleTheme() {

  const light =
    document.body.classList.contains('light');

  setTheme(light ? 'dark' : 'light');

}

// Restore saved theme.

setTheme(
  localStorage.getItem('noviaTheme') || 'dark'
);

// =================================================
// DIGITAL CLOCK
// =================================================

function updateClock() {

  const now = new Date();

  document.getElementById('clock').innerText =
    now.toLocaleTimeString('id-ID',{
      hour12:false
    });

  document.getElementById('date').innerText =
    now.toLocaleDateString('id-ID',{
      weekday:'long',
      day:'numeric',
      month:'long',
      year:'numeric'
    });

}

setInterval(updateClock,1000);

updateClock();

// =================================================
// RENDER RELAY
// =================================================

function renderRelays() {

  let active = 0;

  for(let i=0;i<4;i++) {

    const on = Number(states[i]) === 1;

    if(on) active++;

    document.getElementById('state'+i).innerText =
      on ? 'ON' : 'OFF';

    document.getElementById('switch'+i)
      .classList.toggle('active',on);

    document.getElementById('card'+i)
      .classList.toggle('active',on);

  }

  document.getElementById('activeCount').innerText =
    active;

  document.getElementById('offCount').innerText =
    4-active;

}

// =================================================
// UPDATE DEVICE STATE
// =================================================

async function updateStatus() {

  try {

    const response = await fetch(
      '/api/state?t='+Date.now(),
      {cache:'no-store'}
    );

    if(!response.ok) throw new Error();

    const data = await response.json();

    states = data.channels.map(Number);

    renderRelays();

    const wifi = data.wifi;

    const blynk = data.blynk;

    document.getElementById('wifiStatus').innerText =
      wifi ? 'CONNECTED' : 'DISCONNECTED';

    document.getElementById('blynkStatus').innerText =
      blynk ? 'CONNECTED' : 'DISCONNECTED';

    document.getElementById('wifiDot')
      .classList.toggle('online',wifi);

    document.getElementById('blynkDot')
      .classList.toggle('online',blynk);

    document.getElementById('deviceIP').innerText =
      data.ip;

    document.getElementById('wifiRSSI').innerText =
      wifi ? data.rssi+' dBm' : 'NOT CONNECTED';

  } catch(error) {

    document.getElementById('wifiStatus').innerText =
      'UNAVAILABLE';

    document.getElementById('blynkStatus').innerText =
      'UNKNOWN';

    document.getElementById('wifiDot')
      .classList.remove('online');

    document.getElementById('blynkDot')
      .classList.remove('online');

  }

}

// =================================================
// RELAY CONTROL
// =================================================

async function toggleRelay(ch) {

  if(busy[ch]) return;

  busy[ch] = true;

  const next = states[ch] ? 0 : 1;

  const button =
    document.getElementById('switch'+ch);

  button.disabled = true;

  try {

    const response = await fetch(
      '/api/set?ch='+(ch+1)+
      '&state='+next,
      {cache:'no-store'}
    );

    if(!response.ok) throw new Error();

    const result = await response.json();

    if(!result.ok) throw new Error();

    await updateStatus();

  } catch(error) {

    showMessage(
      'wifiMessage',
      'Kontrol gagal. Periksa koneksi perangkat.'
    );

  } finally {

    busy[ch] = false;

    button.disabled = false;

  }

}

// =================================================
// WIFI SCANNER
// =================================================

async function scanWiFi() {

  const button =
    document.getElementById('scanButton');

  const list =
    document.getElementById('networkList');

  if(scanTimer) {

    clearInterval(scanTimer);

    scanTimer = null;
  }

  button.disabled = true;

  button.innerText = 'SCANNING...';

  list.innerText = 'Mencari jaringan WiFi...';

  try {

    const response = await fetch(
      '/api/scan',
      {cache:'no-store'}
    );

    if(!response.ok) throw new Error();

    const result = await response.json();

    if(!result.ok) throw new Error();

    let attempts = 0;

    scanTimer = setInterval(async function() {

      attempts++;

      try {

        const r = await fetch(
          '/api/scan/status',
          {cache:'no-store'}
        );

        if(!r.ok) throw new Error();

        const data = await r.json();

        if(data.error) throw new Error();

        if(data.scanning) {

          if(attempts >= 30) {

            clearInterval(scanTimer);

            scanTimer = null;

            button.disabled = false;

            button.innerText = 'SCAN AVAILABLE NETWORKS';

            list.innerText = 'Scan timeout. Silakan coba lagi.';

          }

          return;
        }

        clearInterval(scanTimer);

        scanTimer = null;

        button.disabled = false;

        button.innerText = 'SCAN AVAILABLE NETWORKS';

        renderNetworks(data.networks || []);

      } catch(error) {

        clearInterval(scanTimer);

        scanTimer = null;

        button.disabled = false;

        button.innerText = 'SCAN AVAILABLE NETWORKS';

        list.innerText = 'Scan gagal. Coba kembali.';

      }

    },1000);

  } catch(error) {

    button.disabled = false;

    button.innerText = 'SCAN AVAILABLE NETWORKS';

    list.innerText =
      'Tidak dapat memulai scan WiFi.';

  }

}

// =================================================
// RENDER NETWORK LIST
// =================================================

function renderNetworks(networks) {

  const list =
    document.getElementById('networkList');

  list.innerHTML = '';

  if(networks.length === 0) {

    list.innerText = 'Tidak ada jaringan ditemukan.';

    return;
  }

  networks.sort((a,b)=>b.rssi-a.rssi);

  networks.forEach(function(network) {

    const item = document.createElement('div');

    item.className = 'network-item';

    const name = document.createElement('span');

    name.textContent = network.ssid || '(Hidden SSID)';

    const signal = document.createElement('span');

    signal.textContent =
      network.rssi+' dBm '+
      (network.secure ? '🔒' : 'OPEN');

    item.appendChild(name);

    item.appendChild(signal);

    item.onclick = function() {

      document.getElementById('ssid').value =
        network.ssid;

      document.getElementById('pass').focus();

    };

    list.appendChild(item);

  });

}

// =================================================
// WIFI CONFIGURATION SAVE
// =================================================

document.getElementById('wifiForm')
.addEventListener('submit',async function(event) {

  event.preventDefault();

  const ssid =
    document.getElementById('ssid').value.trim();

  const pass =
    document.getElementById('pass').value;

  if(!ssid || ssid.length > 32 || pass.length > 63) {

    showMessage(
      'wifiMessage',
      'Nama WiFi atau password tidak valid.'
    );

    return;
  }

  const formData = new URLSearchParams();

  formData.append('ssid',ssid);

  formData.append('pass',pass);

  try {

    const response = await fetch(
      '/savewifi',
      {
        method:'POST',
        headers:{
          'Content-Type':
          'application/x-www-form-urlencoded'
        },
        body:formData.toString()
      }
    );

    if(!response.ok) throw new Error();

    showMessage(
      'wifiMessage',
      'Konfigurasi tersimpan. ESP32 sedang mencoba koneksi WiFi baru.'
    );

    document.getElementById('pass').value = '';

  } catch(error) {

    showMessage(
      'wifiMessage',
      'Gagal menyimpan konfigurasi WiFi.'
    );

  }

});

// =================================================
// MESSAGE
// =================================================

function showMessage(id,message) {

  const element = document.getElementById(id);

  element.innerText = message;

  element.style.display = 'block';

}

// =================================================
// AUTO UPDATE
// =================================================

setInterval(updateStatus,2000);

updateStatus();

</script>

</body>

</html>

)NOVIAHTML";

// ============================================================
// 28. WEB ROOT HANDLER
// ============================================================

void handleRoot() {

  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );
}

// ============================================================
// 29. WEB SERVER ROUTES
// ============================================================

void setupWebServer() {

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/api/state",
    HTTP_GET,
    handleState
  );

  server.on(
    "/api/set",
    HTTP_GET,
    handleSetChannel
  );

  server.on(
    "/api/scan",
    HTTP_GET,
    handleScanStart
  );

  server.on(
    "/api/scan/status",
    HTTP_GET,
    handleScanStatus
  );

  server.on(
    "/savewifi",
    HTTP_POST,
    handleSaveWiFi
  );

  server.onNotFound([]() {

    server.send(
      404,
      "text/plain",
      "PAGE NOT FOUND"
    );

  });

  server.begin();

  Serial.println("[NOVIA] WEB SERVER STARTED");
}

// ============================================================
// 30. SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(300);

  Serial.println();

  Serial.println("==================================");
  Serial.println("       NOVIA HOME V4.0");
  Serial.println("     PREMIUM SMART HOME SYSTEM");
  Serial.println("==================================");

  // ------------------------------------------
  // INITIALIZE RELAY
  // ------------------------------------------

  for(uint8_t i=0;i<RELAY_COUNT;i++) {

    // Set output latch LOW before output mode.

    digitalWrite(relayPins[i],RELAY_OFF);

    pinMode(relayPins[i],OUTPUT);

    channelState[i] = false;
  }

  Serial.println("[NOVIA] RELAYS INITIALIZED OFF");

  // ------------------------------------------
  // INITIALIZE BUTTON
  // ------------------------------------------

  for(uint8_t i=0;i<RELAY_COUNT;i++) {

    pinMode(
      buttonPins[i],
      INPUT_PULLUP
    );

    buttonStable[i] = digitalRead(buttonPins[i]);

    buttonLast[i] = buttonStable[i];

    buttonTimer[i] = millis();
  }

  Serial.println("[NOVIA] BUTTONS INITIALIZED");

  // ------------------------------------------
  // LOAD WIFI CONFIGURATION
  // ------------------------------------------

  loadWiFiConfig();

  // ------------------------------------------
  // AP + STA MODE
  // ------------------------------------------

  WiFi.mode(WIFI_AP_STA);

  WiFi.setAutoReconnect(true);

  WiFi.softAP(
    AP_SSID,
    AP_PASS
  );

  Serial.print("[NOVIA] AP IP: ");

  Serial.println(WiFi.softAPIP());

  // ------------------------------------------
  // START WEB SERVER
  // ------------------------------------------

  setupWebServer();

  // ------------------------------------------
  // BLYNK CONFIGURATION
  // ------------------------------------------

  Blynk.config(BLYNK_AUTH_TOKEN);

  // ------------------------------------------
  // CONNECT WIFI
  // ------------------------------------------

  if(wifiConfigured) {

    connectWiFi();

  } else {

    Serial.println("[NOVIA] WIFI NOT CONFIGURED");
    Serial.println("[NOVIA] USE AP TO CONFIGURE WIFI");

  }

  Serial.println("==================================");
  Serial.println("       NOVIA HOME V4 READY");
  Serial.println("==================================");
}

// ============================================================
// 31. MAIN LOOP
// ============================================================

void loop() {

  // Web dashboard

  server.handleClient();

  // Physical buttons

  handleButtons();

  // WiFi manager

  handleWiFi();

  // Blynk manager

  handleBlynk();

  // Status serial monitoring

  if(millis()-lastStatusLog >= STATUS_INTERVAL) {

    lastStatusLog = millis();

    // Keep loop responsive.
  }

  yield();
}

// ============================================================
// END NOVIA HOME V4.0
// ============================================================
