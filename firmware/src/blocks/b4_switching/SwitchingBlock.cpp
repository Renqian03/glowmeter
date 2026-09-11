#include "SwitchingBlock.h"
#include "../../../include/pins.h"

void SwitchingBlock::begin() {
#ifndef SIM
  for (int i = 0; i < 4; i++) { pinMode(PIN_RELAY[i], OUTPUT); digitalWrite(PIN_RELAY[i], LOW); }
  for (int i = 0; i < 2; i++) pinMode(PIN_OPTO_IN[i], INPUT);   // 1 k pull-up on the board
  // TODO(B4): attachInterrupt() on PIN_OPTO_IN[] (FALLING) to count edges
  // faster than loop() can poll; keep the ISR to a volatile counter++.
#endif
}

void SwitchingBlock::loop() {
  uint32_t now = millis();
  if (now - lastTick_ < 100) return;
  lastTick_ = now;
#ifdef SIM
  // SIM: opto 1 sees a 2 Hz square wave, opto 2 sees nothing.
  bool level = ((now / 250) % 2) == 0;
  if (level != optoLevel_[0]) { optoLevel_[0] = level; if (!level) optoCount_[0]++; }
#else
  for (int i = 0; i < 2; i++) {
    bool level = digitalRead(PIN_OPTO_IN[i]);
    if (level != optoLevel_[i]) { optoLevel_[i] = level; if (!level) optoCount_[i]++; }
  }
#endif
}

void SwitchingBlock::setRelay(int idx, bool on) {
  relay_[idx] = on;
#ifndef SIM
  digitalWrite(PIN_RELAY[idx], on ? HIGH : LOW);   // ULN2003 input high = coil on
#endif
}

bool SwitchingBlock::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);

  if (strcmp(c, "relay") == 0) {          // {"n":1..4,"on":true}
    int n = a["n"] | 0;
    if (n < 1 || n > 4) { reply["error"] = "n must be 1..4"; return false; }
    setRelay(n - 1, a["on"] | false);
    reply["n"] = n; reply["on"] = relay_[n - 1];
    return true;
  }
  if (strcmp(c, "relay_all") == 0) {      // {"on":false}
    for (int i = 0; i < 4; i++) setRelay(i, a["on"] | false);
    reply["on"] = a["on"] | false;
    return true;
  }
  if (strcmp(c, "opto_reset") == 0) {
    optoCount_[0] = optoCount_[1] = 0;
    return true;
  }
  reply["error"] = "unknown cmd";
  return false;
}

void SwitchingBlock::status(JsonObject out) {
  out["relay1"] = relay_[0]; out["relay2"] = relay_[1];
  out["relay3"] = relay_[2]; out["relay4"] = relay_[3];
  out["opto1"] = optoCount_[0]; out["opto2"] = optoCount_[1];
  out["opto1_level"] = optoLevel_[0]; out["opto2_level"] = optoLevel_[1];
}
