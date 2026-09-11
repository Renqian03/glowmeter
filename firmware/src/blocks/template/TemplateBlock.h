// TEMPLATE BLOCK - copy this folder to blocks/b<N>_<name>/ and rename.
//
// Checklist (the tutor walks you through it, in sim mode):
//   1. Copy  blocks/template/  ->  blocks/b3_outputs/  (your block)
//   2. Rename the class (TemplateBlock -> OutputsBlock) and name() ("b3")
//   3. Replace the example command and status value with your block's
//   4. Write the SIM branch first (plausible fake values), flash, see it
//      in your panel (host/pwa/panels/b3.js), then fill the real branch
//   5. Register it in main.cpp:  static OutputsBlock b3;  registry.add(&b3);
//   6. Add one alarm rule that makes sense for your block (see .cpp)
#pragma once
#include "../Block.h"

class TemplateBlock : public Block {
 public:
  const char* name() const override { return "template"; }   // TODO: "b<N>"
  void begin() override;
  void loop() override;
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

 private:
  // TODO: your block's state. Keep it to plain numbers/bools.
  float setpoint_ = 0;        // what the user asked for (example command)
  float value_ = 0;           // what the hardware reports (example status)
  uint32_t lastTick_ = 0;
};
