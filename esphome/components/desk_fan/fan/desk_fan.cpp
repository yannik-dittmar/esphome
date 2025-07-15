#include "desk_fan.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

namespace esphome {
namespace desk_fan {

void DeskFan::setup() {
  this->irsend = new IRsend(14, true, false);
  this->irsend->begin();

  this->irrecv = new IRrecv(26);
  this->irrecv->enableIRIn();

  this->state = false;
  this->speed = 3;  // Default speed, when fan is powered on the first time
  this->oscillating = false;
}

fan::FanTraits DeskFan::get_traits() {
  auto traits = fan::FanTraits();
  traits.set_direction(false);
  traits.set_speed(true);
  traits.set_oscillation(true);
  traits.set_supported_speed_count(8);
  traits.set_supported_preset_modes({});
  return traits;
}

void DeskFan::control(const fan::FanCall &call) {
  ESP_LOGD("desk_fan", "Received control call");

  if (call.get_state().has_value()) {
    bool target_state = *call.get_state();

    if (!target_state) {
      if (this->state) {
        this->send_ir(IR_POWER_TOGGLE);
        this->state = false;
        this->oscillating = false;
      }
    } else {
      if (!this->state) {
        this->send_ir(IR_POWER_TOGGLE);
        this->state = true;
      }
    }
  }

  // ─── Handle Speed/On/Off ─────────────────────────────────────────────────
  if (call.get_speed().has_value()) {
    uint8_t target_speed = *call.get_speed();

    // If target_speed == 0, turn off (toggle power if currently on)
    if (target_speed == 0) {
      if (this->state) {
        this->send_ir(IR_POWER_TOGGLE);
        this->state = false;
        this->oscillating = false;
      }
    } else {
      // If currently off, power‐toggle on first
      if (!this->state) {
        this->send_ir(IR_POWER_TOGGLE);
        this->state = true;
      }

      if (this->speed > target_speed) {
        this->send_ir(IR_SPEED_DOWN, this->speed - target_speed);
      } else if (this->speed < target_speed) {
        this->send_ir(IR_SPEED_UP, target_speed - this->speed);
      }

      this->speed = target_speed;
    }
  }

  // ─── Handle Oscillation ───────────────────────────────────────────────────
  if (call.get_oscillating().has_value()) {
    bool want_osc = *call.get_oscillating();
    if (want_osc != this->oscillating) {
      this->send_ir(IR_OSCILLATION_TOGGLE);
      this->oscillating = want_osc;
    }
  }

  // Publish the new state back to Home Assistant
  this->publish_state();
}

void DeskFan::send_ir(uint64_t ir_code, uint16_t amount) {
  if (ir_code == 0) {
    return;
  }
  while (amount--) {
    ESP_LOGD("desk_fan", "Sending IR code: 0x%016llX", ir_code);
    this->irsend->sendNEC(ir_code);
    delay(150);
  }
}

void DeskFan::loop() {
  decode_results results;
  if (this->irrecv->decode(&results)) {
    irrecv->resume();  // Prepare to receive the next signal

    if (results.decode_type != NEC) {
      ESP_LOGW("desk_fan", "Received unsupported IR signal type: %s", typeToString(results.decode_type).c_str());
      this->last_ir_code = 0;
      return;
    }
    if (this->handle_remote_command(results.value)) {
      // After every command the remote sends a repeat code, which we ignore the first time
      this->ignore_next_repeat = true;
      this->last_ir_code = results.value;
    }
    publish_state();
  }
}

bool DeskFan::handle_remote_command(uint64_t ir_code) {
  switch (ir_code) {
    case IR_POWER_TOGGLE:
      this->send_ir(IR_POWER_TOGGLE);
      this->state = !this->state;
      if (!this->state) {
        this->oscillating = false;
      }
      break;
    case IR_SPEED_UP:
      this->send_ir(IR_SPEED_UP);
      if (this->speed < this->get_traits().supported_speed_count()) {
        this->speed++;
      }
      break;
    case IR_SPEED_DOWN:
      this->send_ir(IR_SPEED_DOWN);
      if (this->speed > 1) {
        this->speed--;
      }
      break;
    case IR_OSCILLATION_TOGGLE:
      this->send_ir(IR_OSCILLATION_TOGGLE);
      this->oscillating = !this->oscillating;
      break;
    case IR_REPEAT:
      if (this->ignore_next_repeat) {
        this->ignore_next_repeat = false;
      } else {
        if (this->last_ir_code == IR_SPEED_UP || this->last_ir_code == IR_SPEED_DOWN) {
          this->handle_remote_command(this->last_ir_code);  // Repeat the last command
        }
      }
      return false;
    default:
      ESP_LOGW("desk_fan", "Received unknown IR code: 0x%016llX", ir_code);
      this->last_ir_code = 0;
      return false;
  }

  return true;
}

}  // namespace desk_fan
}  // namespace esphome
