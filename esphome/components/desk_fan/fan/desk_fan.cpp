#include "desk_fan.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace desk_fan {

void DeskFan::setup() {
  this->speed = 3;  // Default speed when fan is powered on for the first time
  this->publish_state();
}

// Process one queued IR command per loop() call.
// While a command is executing (blocking transmit), the ESPHome loop is held,
// so no other IR command can start — commands are strictly serialized.
// A non-blocking cooldown (IR_CMD_GAP_MS) is enforced between commands so the
// fan PCB has time to finish processing one command before the next arrives.
void DeskFan::loop() {
  if (ir_queue_.empty())
    return;
  if (millis() - this->ir_last_ms_ < IR_CMD_GAP_MS)
    return;
  auto cmd = ir_queue_.front();
  ir_queue_.pop_front();
  if (cmd.hold_ms > 0) {
    this->transmit_ir_held_(cmd.command, cmd.hold_ms);
  } else {
    this->transmit_ir_(cmd.command, cmd.amount);
  }
  this->ir_last_ms_ = millis();
}

fan::FanTraits DeskFan::get_traits() {
  auto traits = fan::FanTraits();
  traits.set_direction(false);
  traits.set_speed(true);
  traits.set_oscillation(true);
  traits.set_supported_speed_count(8);
  return traits;
}

void DeskFan::control(const fan::FanCall &call) {
  ESP_LOGD("desk_fan", "Received control call");

  if (call.get_state().has_value()) {
    bool target_state = *call.get_state();

    if (!target_state) {
      if (this->state) {
        this->send_ir(NEC_CMD_POWER_TOGGLE);
        this->state = false;
        this->oscillating = false;
      }
    } else {
      if (!this->state) {
        this->send_ir(NEC_CMD_POWER_TOGGLE);
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
        this->send_ir(NEC_CMD_POWER_TOGGLE);
        this->state = false;
        this->oscillating = false;
      }
    } else {
      // If currently off, power-toggle on first
      if (!this->state) {
        this->send_ir(NEC_CMD_POWER_TOGGLE);
        this->state = true;
      }

      if (this->speed > target_speed) {
        this->send_ir(NEC_CMD_SPEED_DOWN, this->speed - target_speed);
      } else if (this->speed < target_speed) {
        this->send_ir(NEC_CMD_SPEED_UP, target_speed - this->speed);
      }

      this->speed = target_speed;
    }
  }

  // ─── Handle Oscillation ───────────────────────────────────────────────────
  if (call.get_oscillating().has_value()) {
    bool want_osc = *call.get_oscillating();
    if (want_osc != this->oscillating) {
      this->send_ir(NEC_CMD_OSCILLATION_TOGGLE);
      this->oscillating = want_osc;
    }
  }

  // Publish the new state back to Home Assistant
  this->publish_state();
}

void DeskFan::send_ir(uint16_t command, uint16_t amount) {
  ir_queue_.push_back({command, amount, 0});
}

void DeskFan::send_ir_held(uint16_t command, uint32_t hold_ms) {
  ir_queue_.push_back({command, 1, hold_ms});
}

void DeskFan::transmit_ir_(uint16_t command, uint16_t amount) {
  remote_base::NECData data{};
  data.address = NEC_ADDRESS;
  data.command = command;
  data.command_repeats = 1;
  for (uint16_t i = 0; i < amount; i++) {
    ESP_LOGD("desk_fan", "Sending NEC: address=0x%04X command=0x%04X", data.address, data.command);
    this->transmit_<remote_base::NECProtocol>(data);
    if (i + 1 < amount)
      delay(150);  // inter-frame gap within a command; post-command gap is handled by IR_CMD_GAP_MS
  }
}

void DeskFan::transmit_ir_held_(uint16_t command, uint32_t hold_ms) {
  remote_base::NECData data{};
  data.address = NEC_ADDRESS;
  data.command = command;
  data.command_repeats = 1;
  uint32_t start = millis();
  do {
    ESP_LOGD("desk_fan", "Sending NEC hold: address=0x%04X command=0x%04X", data.address, data.command);
    this->transmit_<remote_base::NECProtocol>(data);
    delay(50);
  } while (millis() - start < hold_ms);
}

bool DeskFan::on_receive(remote_base::RemoteReceiveData data) {
  // Try to decode as a full NEC packet
  auto decoded = remote_base::NECProtocol().decode(data);
  if (decoded.has_value()) {
    ESP_LOGD("desk_fan", "Received NEC: address=0x%04X command=0x%04X repeats=%d", decoded->address, decoded->command,
             decoded->command_repeats);
    if (decoded->address != NEC_ADDRESS) {
      ESP_LOGD("desk_fan", "Ignoring NEC: address 0x%04X != expected 0x%04X", decoded->address, NEC_ADDRESS);
      return false;
    }
    bool handled = this->handle_remote_command(decoded->command);
    if (handled) {
      this->ignore_next_repeat_ = true;
      this->last_command_ = decoded->command;
      this->publish_state();
    }
    return handled;
  }

  // Check for NEC repeat frame: ~9ms mark followed by ~2.25ms space.
  // The physical remote sends one repeat frame after each button press,
  // and continues sending repeat frames while the button is held.
  if (!data.peek_mark(9000) || !data.peek_space(2250, 1)) {
    return false;
  }
  if (this->ignore_next_repeat_) {
    this->ignore_next_repeat_ = false;
    return true;
  }
  // Repeat speed commands while the button is held down.
  // Light toggle is handled via a single queued hold command (no per-repeat forwarding needed).
  if (this->last_command_ == NEC_CMD_SPEED_UP || this->last_command_ == NEC_CMD_SPEED_DOWN) {
    bool handled = this->handle_remote_command(this->last_command_);
    if (handled) {
      this->publish_state();
    }
  }
  return true;
}

bool DeskFan::handle_remote_command(uint16_t command) {
  switch (command) {
    case NEC_CMD_POWER_TOGGLE:
      this->send_ir(NEC_CMD_POWER_TOGGLE);
      this->state = !this->state;
      if (!this->state) {
        this->oscillating = false;
      }
      break;
    case NEC_CMD_SPEED_UP:
      this->send_ir(NEC_CMD_SPEED_UP);
      if (this->speed < this->get_traits().supported_speed_count()) {
        this->speed++;
      }
      break;
    case NEC_CMD_SPEED_DOWN:
      this->send_ir(NEC_CMD_SPEED_DOWN);
      if (this->speed > 1) {
        this->speed--;
      }
      break;
    case NEC_CMD_OSCILLATION_TOGGLE:
      this->send_ir(NEC_CMD_OSCILLATION_TOGGLE);
      this->oscillating = !this->oscillating;
      break;
    case NEC_CMD_LIGHT_TOGGLE:
      // Enqueue a held send — the fan PCB requires a sustained IR hold to toggle the light.
      // Using a single queued hold prevents interleaving with other IR commands.
      this->send_ir_held(NEC_CMD_LIGHT_TOGGLE, 2000);
      this->light_on_ = !this->light_on_;
      if (this->light_output_ != nullptr) {
        this->light_output_->publish_to_ha();
      }
      break;
    default:
      ESP_LOGW("desk_fan", "Received unknown NEC command: 0x%04X", command);
      return false;
  }
  return true;
}

// ─── DeskFanLight ─────────────────────────────────────────────────────────────

void DeskFanLight::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  if (binary != fan_->light_on_) {
    // The fan PCB requires a sustained IR hold to toggle the light.
    // Enqueue the hold — it will be transmitted after any in-progress IR commands finish.
    fan_->send_ir_held(DeskFan::NEC_CMD_LIGHT_TOGGLE, 2000);
    fan_->light_on_ = binary;
  }
}

void DeskFanLight::publish_to_ha() {
  if (light_state_ == nullptr)
    return;
  // Update HA with the new state. write_state() will see no change and skip IR.
  light_state_->make_call().set_state(fan_->light_on_).perform();
}

}  // namespace desk_fan
}  // namespace esphome
