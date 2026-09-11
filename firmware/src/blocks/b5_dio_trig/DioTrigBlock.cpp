#include "DioTrigBlock.h"
#include "../../../include/pins.h"

void DioTrigBlock::begin() {
#ifndef SIM
  for (int i = 0; i < 8; i++) pinMode(PIN_DIO[i], OUTPUT);
  pinMode(PIN_TRIG_DIR, OUTPUT);
  digitalWrite(PIN_TRIG_DIR, LOW);      // safe default: TRIG SMA is an input
  pinMode(PIN_TRIG_IO, INPUT);
  writeDio();
  // TODO(B5): fast outputs - use the LEDC peripheral (Arduino core 2.x:
  // ledcSetup + ledcAttachPin + ledcWriteTone) or RMT on PIN_FAST_OUT[]
  // for clocks up to the 74HCT125's limit.
#endif
}

void DioTrigBlock::loop() {
#ifndef SIM
  if (!trigOut_) trigLevel_ = digitalRead(PIN_TRIG_IO);
#endif
}

void DioTrigBlock::writeDio() {
#ifndef SIM
  for (int i = 0; i < 8; i++) digitalWrite(PIN_DIO[i], (mask_ >> i) & 1);
#endif
}

void DioTrigBlock::setFast(int idx, float hz) {
  fastHz_[idx] = hz;
#ifndef SIM
  // TODO(B5): start/stop the clock on PIN_FAST_OUT[idx] at `hz` (0 = stop).
#endif
}

bool DioTrigBlock::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);

  if (strcmp(c, "dio") == 0) {            // {"n":1..8,"level":true}
    int n = a["n"] | 0;
    if (n < 1 || n > 8) { reply["error"] = "n must be 1..8"; return false; }
    if (a["level"] | false) mask_ |= 1 << (n - 1); else mask_ &= ~(1 << (n - 1));
    writeDio();
    reply["mask"] = mask_;
    return true;
  }
  if (strcmp(c, "dio_mask") == 0) {       // {"mask":0..255}
    mask_ = a["mask"] | 0;
    writeDio();
    reply["mask"] = mask_;
    return true;
  }
  if (strcmp(c, "trig_dir") == 0) {       // {"out":true}
    trigOut_ = a["out"] | false;
#ifndef SIM
    pinMode(PIN_TRIG_IO, trigOut_ ? OUTPUT : INPUT);
    if (trigOut_) digitalWrite(PIN_TRIG_IO, trigLevel_);
    digitalWrite(PIN_TRIG_DIR, trigOut_);  // set direction after the pin mode
#endif
    reply["out"] = trigOut_;
    return true;
  }
  if (strcmp(c, "trig") == 0) {           // {"level":true} - only when trig_dir out
    if (!trigOut_) { reply["error"] = "TRIG is an input; send trig_dir out=true first"; return false; }
    trigLevel_ = a["level"] | false;
#ifndef SIM
    digitalWrite(PIN_TRIG_IO, trigLevel_);
#endif
    reply["level"] = trigLevel_;
    return true;
  }
  if (strcmp(c, "fast_out") == 0) {       // {"n":1..2,"freq_hz":1000}
    int n = a["n"] | 0;
    if (n < 1 || n > 2) { reply["error"] = "n must be 1 or 2"; return false; }
    setFast(n - 1, constrain(a["freq_hz"] | 0.0f, 0.0f, 40e6f));
    reply["n"] = n; reply["freq_hz"] = fastHz_[n - 1];
    return true;
  }
  reply["error"] = "unknown cmd";
  return false;
}

void DioTrigBlock::status(JsonObject out) {
  out["dio"] = mask_;
  char key[5] = "dio1";
  for (int i = 0; i < 8; i++) { key[3] = '1' + i; out[key] = (mask_ >> i) & 1; }
  out["trig_dir"] = trigOut_;
  out["trig"] = trigLevel_;
  out["fast1_hz"] = fastHz_[0]; out["fast2_hz"] = fastHz_[1];
}
