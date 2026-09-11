#include "OutputsBlock.h"
#include "../../../include/pins.h"
#ifndef SIM
#include <SPI.h>
#endif

static const char* modeName(int m) { return m == 0 ? "off" : (m == 1 ? "dc" : "sine"); }

void OutputsBlock::begin() {
#ifndef SIM
  pinMode(PIN_CS_DAC, OUTPUT);
  digitalWrite(PIN_CS_DAC, HIGH);
  SPI.begin(PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);
  // TODO(B3): DAC8563 init - enable the internal 2.5 V reference and set
  // both channels to mid-scale (= 0 V after the +-10 V stage).
#endif
}

void OutputsBlock::loop() {
  // The sine is synthesised here at ~1 kHz update rate: fine for the demo
  // (waveform out, seen on the own input). A real AWG would use timer + DMA.
  uint32_t now = millis();
  if (now == lastTick_) return;
  lastTick_ = now;
  float t = now / 1000.0f;
  for (int i = 0; i < 2; i++) {
    Channel& c = ch_[i];
    switch (c.mode) {
      case OFF:  c.volts = 0; break;
      case DC:   c.volts = c.dc; break;
      case SINE: c.volts = c.offset + c.amp * sinf(2 * PI * c.freq * t); break;
    }
    writeDac(i, c.volts);
  }
}

void OutputsBlock::writeDac(int ch, float volts) {
  volts = constrain(volts, -10.0f, 10.0f);
  (void)ch;
#ifndef SIM
  // TODO(B3): the stage gives AO = 4 * (DAC - VREF), VREF = 2.5 V,
  // so DAC volts = 2.5 + AO/4, code = DAC_volts / 5.0 * 65535.
  // 24-bit SPI frame: command 0x18 | ch (write and update), then the code.
#endif
}

bool OutputsBlock::handle(JsonObjectConst cmd, JsonObject reply) {
  const char* c = cmd["cmd"] | "";
  JsonObjectConst a = argsOf(cmd);
  int ch = a["ch"] | 1;
  if (ch < 1 || ch > 2) {
    reply["error"] = "ch must be 1 or 2";
    return false;
  }
  Channel& x = ch_[ch - 1];

  if (strcmp(c, "set_dc") == 0) {          // {"ch":1,"volts":2.5}
    x.dc = constrain(a["volts"] | 0.0f, -10.0f, 10.0f);
    x.mode = DC;
  } else if (strcmp(c, "sine") == 0) {     // {"ch":1,"freq":10,"amp":5,"offset":0}
    x.freq = constrain(a["freq"] | 1.0f, 0.01f, 50000.0f);
    x.amp = constrain(a["amp"] | 1.0f, 0.0f, 10.0f);
    x.offset = constrain(a["offset"] | 0.0f, -10.0f, 10.0f);
    x.mode = SINE;
  } else if (strcmp(c, "off") == 0) {
    x.mode = OFF;
  } else {
    reply["error"] = "unknown cmd";
    return false;
  }
  reply["ch"] = ch;
  reply["mode"] = modeName(x.mode);
  return true;
}

void OutputsBlock::status(JsonObject out) {
  out["ao1"] = ch_[0].volts;
  out["ao2"] = ch_[1].volts;
  out["mode1"] = modeName(ch_[0].mode);
  out["mode2"] = modeName(ch_[1].mode);
}
