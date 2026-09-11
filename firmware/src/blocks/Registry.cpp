#include "Registry.h"

void Registry::add(Block* b) {
  if (count_ < kMaxBlocks) blocks_[count_++] = b;
  else Serial.printf("[registry] too many blocks, dropping %s\n", b->name());
}

Block* Registry::find(const char* name) const {
  for (int i = 0; i < count_; i++)
    if (strcmp(blocks_[i]->name(), name) == 0) return blocks_[i];
  return nullptr;
}

void Registry::beginAll() {
  for (int i = 0; i < count_; i++) {
    blocks_[i]->begin();
    Serial.printf("[registry] %s ready\n", blocks_[i]->name());
  }
}

void Registry::loopAll() {
  for (int i = 0; i < count_; i++) blocks_[i]->loop();
}

void Registry::statusAll(JsonObject out) {
  for (int i = 0; i < count_; i++) blocks_[i]->status(out[blocks_[i]->name()].to<JsonObject>());
}

void Registry::names(JsonArray out) const {
  for (int i = 0; i < count_; i++) out.add(blocks_[i]->name());
}
