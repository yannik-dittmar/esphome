#pragma once

#include <deque>
#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace desk_fan {

class DeskFanLight;

// Represents a queued IR command to be transmitted in order.
struct IRCommand {
  uint16_t command;
  uint16_t amount;   // number of frames (used when hold_ms == 0)
  uint32_t hold_ms;  // 0 = regular send, >0 = hold mode (send frames for this many ms)
};

class DeskFan : public Component,
                public fan::Fan,
                public remote_base::RemoteTransmittable,
                public remote_base::RemoteReceiverListener {
 public:
  void setup() override;
  void loop() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Enqueue a command to be sent `amount` times (non-overlapping with any in-progress command).
  void send_ir(uint16_t command, uint16_t amount = 1);
  /// Enqueue a command to be sent repeatedly for `hold_ms` milliseconds.
  void send_ir_held(uint16_t command, uint32_t hold_ms);

  void set_light_output(DeskFanLight *light) { light_output_ = light; }

  // Light on/off state — tracked here so DeskFanLight can read/write it.
  // Initialised to true because the fan always powers on with the light on.
  bool light_on_{true};

  // The desk fan remote uses NEC timing with MSB-first byte order (non-standard).
  // IRremoteESP8266 stored these as 32-bit MSB-first values (0x01FE48B7 etc.).
  // ESPHome's NEC encoder/decoder uses LSB-first, so each byte must be bit-reversed:
  //   address: reverseBits(0x01)=0x80 | reverseBits(0xFE)<<8=0x7F00  → 0x7F80
  //   command: reverseBits(cmd) | reverseBits(~cmd)<<8
  static constexpr uint16_t NEC_ADDRESS = 0x7F80;
  static constexpr uint16_t NEC_CMD_POWER_TOGGLE = 0xED12;        // was 0x01FE48B7
  static constexpr uint16_t NEC_CMD_SPEED_UP = 0xF906;            // was 0x01FE609F
  static constexpr uint16_t NEC_CMD_SPEED_DOWN = 0xFB04;          // was 0x01FE20DF
  static constexpr uint16_t NEC_CMD_OSCILLATION_TOGGLE = 0xE619;  // was 0x01FE9867
  static constexpr uint16_t NEC_CMD_LIGHT_TOGGLE = 0xF50A;        // was 0x01FEA857

 protected:
  bool handle_remote_command(uint16_t command);

  /// Transmit `amount` NEC frames back-to-back (blocking). Called only from loop().
  void transmit_ir_(uint16_t command, uint16_t amount);
  /// Transmit NEC frames continuously for `hold_ms` milliseconds (blocking). Called only from loop().
  void transmit_ir_held_(uint16_t command, uint32_t hold_ms);

  // NOTE: power state, speed and oscillating are tracked via the Fan base class
  // public fields (this->state, this->speed, this->oscillating) so that
  // publish_state() reads the correct values.

  uint16_t last_command_{0};        // Last command received from the physical remote (for NEC repeat)
  bool ignore_next_repeat_{false};  // The remote always sends one repeat after each press; skip it
  DeskFanLight *light_output_{nullptr};

  std::deque<IRCommand> ir_queue_;  // Pending IR commands, processed one at a time from loop()
  uint32_t ir_last_ms_{0};          // millis() when the last IR command finished transmitting

  // Minimum gap between the end of one IR command and the start of the next.
  // The fan PCB needs time to process a command before it can accept the next one.
  static constexpr uint32_t IR_CMD_GAP_MS = 500;
};

class DeskFanLight : public light::LightOutput {
 public:
  void set_fan(DeskFan *fan) { fan_ = fan; }

  void setup_state(light::LightState *state) override { light_state_ = state; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
    return traits;
  }

  void write_state(light::LightState *state) override;

  /// Call after updating fan_->light_on_ from the physical remote to sync HA state.
  void publish_to_ha();

 protected:
  DeskFan *fan_{nullptr};
  light::LightState *light_state_{nullptr};
};

}  // namespace desk_fan
}  // namespace esphome
