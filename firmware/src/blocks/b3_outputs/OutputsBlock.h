// b3_outputs: DAC8563 dual 16-bit DAC over SPI (SYNC = PIN_CS_DAC) followed
// by an OPA2192 stage: 0..5 V from the DAC -> -10..+10 V on AO1, AO2.
// Commands: set_dc {ch, volts}, sine {ch, freq, amp, offset}, off {ch}
// Status:   ao1, ao2 (volts now), mode1, mode2 ("off"|"dc"|"sine")
#pragma once
#include "../Block.h"

class OutputsBlock : public Block {
 public:
  const char* name() const override { return "b3"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  enum Mode { OFF, DC, SINE };
  struct Channel {
    Mode mode = OFF;
    float volts = 0;              // current output
    float dc = 0, freq = 1, amp = 1, offset = 0;
  };
  void writeDac(int ch, float volts);

  Channel ch_[2];
  uint32_t lastTick_ = 0;
};
