#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <functional>
#include <memory>
#include <cmath>

namespace esphome {
namespace tcl_minisplit {

// Efficient state container using a flat struct instead of std::map
// Saves significant heap on ESP8266 compared to Ryan's approach
struct AcState {
  // Parsed from RX (61 bytes)
  bool power{false};
  bool eco{false};
  bool turbo{false};
  bool display{true};
  bool health{false};
  bool mute{false};
  bool sleep{false};
  bool deep_sleep{false};
  bool swing_h{false};
  bool swing_v{false};
  uint8_t mode{0};       // 0x01=cool, 0x02=fan, 0x03=dry, 0x05=auto
  uint8_t fan{0};         // 0=auto, 1=low, 2=med, 3=high
  uint8_t target_temp{24};
  float current_temp{NAN};
  uint8_t fan_speed_raw{0};
  uint8_t pipe_out{0};
  uint8_t pipe_in{0};
  float compressor_current{0.0f};
  uint8_t compressor_state{0};
  uint8_t fault{0};
  uint8_t supply_voltage{0};
  uint8_t outside_motor{0};
  uint8_t sleep_ext{0};   // Full byte 19 for deep sleep detection

  // TX-only state (not reported by device)
  bool beep{true};
};

struct AcStateListener {
  std::function<void(const AcState &state)> func;
};

class TclMinisplit : public Component, public uart::UARTDevice {
 public:
  TclMinisplit() = default;

  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void register_listener(std::function<void(const AcState &state)> func);

  // Pending state management for commands
  void prepare_pending_state();
  bool has_pending_state() const { return pending_state_ != nullptr; }
  AcState *get_pending_state() { return pending_state_.get(); }

 protected:
  // Serial protocol
  void read_serial_data_();
  void process_serial_data_();
  void parse_rx_packet_(const uint8_t *data, size_t len);
  void send_pending_command_();
  void send_heartbeat_if_needed_();
  void build_tx_packet_(const AcState &state, uint8_t *cmd, size_t len);
  void calculate_checksum_(uint8_t *data, size_t len);
  bool validate_checksum_(const uint8_t *data, size_t len);

  // Temperature averaging
  void update_temp_average_(float new_temp);

  // Notify all listeners
  void notify_listeners_();

  // Log helpers
  void log_hex_(const char *prefix, const uint8_t *data, size_t len);

  // Current confirmed state from device
  AcState state_{};

  // Pending state to send (nullptr = nothing to send)
  std::unique_ptr<AcState> pending_state_{nullptr};

  // Listeners
  std::vector<AcStateListener> listeners_;

  // Serial RX state machine
  uint8_t rx_buffer_[70]{};
  uint8_t rx_pos_{0};
  bool rx_wait_len_{false};
  uint8_t rx_skip_{0};

  // Temperature moving average
  static constexpr int TEMP_WINDOW_SIZE = 10;
  float temp_readings_[TEMP_WINDOW_SIZE]{};
  int temp_count_{0};
  int temp_index_{0};
  float temp_sum_{0.0f};

  // Timing
  unsigned long last_heartbeat_{0};
  bool awaiting_response_{false};

  // TX constants
  static constexpr size_t TX_LENGTH = 35;
  static constexpr uint8_t TX_BASE[TX_LENGTH] = {
    0xBB, 0x00, 0x01, 0x03, 0x1D, 0x00, 0x00,
    0x64, 0x03, 0xF3, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
};

}  // namespace tcl_minisplit
}  // namespace esphome
