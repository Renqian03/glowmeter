// "base" block: the things every board has, even a bare dev board.
//   * RGB status LED (WS2812 on GPIO48) - colour + brightness from the phone
//   * a free-running counter and the chip temperature as a "signal" so the
//     live chart has something to draw in Workshop 1
//   * uptime, WiFi RSSI, free heap
// Project 2 (Workshop 1) may add one command + one live measurement here
// (workbook ch. 1, section 1.5, "Measure something"). Default: a ring buffer of ADC_MAX_N = 4096 uint16_t
// samples (mV) filled at 1 kHz from GPIO 4 in loop() - sampling only, no
// statistics there; command set_avg {n}, n in 1..1024; statistics computed in
// status() at 20 Hz: adc_v = mean of the most recent n samples (V), adc_sd =
// standard deviation of the block means (split the 4096 samples into
// floor(4096/n) consecutive blocks of n, take each block's mean, take the sd
// of those means, V) = "the noise of one n-sample average", avg_n = n.
// The LED 'press' button + 'presses' counter is the minimal fallback.
// Private members the recipe adds:
//   static constexpr int ADC_PIN = 4, ADC_MAX_N = 4096;
//   uint16_t adcBuf_[ADC_MAX_N]; int adcHead_ = 0, adcN_ = 16; uint32_t lastSample_ = 0;
#pragma once
#include <Adafruit_NeoPixel.h>
#include "../Block.h"

class BaseBlock : public Block {
 public:
  const char* name() const override { return "base"; }
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

  // Used by main.cpp to show WiFi state on the LED before a client connects.
  void setLed(uint8_t r, uint8_t g, uint8_t b);

 private:
  void showLed();

  Adafruit_NeoPixel pixel_{1, /*pin*/ 48, NEO_GRB + NEO_KHZ800};
  uint8_t r_ = 0, g_ = 0, b_ = 40;   // boot colour: dim blue
  uint8_t brightness_ = 40;          // 0..255
  uint32_t counter_ = 0;             // increments 10x per second
  uint32_t lastTick_ = 0;
  float tempC_ = 0;
};
