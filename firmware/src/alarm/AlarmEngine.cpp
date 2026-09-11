#include "AlarmEngine.h"
#include <LittleFS.h>

static const char* kFile = "/alarms.json";

void AlarmEngine::begin() { load(); }

bool AlarmEngine::compare(const Rule& r, float v) const {
  if (strcmp(r.op, "gt") == 0) return v > r.threshold;
  if (strcmp(r.op, "lt") == 0) return v < r.threshold;
  if (strcmp(r.op, "ge") == 0) return v >= r.threshold;
  if (strcmp(r.op, "le") == 0) return v <= r.threshold;
  if (strcmp(r.op, "eq") == 0) return v == r.threshold;
  if (strcmp(r.op, "ne") == 0) return v != r.threshold;
  return false;
}

void AlarmEngine::evaluate(JsonObjectConst blocks) {
  for (Rule& r : rules_) {
    if (!r.used) continue;
    JsonVariantConst v = blocks[r.block][r.key];
    if (!v.is<float>()) continue;          // key missing or not numeric: skip
    bool now = compare(r, v.as<float>());
    if (now && !r.active) fire(r, v.as<float>());   // rising edge only
    r.active = now;
  }
}

void AlarmEngine::fire(Rule& r, float v) {
  r.fired++;
  Serial.printf("[alarm] rule %d: %s.%s %s %.3f (value %.3f) -> %s\n",
                r.id, r.block, r.key, r.op, r.threshold, v, r.action);

  // "relay:N:on" / "relay:N:off" -> send {"cmd":"relay","args":{"n":N,"on":..}} to b4
  if (strncmp(r.action, "relay:", 6) == 0) {
    int n = atoi(r.action + 6);
    const char* colon = strchr(r.action + 6, ':');
    bool on = colon && strcmp(colon + 1, "on") == 0;
    Block* b4 = reg_.find("b4");
    if (b4) {
      JsonDocument cmd, reply;
      cmd["cmd"] = "relay";
      cmd["args"]["n"] = n;
      cmd["args"]["on"] = on;
      b4->handle(cmd.as<JsonObjectConst>(), reply.to<JsonObject>());
    }
  }

  // Every firing is also announced to the clients (the PWA shows a toast).
  if (notify_) {
    JsonDocument doc;
    doc["type"] = "alarm";
    doc["t"] = millis();
    doc["rule"] = r.id;
    doc["block"] = r.block;
    doc["key"] = r.key;
    doc["value"] = v;
    doc["action"] = r.action;
    String s;
    serializeJson(doc, s);
    notify_(s);
  }
}

bool AlarmEngine::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);

  if (strcmp(c, "list") == 0) {
    JsonArray arr = reply["rules"].to<JsonArray>();
    for (const Rule& r : rules_) if (r.used) toJson(r, arr.add<JsonObject>());
    return true;
  }
  if (strcmp(c, "add") == 0) {
    // {"block":"b1","key":"ai1","op":"gt","threshold":9.0,"action":"relay:1:off"}
    if (!a["block"].is<const char*>() || !a["key"].is<const char*>() || !a["threshold"].is<float>()) {
      reply["error"] = "need block, key, threshold";
      return false;
    }
    for (Rule& r : rules_) {
      if (r.used) continue;
      r.used = true;
      r.id = nextId_++;
      strlcpy(r.block, a["block"], sizeof r.block);
      strlcpy(r.key, a["key"], sizeof r.key);
      strlcpy(r.op, a["op"] | "gt", sizeof r.op);
      r.threshold = a["threshold"].as<float>();
      strlcpy(r.action, a["action"] | "notify", sizeof r.action);
      r.active = false;
      r.fired = 0;
      save();
      toJson(r, reply);
      return true;
    }
    reply["error"] = "rule table full";
    return false;
  }
  if (strcmp(c, "remove") == 0) {          // {"id":3}
    int id = a["id"] | -1;
    for (Rule& r : rules_) {
      if (r.used && r.id == id) {
        r.used = false;
        save();
        reply["removed"] = id;
        return true;
      }
    }
    reply["error"] = "no such id";
    return false;
  }
  if (strcmp(c, "clear") == 0) {
    for (Rule& r : rules_) r.used = false;
    save();
    return true;
  }
  reply["error"] = "unknown cmd";
  return false;
}

void AlarmEngine::status(JsonObject out) {
  int n = 0, active = 0;
  for (const Rule& r : rules_) if (r.used) { n++; if (r.active) active++; }
  out["rules"] = n;
  out["active"] = active;
}

void AlarmEngine::toJson(const Rule& r, JsonObject out) const {
  out["id"] = r.id;
  out["block"] = r.block;
  out["key"] = r.key;
  out["op"] = r.op;
  out["threshold"] = r.threshold;
  out["action"] = r.action;
  out["active"] = r.active;
  out["fired"] = r.fired;
}

void AlarmEngine::save() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const Rule& r : rules_) if (r.used) toJson(r, arr.add<JsonObject>());
  File f = LittleFS.open(kFile, "w");
  if (!f) { Serial.println("[alarm] cannot write /alarms.json"); return; }
  serializeJson(doc, f);
  f.close();
}

void AlarmEngine::load() {
  File f = LittleFS.open(kFile, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
      JsonDocument cmd;
      cmd["cmd"] = "add";
      cmd["args"]["block"] = o["block"];
      cmd["args"]["key"] = o["key"];
      cmd["args"]["op"] = o["op"];
      cmd["args"]["threshold"] = o["threshold"];
      cmd["args"]["action"] = o["action"];
      JsonDocument reply;
      handle(cmd.as<JsonObjectConst>(), reply.to<JsonObject>());
    }
  }
  f.close();
  Serial.printf("[alarm] loaded %d rule(s)\n", nextId_ - 1);
}
