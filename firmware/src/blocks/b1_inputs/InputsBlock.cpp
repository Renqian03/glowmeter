#include "InputsBlock.h"
#include "../../../include/pins.h"
#ifndef SIM
#include <SPI.h>
#endif

void InputsBlock::begin() {
#ifndef SIM
  pinMode(PIN_CS_ADC, OUTPUT);
  digitalWrite(PIN_CS_ADC, HIGH);
  SPI.begin(PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);
  // TODO(B1): ADS8688 init - /RST and /PD are pulled high on the board.
  // Send AUTO_RST (0xA000), then program the range register of each channel.
#endif
}

void InputsBlock::loop() {
  uint32_t now = millis();
  if (now - lastTick_ < 50) return;   // 20 Hz is enough for the status panel
  lastTick_ = now;
  readAll();
}

void InputsBlock::readAll() {
#ifdef SIM
  // SIM: ch1 = 1 Hz sine 8 Vpp, ch2 = 0.5 Hz triangle, others = offsets + noise.
  float t = millis() / 1000.0f;
  volts_[0] = 4.0f * sinf(2 * PI * 1.0f * t);
  volts_[1] = 5.0f * (2 * fabsf(fmodf(t, 2.0f) - 1.0f) - 1.0f);
  for (int i = 2; i < kChannels; i++) volts_[i] = 0.1f * i + random(-50, 50) / 10000.0f;
#else
  // TODO(B1): for each channel send MAN_CH_n (0xC000 + n*0x0400) and read the
  // 16-bit result in the following frame; convert with the channel's range:
  //   +-10 V range: v = (code - 32768) * 20.0 / 65536
  for (int i = 0; i < kChannels; i++) volts_[i] = 0;
#endif
}

bool InputsBlock::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);

  if (strcmp(c, "read_all") == 0) {
    readAll();
    JsonArray ai = reply["ai"].to<JsonArray>();
    for (int i = 0; i < kChannels; i++) ai.add(volts_[i]);
    return true;
  }
  if (strcmp(c, "set_range") == 0) {     // {"ch":1..8,"range":0..6}
    int ch = a["ch"] | 0;
    int range = a["range"] | -1;
    if (ch < 1 || ch > kChannels || range < 0 || range > 6) {
      reply["error"] = "ch 1..8, range 0..6";
      return false;
    }
    range_[ch - 1] = range;
#ifndef SIM
    // TODO(B1): write the range register (0x05 + ch-1) for this channel over SPI.
#endif
    reply["ch"] = ch;
    reply["range"] = range;
    return true;
  }
  reply["error"] = "unknown cmd";
  return false;
}

void InputsBlock::status(JsonObject out) {
  char key[4] = "ai1";
  for (int i = 0; i < kChannels; i++) {
    key[2] = '1' + i;
    out[key] = volts_[i];
  }
  out["range"] = range_[0];   // range of ch1 (the panel shows one selector)
}
