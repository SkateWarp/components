#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
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

// Compact struct for NVS persistence — only controllable fields
// NVS handles integrity (CRC per entry), no need for our own checksum
// Packed: 3 bytes total
struct __attribute__((packed)) SavedState {
  uint8_t power   : 1;
  uint8_t eco     : 1;
  uint8_t turbo   : 1;
  uint8_t display : 1;
  uint8_t health  : 1;
  uint8_t mute    : 1;
  uint8_t sleep   : 1;
  uint8_t swing_v : 1;
  uint8_t mode    : 4;
  uint8_t fan     : 3;
  uint8_t _pad    : 1;
  uint8_t target_temp;  // 16-31

  void from_ac_state(const AcState &s) {
    power = s.power; eco = s.eco; turbo = s.turbo;
    display = s.display; health = s.health; mute = s.mute;
    sleep = s.sleep; swing_v = s.swing_v;
    mode = s.mode; fan = s.fan; target_temp = s.target_temp;
    _pad = 0;
  }

  void to_ac_state(AcState &s) const {
    s.power = power; s.eco = eco; s.turbo = turbo;
    s.display = display; s.health = health; s.mute = mute;
    s.sleep = sleep; s.swing_v = swing_v;
    s.mode = mode; s.fan = fan; s.target_temp = target_temp;
  }

  bool equals(const SavedState &other) const {
    return power == other.power && eco == other.eco && turbo == other.turbo &&
           display == other.display && health == other.health && mute == other.mute &&
           sleep == other.sleep && swing_v == other.swing_v &&
           mode == other.mode && fan == other.fan && target_temp == other.target_temp;
  }
};

struct AcStateListener {
  std::function<void(const AcState &state)> func;
};

class TclMinisplit : public Component, public uart::UARTDevice {
 public:
  TclMinisplit() = default;

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void register_listener(std::function<void(const AcState &state)> func);

  // Pending state management for commands
  void prepare_pending_state();
  bool has_pending_state() const { return pending_state_ != nullptr; }
  AcState *get_pending_state() { return pending_state_.get(); }

  // Persistence control
  void set_persistence_enabled(bool enabled);
  bool get_persistence_enabled() const { return persistence_enabled_; }

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

  // ─── Persistence ──────────────────────────────────────────────
  // Strategy: debounced write — only saves to NVS when state has been
  // stable for PERSIST_DEBOUNCE_MS and actually differs from last save.
  // ESP32 NVS has built-in wear leveling across flash pages.
  // With 60s debounce, worst case ~1440 writes/day, well within the
  // ~100k cycle rating even without NVS wear leveling.
  void persistence_check_save_();
  void persistence_restore_on_first_rx_();
  void persistence_mark_dirty_();

  bool persistence_enabled_{false};
  bool persistence_restored_{false};     // Have we restored after boot?
  bool persistence_has_saved_{false};    // Did we load a valid state from NVS?
  bool persistence_dirty_{false};        // State changed since last save?
  unsigned long persistence_dirty_since_{0};  // When state first became dirty
  SavedState last_saved_state_{};        // What we last wrote to NVS

  ESPPreferenceObject pref_state_;       // NVS handle for saved state
  ESPPreferenceObject pref_enabled_;     // NVS handle for enabled flag

  static constexpr unsigned long PERSIST_DEBOUNCE_MS = 60000;  // 60 seconds

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
  bool first_rx_received_{false};

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
