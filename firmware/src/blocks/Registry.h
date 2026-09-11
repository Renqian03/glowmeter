// A fixed list of the registered blocks. main.cpp adds them in setup();
// the WebSocket handler and the status broadcast walk the list.
#pragma once
#include "Block.h"

class Registry {
 public:
  static constexpr int kMaxBlocks = 12;

  void add(Block* b);
  Block* find(const char* name) const;
  int count() const { return count_; }
  Block* at(int i) const { return blocks_[i]; }

  void beginAll();
  void loopAll();

  // Fills {"base":{...},"b1":{...},...} into `out`.
  void statusAll(JsonObject out);

  // Lists block names into a JSON array (used by the "hello" message).
  void names(JsonArray out) const;

 private:
  Block* blocks_[kMaxBlocks] = {};
  int count_ = 0;
};
