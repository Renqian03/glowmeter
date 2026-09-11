// Class-board instrument firmware - entry point.
//
// What happens here, in order:
//   1. Serial (native USB), LittleFS (holds the web app + alarm rules)
//   2. WiFi: join the network from include/secrets.h, else open our own AP
//   3. mDNS "instrument.local", OTA (ArduinoOTA), HTTP server for the PWA,
//      WebSocket at /ws for commands + the 20 Hz status broadcast
//   4. Register the blocks; loop() runs them and the broadcast.
//
// Everything block-specific lives in src/blocks/<name>/. Students normally
// only touch their own block folder and its panel; this file changes only
// when a new block is registered (one line).
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <mutex>
#include <vector>

#include "blocks/Registry.h"
#include "blocks/base/BaseBlock.h"
#include "blocks/template/TemplateBlock.h"
#include "blocks/b1_inputs/InputsBlock.h"
#include "blocks/b2_power/PowerBlock.h"
#include "blocks/b3_outputs/OutputsBlock.h"
#include "blocks/b4_switching/SwitchingBlock.h"
#include "blocks/b5_dio_trig/DioTrigBlock.h"
#include "alarm/AlarmEngine.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_SECRETS 1
#endif

static const char* kApPassword = "instrument";
// Hostname = "instrument-XXXX" (last 4 hex digits of the MAC), the same name as the access point,
// so six boards on one lab network do not collide on mDNS / OTA. Filled in by hostName().
static String gHostname;
static const char* hostName() {
  if (gHostname.isEmpty()) {
    String mac = WiFi.macAddress();            // "AA:BB:CC:DD:EE:FF"
    gHostname = "instrument-" + mac.substring(12, 14) + mac.substring(15, 17);
  }
  return gHostname.c_str();
}
static const uint32_t kStatusPeriodMs = 50;   // 20 Hz

// ---- blocks --------------------------------------------------------------
static Registry registry;
static BaseBlock base;
static TemplateBlock tpl;
static InputsBlock b1;
static PowerBlock b2;
static OutputsBlock b3;
static SwitchingBlock b4;
static DioTrigBlock b5;
static AlarmEngine alarms(registry);

// ---- network -------------------------------------------------------------
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Incoming WebSocket messages arrive on the async-TCP task. We queue them and
// handle them in loop(), so all block code runs on one task and blocks never
// need locks. (Small teaching point: concurrency is solved by not having any.)
struct Pending { uint32_t client; String text; };
static std::vector<Pending> pending;
static std::mutex pendingMutex;

static void onWsEvent(AsyncWebSocket* s, AsyncWebSocketClient* client, AwsEventType type,
                      void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[ws] client %u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Tell the new client which blocks exist so it can build its tabs.
    JsonDocument doc;
    doc["type"] = "hello";
    doc["fw"] = FW_VERSION;
#ifdef SIM
    doc["sim"] = true;
#else
    doc["sim"] = false;
#endif
    registry.names(doc["blocks"].to<JsonArray>());
    String out;
    serializeJson(doc, out);
    client->text(out);
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[ws] client %u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    // Only complete, single-frame text messages; our messages are small.
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      std::lock_guard<std::mutex> lock(pendingMutex);
      if (pending.size() < 32) pending.push_back({client->id(), String((const char*)data, len)});
    }
  }
}

// Handle one request: {"id":1,"block":"base","cmd":"led","args":{...}}
// Reply:              {"id":1,"ok":true,"result":{...}} or {"id":1,"ok":false,"error":"..."}
static void handleRequest(uint32_t clientId, const String& text) {
  JsonDocument req, res;
  DeserializationError err = deserializeJson(req, text);
  res["id"] = req["id"];
  if (err) {
    res["ok"] = false;
    res["error"] = String("bad json: ") + err.c_str();
  } else {
    const char* blockName = req["block"] | "";
    Block* block = registry.find(blockName);
    if (!block) {
      res["ok"] = false;
      res["error"] = String("unknown block: ") + blockName;
    } else {
      JsonObject result = res["result"].to<JsonObject>();
      bool ok = block->handle(req.as<JsonObjectConst>(), result);
      res["ok"] = ok;
      if (!ok) {
        res["error"] = result["error"] | "error";
        res.remove("result");
      }
    }
  }
  String out;
  serializeJson(res, out);
  AsyncWebSocketClient* c = ws.client(clientId);
  if (c) c->text(out);
}

static void broadcastStatus() {
  JsonDocument doc;
  doc["type"] = "status";
  doc["t"] = millis();
  JsonObject blocks = doc["blocks"].to<JsonObject>();
  registry.statusAll(blocks);
  alarms.evaluate(blocks);              // rules see exactly what the clients see
  if (ws.count() == 0) return;          // nobody listening: skip serialisation
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

static void startWiFi() {
  WiFi.setHostname(hostName());
#ifdef HAVE_SECRETS
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] joining %s", WIFI_SSID);
  base.setLed(40, 40, 0);   // yellow = connecting
  for (int i = 0; i < 100 && WiFi.status() != WL_CONNECTED; i++) {   // 10 s
    delay(100);
    if (i % 10 == 0) Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] STA %s  http://%s.local/\n", WiFi.localIP().toString().c_str(), hostName());
    base.setLed(0, 40, 0);  // green = on the lab network
    return;
  }
  Serial.println("[wifi] STA failed, falling back to AP");
#endif
  // Access point "instrument-XXXX" (last 4 hex digits of the MAC), 192.168.4.1.
  String mac = WiFi.macAddress();       // "AA:BB:CC:DD:EE:FF"
  String suffix = mac.substring(12, 14) + mac.substring(15, 17);
  String ssid = "instrument-" + suffix;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), kApPassword);
  Serial.printf("[wifi] AP \"%s\" password \"%s\"  http://%s/\n",
                ssid.c_str(), kApPassword, WiFi.softAPIP().toString().c_str());
  base.setLed(0, 0, 40);    // blue = own access point
}

static void startServer() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Plain-JSON info for scripts that do not want a WebSocket.
  server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["fw"] = FW_VERSION;
#ifdef SIM
    doc["sim"] = true;
#else
    doc["sim"] = false;
#endif
    doc["mac"] = WiFi.macAddress();
    registry.names(doc["blocks"].to<JsonArray>());
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // The web app: everything under firmware/data/ (copied from host/pwa/).
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache");
  server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("[http] server started");
}

void setup() {
  Serial.begin(115200);
  delay(300);   // give USB-CDC a moment so the first lines are not lost
  Serial.printf("\n[boot] class-board firmware %s%s\n", FW_VERSION,
#ifdef SIM
                " (SIM)"
#else
                ""
#endif
  );

  if (!LittleFS.begin(true)) Serial.println("[fs] LittleFS mount failed (run: pio run -t uploadfs)");

  // Register blocks. Adding a block = one line here + one panel file.
  registry.add(&base);
  registry.add(&b1);
  registry.add(&b2);
  registry.add(&b3);
  registry.add(&b4);
  registry.add(&b5);
  registry.add(&tpl);      // the copy-me example; remove once every block exists
  registry.add(&alarms);
  registry.beginAll();

  startWiFi();

  if (MDNS.begin(hostName())) MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(hostName());
  ArduinoOTA.begin();

  startServer();
}

void loop() {
  registry.loopAll();
  ArduinoOTA.handle();

  // Commands queued by the WebSocket task.
  std::vector<Pending> batch;
  {
    std::lock_guard<std::mutex> lock(pendingMutex);
    batch.swap(pending);
  }
  for (const Pending& p : batch) handleRequest(p.client, p.text);

  static uint32_t lastStatus = 0;
  uint32_t now = millis();
  if (now - lastStatus >= kStatusPeriodMs) {
    lastStatus = now;
    ws.cleanupClients();
    broadcastStatus();
  }
}
