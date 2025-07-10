#include "desk_fan.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

namespace esphome {
namespace desk_fan {

void DeskFan::setup() {
  this->irsend = new IRsend(14, true, false);
  this->irsend->begin();
}

fan::FanTraits DeskFan::get_traits() {
  auto traits = fan::FanTraits();
  traits.set_direction(false);
  traits.set_speed(true);
  traits.set_oscillation(true);
  traits.set_supported_speed_count(9);
  traits.set_supported_preset_modes({});
  return traits;
}

void DeskFan::control(const fan::FanCall &call) {
  ESP_LOGD("desk_fan", "Received control call");

  if (call.get_state().has_value()) {
    bool target_state = *call.get_state();

    if (!target_state) {
      if (this->state) {
        this->send_power_toggle_();
        this->state = false;
        this->speed = 0;
      }
    } else {
      if (!this->state) {
        this->send_power_toggle_();
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
        this->send_power_toggle_();
        this->state = false;
        this->speed = 0;
      }
    } else {
      // If currently off, power‐toggle on first
      if (!this->state) {
        this->send_power_toggle_();
        this->state = true;
        // Assume fan starts at speed = 1 on power-on
        this->speed = 1;
      }

      // Now adjust from speed → target_speed
      while (this->speed < target_speed) {
        this->send_speed_up_();
        delay(100);
        this->speed++;
      }
      while (this->speed > target_speed) {
        this->send_speed_down_();
        delay(100);
        this->speed--;
      }
    }
  }

  // ─── Handle Oscillation ───────────────────────────────────────────────────
  if (call.get_oscillating().has_value()) {
    bool want_osc = *call.get_oscillating();
    if (want_osc != this->oscillating) {
      this->send_oscillation_toggle_();
      this->oscillating = want_osc;
    }
  }

  // Publish the new state back to Home Assistant
  this->publish_state();
}

void DeskFan::set_sleep_timer(uint32_t minutes) {
  if (minutes == 0) {
    // No timer if minutes == 0
    return;
  }
  // WARNING: this uses a blocking delay. For short timers it's okay,
  // but for longer ones (over a minute) it will freeze the main loop.
  // In production, switch to App.schedule or an esphome Timer instead.
  delay(minutes * 60000);
  if (this->state) {
    this->send_power_toggle_();
    this->state = false;
    this->speed = 0;
    this->publish_state();
  }
}

void DeskFan::send_power_toggle_() { this->irsend->sendNEC(this->IR_POWER_TOGGLE); }

void DeskFan::send_speed_up_() { this->irsend->sendNEC(this->IR_SPEED_UP); }

void DeskFan::send_speed_down_() { this->irsend->sendNEC(this->IR_SPEED_DOWN); }

void DeskFan::send_oscillation_toggle_() { this->irsend->sendNEC(this->IR_OSCILLATION_TOGGLE); }

}  // namespace desk_fan
}  // namespace esphome
