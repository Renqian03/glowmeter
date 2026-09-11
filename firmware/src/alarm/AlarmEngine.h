// Alarm engine: a small list of rules, each
//   {block, key, op, threshold, action}
// evaluated against every status broadcast (20 Hz). When a rule goes from
// false to true it fires its action once:
//   action "relay:N:on" / "relay:N:off"  -> b4 relay N
//   action "notify"                       -> {"type":"alarm",...} to all clients
// When the rule goes back to false, a relay action is NOT undone (the user
// decides); notify is armed again.
//
// The engine is itself a Block named "alarms" so the normal message protocol
// manages it: cmds list / add / remove / clear. Rules persist in /alarms.json
// on LittleFS so they survive a reboot.
#pragma once
#include <functional>
#include "../blocks/Block.h"
#include "../blocks/Registry.h"

class AlarmEngine : public Block {
 public:
  static constexpr int kMaxRules = 16;

  explicit AlarmEngine(Registry& reg) : reg_(reg) {}

  const char* name() const override { return "alarms"; }
  void begin() override;
  void loop() override {}
  bool handle(JsonObjectConst cmd, JsonObject reply) override;
  void status(JsonObject out) override;

  // Called by main.cpp with the freshly built {"base":{...},"b1":{...}} object.
  void evaluate(JsonObjectConst blocks);

  // main.cpp installs a function that broadcasts a JSON string to all clients.
  void onNotify(std::function<void(const String&)> fn) { notify_ = fn; }

 private:
  struct Rule {
    bool used = false;
    int id = 0;
    char block[12] = "";
    char key[24] = "";
    char op[4] = "gt";       // gt | lt | ge | le | eq | ne
    float threshold = 0;
    char action[24] = "notify";
    bool active = false;     // last evaluation result (for edge detection)
    uint32_t fired = 0;      // how many times it fired
  };

  bool compare(const Rule& r, float v) const;
  void fire(Rule& r, float v);
  void toJson(const Rule& r, JsonObject out) const;
  void save();
  void load();

  Registry& reg_;
  Rule rules_[kMaxRules];
  int nextId_ = 1;
  std::function<void(const String&)> notify_;
};
