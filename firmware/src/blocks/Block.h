// The block-driver interface. Every hardware block (base, b1..b5, template)
// is one class that implements these five methods and nothing else.
//
// Rules that keep the system simple:
//   * All block code runs from loop() on the main task. No locks needed.
//   * A block never talks to another block directly; the phone / Python
//     client orchestrates, and the alarm engine can trigger relays.
//   * Under -DSIM=1 the block fakes its hardware so the whole app works on
//     a bare dev board. Real-hardware code goes in #ifndef SIM branches.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class Block {
 public:
  virtual ~Block() = default;

  // Short lowercase id used in every message: "base", "b1", "b4", ...
  virtual const char* name() const = 0;

  // Called once from setup(). Configure pins / SPI / state here.
  virtual void begin() = 0;

  // Called every loop() pass. Must return quickly (no delay()).
  virtual void loop() = 0;

  // Handle one command from a client.
  //   cmd   = {"cmd":"led","args":{"r":255,"g":0,"b":0}}
  //   reply = object to fill with the result (becomes "result" in the reply).
  // Return false for an unknown command or bad arguments; put a human
  // readable message in reply["error"].
  virtual bool handle(JsonObjectConst cmd, JsonObject reply) = 0;

  // Fill `out` with the block's current status values. Called 20x per
  // second for the broadcast; keep it to a handful of numbers/strings.
  // Numeric top-level keys are what the live chart and alarm rules can use.
  virtual void status(JsonObject out) = 0;
};

// Small helper shared by all blocks: fetch args object (may be null).
inline JsonObjectConst argsOf(JsonObjectConst cmd) { return cmd["args"].as<JsonObjectConst>(); }
