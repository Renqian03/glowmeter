// b5_dio_trig: 8 TTL outputs (74AHCT541 on PIN_DIO[0..7]), 2 fast outputs
// (PIN_FAST_OUT[0..1] -> 74HCT125) and the switchable TRIG SMA
// (74LVC1T45: PIN_TRIG_IO data, PIN_TRIG_DIR 1 = output to SMA).
// Commands: dio {n, level}, dio_mask {mask}, trig_dir {out}, trig {level},
//           fast_out {n, freq_hz}   (0 = off)
// Status:   dio (mask), dio1..dio8, trig_dir, trig, fast1_hz, fast2_hz
#pragma once
#include "../Block.h"

class DioTrigBlock : public Block {
 public:
  const char* name() const override { return "b5"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  void writeDio();
  void setFast(int idx, float hz);

  uint8_t mask_ = 0;        // bit n-1 = DIO n
  bool trigOut_ = false;    // TRIG_DIR
  bool trigLevel_ = false;  // driven level (output) or read level (input)
  float fastHz_[2] = {};
};
