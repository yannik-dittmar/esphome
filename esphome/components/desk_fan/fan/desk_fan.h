#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>

namespace esphome {
namespace desk_fan {

class DeskFan : public fan::Fan, public Component {
 public:
  void setup() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
  void loop() override;

 protected:
  void send_ir(uint64_t ir_code, uint16_t amount = 1);
  bool handle_remote_command(uint64_t ir_code);

  IRsend *irsend;
  IRrecv *irrecv;

  uint64_t last_ir_code = 0;        // Last IR code received from the remote, used for repeating actions
  bool ignore_next_repeat = false;  // Ignore the next repeat code, because the remote sends it after every action

  // ──────────────────────────────────────────────────────────────────────────
  // IR Codes for the Desk Fan
  static constexpr uint64_t IR_REPEAT = 0xFFFFFFFFFFFFFFFFUL;
  static constexpr uint64_t IR_POWER_TOGGLE = 0x01FE48B7UL;
  static constexpr uint64_t IR_SPEED_UP = 0x1FE609FUL;
  static constexpr uint64_t IR_SPEED_DOWN = 0x1FE20DF;
  static constexpr uint64_t IR_OSCILLATION_TOGGLE = 0x1FE9867UL;
  // ──────────────────────────────────────────────────────────────────────────
};

}  // namespace desk_fan
}  // namespace esphome
