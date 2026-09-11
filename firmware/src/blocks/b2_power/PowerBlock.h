// b2_power: USB-C / 5 V jack -> +5V_RAW, AMS1117 -> +3V3, two isolated
// DC-DC modules -> +-12 V, 78L05 -> +5VA. Rail LEDs and test points.
// Commands: rails    Status: v5_raw, v3v3, v12p, v12n, v5a, measured
//
// NOTE: board v0.6 has no rail sensing wired to the MCU, so on real hardware
// this block reports the nominal design values with measured=false. If the
// instructor adds a sense path (e.g. ADS8688 ch7/8 or an ESP ADC pin), fill
// in the #ifndef SIM branch in the .cpp.
#pragma once
#include "../Block.h"

class PowerBlock : public Block {
 public:
  const char* name() const override { return "b2"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  float v5raw_ = 5.0f, v3v3_ = 3.3f, v12p_ = 12.0f, v12n_ = -12.0f, v5a_ = 5.0f;
  bool measured_ = false;
  uint32_t lastTick_ = 0;
};
