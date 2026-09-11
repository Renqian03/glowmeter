// b4_switching: 4 relays (ULN2003 on PIN_RELAY[0..3]) and 2 isolated fast
// inputs (6N137 on PIN_OPTO_IN[0..1], active low on the 3V3 side).
// Commands: relay {n, on}, relay_all {on}, opto_reset
// Status:   relay1..relay4 (0/1), opto1, opto2 (edge counts), opto1_level, opto2_level
// The alarm engine's "relay" action calls handle() with cmd "relay".
#pragma once
#include "../Block.h"

class SwitchingBlock : public Block {
 public:
  const char* name() const override { return "b4"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  void setRelay(int idx, bool on);

  bool relay_[4] = {};
  uint32_t optoCount_[2] = {};
  bool optoLevel_[2] = {};
  uint32_t lastTick_ = 0;
};
