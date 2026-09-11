// b1_inputs: ADS8688 8-channel 16-bit ADC over SPI (CS = PIN_CS_ADC),
// +-10 V inputs AI1..AI8 on the front-panel SMAs.
// Commands: read_all, set_range {ch, range}   Status: ai1..ai8 (volts), range
#pragma once
#include "../Block.h"

class InputsBlock : public Block {
 public:
  const char* name() const override { return "b1"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  static constexpr int kChannels = 8;
  void readAll();

  float volts_[kChannels] = {};
  // ADS8688 range codes: 0 = +-10 V (2.5*Vref), 1 = +-5 V, 2 = +-2.5 V,
  // 5 = 0..10 V, 6 = 0..5 V (datasheet, "Range Select Registers").
  uint8_t range_[kChannels] = {};
  uint32_t lastTick_ = 0;
};
