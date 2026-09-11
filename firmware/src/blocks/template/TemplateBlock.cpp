#include "TemplateBlock.h"
#include "../../../include/pins.h"

void TemplateBlock::begin() {
#ifdef SIM
  // Nothing to set up in sim mode.
#else
  // TODO: pinMode(...) / SPI.begin(...) for your block's pins (see pins.h).
#endif
}

void TemplateBlock::loop() {
  // Poll your hardware at a sensible rate. 10 Hz is plenty for a status value.
  uint32_t now = millis();
  if (now - lastTick_ < 100) return;
  lastTick_ = now;

#ifdef SIM
  // SIM: pretend the hardware follows the setpoint with a little noise.
  value_ += (setpoint_ - value_) * 0.2f + (random(-100, 100) / 1000.0f);
#else
  // TODO: read the real value from your hardware.
#endif
}

bool TemplateBlock::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);

  // --- one example command: {"cmd":"set_value","args":{"value":2.5}} ---
  if (strcmp(c, "set_value") == 0) {
    if (!a["value"].is<float>()) { reply["error"] = "args.value (number) required"; return false; }
    setpoint_ = a["value"].as<float>();
#ifndef SIM
    // TODO: write setpoint_ to the hardware.
#endif
    reply["setpoint"] = setpoint_;
    return true;
  }
  // TODO: add your block's commands here (B1 read_all/set_range, B3 set_dc/sine, ...)

  reply["error"] = "unknown cmd";
  return false;
}

void TemplateBlock::status(JsonObject out) {
  // --- one example status value; numeric keys can be charted and alarmed ---
  out["value"] = value_;
  out["setpoint"] = setpoint_;
  // TODO: add your block's status values.
}

// --- Alarm hook -----------------------------------------------------------
// Alarm rules are data, not code: add one from the "Alarms" panel or with
//   instrument alarms add --block template --key value --op gt --threshold 5 --action relay:1:on
// which sends {"block":"alarms","cmd":"add","args":{...}}. Each block owner
// adds one rule that makes sense for their block and shows it at the demo
// (e.g. B1: "ai1 > 9 V -> relay 1 off", B2: "v3v3 < 3.0 -> notify").
