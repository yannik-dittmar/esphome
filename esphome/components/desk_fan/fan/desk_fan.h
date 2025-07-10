#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

namespace esphome {
namespace desk_fan {

class DeskFan : public fan::Fan, public Component {
 public:
  void setup() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
  void set_sleep_timer(uint32_t minutes);

 protected:
  void send_power_toggle_();
  void send_speed_up_();
  void send_speed_down_();
  void send_oscillation_toggle_();

  IRsend *irsend;

  // ──────────────────────────────────────────────────────────────────────────
  // Replace these with your actual IR pulse arrays (vector<uint16_t>).
  const uint64_t IR_POWER_TOGGLE = 0x01FE48B7UL;
  const uint64_t IR_SPEED_UP = 0x1FE609FUL;
  const uint64_t IR_SPEED_DOWN = 0x1FE20DF;
  const uint64_t IR_OSCILLATION_TOGGLE = 0x1FE9867UL;
  // ──────────────────────────────────────────────────────────────────────────
};

}  // namespace desk_fan
}  // namespace esphome
