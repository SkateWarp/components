#include "tcl_minisplit.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

static const char *const TAG = "tcl_minisplit";

// Static constexpr member definition (needed for C++14)
constexpr uint8_t TclMinisplit::TX_BASE[TX_LENGTH];

void TclMinisplit::setup() {
  // Initialize NVS preference handles
  // fnv1_hash generates a stable key from the string
  this->pref_state_ = global_preferences->make_preference<SavedState>(fnv1_hash("tcl_ac_state"));
  this->pref_enabled_ = global_preferences->make_preference<bool>(fnv1_hash("tcl_ac_persist"));

  // Load persistence enabled flag
  bool saved_enabled = false;
  if (this->pref_enabled_.load(&saved_enabled)) {
    this->persistence_enabled_ = saved_enabled;
    ESP_LOGI(TAG, "Persistence loaded: %s", saved_enabled ? "ON" : "OFF");
  }

  // Pre-load saved state (will be applied on first RX)
  if (this->persistence_enabled_) {
    if (this->pref_state_.load(&this->last_saved_state_)) {
      this->persistence_has_saved_ = true;
      ESP_LOGI(TAG, "Saved state loaded: mode=0x%02X temp=%.1f power=%d",
               this->last_saved_state_.mode,
               16.0f + this->last_saved_state_.target_temp / 2.0f,
               this->last_saved_state_.power);
    } else {
      ESP_LOGW(TAG, "No saved state found");
      this->last_saved_state_ = {};
    }
  }
}

void TclMinisplit::loop() {
  this->read_serial_data_();
  this->send_pending_command_();
  this->send_heartbeat_if_needed_();
  this->persistence_check_save_();
}

void TclMinisplit::register_listener(std::function<void(const AcState &state)> func) {
  this->listeners_.push_back({std::move(func)});
}

void TclMinisplit::prepare_pending_state() {
  this->pending_state_ = std::make_unique<AcState>(this->state_);
}

// ─── Serial RX ──────────────────────────────────────────────────
// State machine parser: handles byte-by-byte reception
// Protocol: [0xBB] [ver_hi] [ver_lo] [type] [len] [len bytes...] [xor]
// Improved over both implementations:
//  - Anyelo's: same state machine logic but without static variables (reentrant)
//  - Ryan's: validates header during reception, not just after timeout

void TclMinisplit::read_serial_data_() {
  while (this->available()) {
    uint8_t byte = this->read();

    if (byte == 0xBB && this->rx_skip_ == 0 && !this->rx_wait_len_) {
      // Start of new packet
      this->rx_pos_ = 0;
      this->rx_skip_ = 3;  // Skip 3 bytes (ver_hi, ver_lo, type) before length
      this->rx_wait_len_ = true;
      if (this->rx_pos_ < sizeof(this->rx_buffer_))
        this->rx_buffer_[this->rx_pos_++] = byte;
    } else if (this->rx_skip_ == 0 && this->rx_wait_len_) {
      // This byte is the length field
      if (this->rx_pos_ < sizeof(this->rx_buffer_))
        this->rx_buffer_[this->rx_pos_++] = byte;
      this->rx_skip_ = byte + 1;  // length + 1 for checksum
      this->rx_wait_len_ = false;
    } else if (this->rx_skip_ > 0) {
      if (this->rx_pos_ < sizeof(this->rx_buffer_))
        this->rx_buffer_[this->rx_pos_++] = byte;
      if (--this->rx_skip_ == 0 && !this->rx_wait_len_) {
        // Complete packet received
        this->process_serial_data_();
      }
    }
  }
}

void TclMinisplit::process_serial_data_() {
  size_t len = this->rx_pos_;
  bool dev_mode_active = (millis() < this->dev_pause_until_);

  // We expect 61-byte status responses with type 0x04
  if (len == 61 && this->rx_buffer_[3] == 0x04) {
    if (this->validate_checksum_(this->rx_buffer_, len)) {
      this->log_hex_("RX", this->rx_buffer_, len);
      this->parse_rx_packet_(this->rx_buffer_, len);
      this->awaiting_response_ = false;
      // In dev mode, also publish known packets so user sees all traffic
      if (dev_mode_active && this->raw_rx_sensor_ != nullptr) {
        this->raw_rx_sensor_->publish_state(this->format_hex_(this->rx_buffer_, len));
      }
    } else {
      ESP_LOGW(TAG, "Invalid checksum on %d-byte packet", len);
    }
  } else if (len > 0) {
    // Log any unexpected/unknown packet (useful for dev mode probing)
    this->log_hex_("RX UNK", this->rx_buffer_, len);
    ESP_LOGD(TAG, "Unknown packet: type=0x%02X len=%d", len > 3 ? this->rx_buffer_[3] : 0, len);
    if (this->raw_rx_sensor_ != nullptr) {
      this->raw_rx_sensor_->publish_state(this->format_hex_(this->rx_buffer_, len));
    }
  }

  this->rx_pos_ = 0;
}

void TclMinisplit::parse_rx_packet_(const uint8_t *data, size_t len) {
  // Byte 7: [turbo, eco, display, power, mode(4)]
  this->state_.turbo   = (data[7] >> 7) & 1;
  this->state_.eco     = (data[7] >> 6) & 1;
  this->state_.display = (data[7] >> 5) & 1;
  this->state_.power   = (data[7] >> 4) & 1;
  this->state_.mode    = data[7] & 0x0F;

  // Byte 8: [x, fan(3), temp(4)]
  uint8_t raw_fan = (data[8] >> 4) & 0x07;
  if (this->five_fan_speeds_) {
    // 5-speed RX: raw masked values are non-sequential
    // adaasch: 0x8=auto,0x9=s1,0xC=s2,0xA=s3,0xD=s4,0xB=s5
    // After &0x07:  0=auto, 1=s1, 4=s2, 2=s3, 5=s4, 3=s5
    // Normalize to 3 bands: s1→low(1), s2-s3→med(2), s4-s5→high(3)
    static const uint8_t rx_5fan[] = {0, 1, 2, 3, 1, 2};  // index=raw&7, value=normalized
    this->state_.fan = (raw_fan < sizeof(rx_5fan)) ? rx_5fan[raw_fan] : 0;
  } else {
    // 3-speed: raw values 0-3 already sequential (auto/low/med/high)
    this->state_.fan = raw_fan;
  }
  this->state_.target_temp = static_cast<float>((data[8] & 0x0F) + 16);

  // Byte 9: [0,timer_active,0,0, 0,health,0,0]
  this->state_.timer_active = (data[9] >> 6) & 1;
  this->state_.health       = (data[9] >> 2) & 1;

  // Byte 10: swing
  this->state_.swing_v = (data[10] >> 6) & 1;
  this->state_.swing_h = (data[10] >> 5) & 1;

  // Bytes 11-12: timer values
  this->state_.timer_hour = data[11] & 0x3F;  // 6 bits for hours
  this->state_.timer_min  = data[12] & 0x3F;  // 6 bits for minutes

  // Bytes 17-18: current temperature (raw ADC → °C)
  float raw_temp = (((data[17] << 8) | data[18]) / 374.0f - 32.0f) / 1.8f;
  this->update_temp_average_(raw_temp);

  // Byte 19: sleep
  this->state_.sleep     = data[19] & 1;
  this->state_.sleep_ext = data[19];
  this->state_.deep_sleep = (data[19] & 0x80) == 0;  // bit7=0 means deep sleep active

  // Byte 33: mute (bit 7)
  this->state_.mute = (data[33] >> 7) & 1;

  // Byte 34: actual fan speed
  this->state_.fan_speed_raw = data[34];

  // Bytes 35-36: pipe temperatures
  this->state_.pipe_out = data[35];
  this->state_.pipe_in  = data[36];

  // Byte 39: compressor current (÷10 for amps)
  this->state_.compressor_current = data[39] / 10.0f;

  // Byte 40: compressor state / action
  // 0x8A = cooling, 0xCA = heating, 0x80 = idle, 0xC0 = cooldown
  this->state_.compressor_state = data[40];

  // Byte 44: fault code
  this->state_.fault = data[44];

  // Byte 45: supply voltage
  this->state_.supply_voltage = data[45];

  // Byte 46: outside motor
  this->state_.outside_motor = data[46];

  // Byte 50: [0,0,0,0, 0,0,clean_filter,0]
  this->state_.clean_filter = (data[50] >> 1) & 1;

  // Bytes 51-52: vane positions
  this->state_.swing_v_pos = data[51];
  this->state_.swing_h_pos = data[52];

  // On first valid RX after boot, restore saved state if persistence is on
  if (!this->first_rx_received_) {
    this->first_rx_received_ = true;
    this->persistence_restore_on_first_rx_();
  } else if (this->persistence_enabled_) {
    // Check if controllable state changed (e.g. physical remote used)
    SavedState current;
    current.from_ac_state(this->state_, this->gen_);
    if (!current.equals(this->last_saved_state_)) {
      this->persistence_mark_dirty_();
    }
  }

  this->notify_listeners_();
}

// ─── Serial TX ──────────────────────────────────────────────────

void TclMinisplit::send_pending_command_() {
  if (!this->pending_state_ || this->awaiting_response_)
    return;

  uint8_t cmd[TX_LENGTH];
  this->build_tx_packet_(*this->pending_state_, cmd, TX_LENGTH);
  this->calculate_checksum_(cmd, TX_LENGTH);

  this->log_hex_("TX", cmd, TX_LENGTH);
  this->write_array(cmd, TX_LENGTH);

  this->pending_state_.reset();
  this->awaiting_response_ = true;
  this->last_heartbeat_ = millis();

  // A user command was sent — mark persistence dirty
  this->persistence_mark_dirty_();
}

void TclMinisplit::build_tx_packet_(const AcState &state, uint8_t *cmd, size_t len) {
  memcpy(cmd, TX_BASE, len);

  // Byte 7: [eco(7), display(6), beep(5), ?(4), ?(3), power(2), gen(1:0)]
  // adaasch: power=0x4 in bits[4:7] → that's bit 2 in LSB-first
  // Timer enable bits removed — adaasch confirms they don't exist in TX
  cmd[7] = 0;
  cmd[7] |= (state.eco     ? 1 : 0) << 7;
  cmd[7] |= (state.display ? 1 : 0) << 6;
  cmd[7] |= (state.beep    ? 1 : 0) << 5;
  cmd[7] |= (state.power   ? 1 : 0) << 2;
  cmd[7] |= (this->gen_ & 0x03);  // bits 0-1 = protocol generation (hub-level)

  // Byte 8: [mute(7), ?(6), turbo(6), health(4), mode(3:0)]
  // Mode mapping: RX→TX (protocol quirk, confirmed by all sources)
  //   RX 0x01 (cool) → TX 0x03    RX 0x04 (heat) → TX 0x01
  //   RX 0x02 (fan)  → TX 0x07    RX 0x05 (auto) → TX 0x08
  //   RX 0x03 (dry)  → TX 0x02
  static const uint8_t mode_map[] = {0, 0x03, 0x07, 0x02, 0x01, 0x08};
  uint8_t tx_mode = (state.mode < sizeof(mode_map)) ? mode_map[state.mode] : 0x03;
  cmd[8] = tx_mode;
  cmd[8] |= (state.turbo  ? 1 : 0) << 6;
  cmd[8] |= (state.mute   ? 1 : 0) << 7;
  cmd[8] |= (state.health ? 1 : 0) << 4;

  // Byte 9: [?(7:4), temp(3:0)]  temp = 31 - integer_part
  float clamped = std::max(16.0f, std::min(31.0f, state.target_temp));
  uint8_t int_temp = static_cast<uint8_t>(clamped);
  bool half_degree = (clamped - int_temp) >= 0.25f;
  cmd[9] = 31 - int_temp;

  // Byte 10: [8deg_heater(7), ?(6), vswing(5:3), fan(2:0)]
  // Fan mapping: normalized (0=auto,1=low,2=med,3=high) → TX wire value
  // 3-speed: auto=0, low=0x02(s1), med=0x03(s2), high=0x05(s3)
  // 5-speed: auto=0, low=0x02(s1), med=0x03(s3), high=0x05(s5) — same wire values, AC skips s2/s4
  static const uint8_t fan_map[] = {0, 2, 3, 5};
  uint8_t tx_fan = (state.fan < sizeof(fan_map)) ? fan_map[state.fan] : 0;
  cmd[10] = tx_fan;
  if (state.swing_v) {
    cmd[10] |= 0x07 << 3;  // bits 3,4,5 = vswing move
  }

  // Byte 11: [?(7:4), hswing(3), ?(2), halfdegree(1), ?(0)]
  // Per adaasch — verified with logic analyzer
  cmd[11] = 0;
  if (state.swing_h) {
    cmd[11] |= (1 << 3);  // bit 3 = hswing move
  }
  if (half_degree) {
    cmd[11] |= (1 << 1);  // bit 1 = 0.5°C
  }

  // Byte 12: [fahrenheit(7), ?(6:0)]
  cmd[12] = 0;
  if (state.fahrenheit) {
    cmd[12] |= (1 << 7);
  }

  // Byte 14: [?(7:6), halfdegree(5), ?(4), swingh(3), ?(2:0)]
  // Per junkfix — write BOTH bytes 11 and 14 for firmware compatibility
  cmd[14] = 0;
  if (half_degree) {
    cmd[14] |= (1 << 5);  // bit 5 = half degree (junkfix layout)
  }
  if (state.swing_h) {
    cmd[14] |= (1 << 3);  // bit 3 = hswing (junkfix layout)
  }

  // Byte 19: [?(7:2), sleep_mode(1:0)]
  // adaasch: 0=off, 1=default, 2=elderly, 3=young
  cmd[19] = state.sleep_mode & 0x03;

  // Byte 32: vertical vane position (adaasch mapping)
  // 0x00=N/A, 0x01-0x05=fix positions, 0x08/0x10/0x18=move ranges
  cmd[32] = state.vswing_pos_tx;

  // Byte 33: horizontal vane position (adaasch mapping)
  // 0x80=N/A, 0x81-0x85=fix positions, 0x88/0x90/0x98/0xA0=move ranges
  cmd[33] = state.hswing_pos_tx;
}

void TclMinisplit::send_heartbeat_if_needed_() {
  unsigned long now = millis();

  // Dev mode: pause heartbeats to let custom responses come through
  if (now < this->dev_pause_until_)
    return;

  if (now - this->last_heartbeat_ >= 500) {
    static const uint8_t heartbeat[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
    this->write_array(heartbeat, sizeof(heartbeat));
    this->last_heartbeat_ = now;
    this->awaiting_response_ = false;  // Reset if stuck
  }
}

// ─── Helpers ────────────────────────────────────────────────────

void TclMinisplit::update_temp_average_(float new_temp) {
  if (this->temp_count_ < TEMP_WINDOW_SIZE) {
    this->temp_readings_[this->temp_count_] = new_temp;
    this->temp_sum_ += new_temp;
    this->temp_count_++;
  } else {
    this->temp_sum_ -= this->temp_readings_[this->temp_index_];
    this->temp_readings_[this->temp_index_] = new_temp;
    this->temp_sum_ += new_temp;
  }
  this->temp_index_ = (this->temp_index_ + 1) % TEMP_WINDOW_SIZE;
  this->state_.current_temp = this->temp_sum_ / this->temp_count_;
}

void TclMinisplit::calculate_checksum_(uint8_t *data, size_t len) {
  uint8_t xor_sum = 0;
  for (size_t i = 0; i < len - 1; i++)
    xor_sum ^= data[i];
  data[len - 1] = xor_sum;
}

bool TclMinisplit::validate_checksum_(const uint8_t *data, size_t len) {
  uint8_t xor_sum = 0;
  for (size_t i = 0; i < len - 1; i++)
    xor_sum ^= data[i];
  if (xor_sum != data[len - 1]) {
    ESP_LOGW(TAG, "Checksum mismatch: got 0x%02X, expected 0x%02X", data[len - 1], xor_sum);
    return false;
  }
  return true;
}

void TclMinisplit::notify_listeners_() {
  for (auto &listener : this->listeners_) {
    listener.func(this->state_);
  }
}

void TclMinisplit::log_hex_(const char *prefix, const uint8_t *data, size_t len) {
  std::string hex = this->format_hex_(data, len);
  ESP_LOGD(TAG, "%s: %s", prefix, hex.c_str());
}

std::string TclMinisplit::format_hex_(const uint8_t *data, size_t len) {
  std::string result;
  result.reserve(len * 3);
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    if (i > 0) result += ' ';
    snprintf(buf, sizeof(buf), "%02X", data[i]);
    result += buf;
  }
  return result;
}

// ─── Dev Mode ─────────────────────────────────────────────────

void TclMinisplit::send_raw_hex(const std::string &hex) {
  // Parse hex string like "BB00010320..." into bytes and send over UART
  // Ignores spaces and validates hex chars
  std::vector<uint8_t> data;
  data.reserve(hex.size() / 2);

  for (size_t i = 0; i < hex.size(); i++) {
    char c = hex[i];
    if (c == ' ' || c == ':' || c == '-')
      continue;

    if (i + 1 >= hex.size())
      break;

    char hi = hex[i];
    char lo = hex[i + 1];
    i++;  // skip lo char

    auto hex_val = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      return -1;
    };

    int h = hex_val(hi);
    int l = hex_val(lo);
    if (h < 0 || l < 0) {
      ESP_LOGW(TAG, "Invalid hex char in raw data at pos %d", i);
      return;
    }
    data.push_back((h << 4) | l);
  }

  if (data.empty()) {
    ESP_LOGW(TAG, "Empty raw data, nothing to send");
    return;
  }

  this->log_hex_("RAW TX", data.data(), data.size());
  this->write_array(data.data(), data.size());
  if (this->raw_tx_sensor_ != nullptr) {
    this->raw_tx_sensor_->publish_state(this->format_hex_(data.data(), data.size()));
  }

  // Pause heartbeats for 3 seconds to allow response from AC
  this->dev_pause_until_ = millis() + 3000;
  ESP_LOGI(TAG, "Dev mode: heartbeats paused for 3s");
}

// ─── Persistence ──────────────────────────────────────────────────
//
// Write strategy (flash wearout mitigation):
//   1. Only write when persistence_enabled_ is true
//   2. Only write if controllable state actually differs from last save
//   3. Debounce: wait PERSIST_DEBOUNCE_MS (60s) after last change
//      → rapid adjustments (user tweaking temp up/down) batch into 1 write
//   4. ESP32 NVS already applies wear leveling across flash pages
//
// Worst case with 60s debounce: 1440 writes/day
// ESP32 flash rated ~100k cycles/sector, NVS spreads across multiple pages
// → years of continuous use before any concern

void TclMinisplit::set_persistence_enabled(bool enabled) {
  if (this->persistence_enabled_ == enabled)
    return;

  this->persistence_enabled_ = enabled;
  this->pref_enabled_.save(&enabled);

  if (enabled) {
    // Save current state immediately when enabling
    SavedState snap;
    snap.from_ac_state(this->state_, this->gen_);
    this->last_saved_state_ = snap;
    this->pref_state_.save(&snap);
    this->persistence_dirty_ = false;
    this->persistence_has_saved_ = true;
    ESP_LOGI(TAG, "Persistence enabled — current state saved");
  } else {
    this->persistence_dirty_ = false;
    ESP_LOGI(TAG, "Persistence disabled");
  }
}

void TclMinisplit::persistence_mark_dirty_() {
  if (!this->persistence_enabled_)
    return;

  if (!this->persistence_dirty_) {
    this->persistence_dirty_ = true;
    this->persistence_dirty_since_ = millis();
    ESP_LOGD(TAG, "Persistence: state marked dirty, will save in %lus",
             PERSIST_DEBOUNCE_MS / 1000);
  }
}

void TclMinisplit::persistence_check_save_() {
  if (!this->persistence_enabled_ || !this->persistence_dirty_)
    return;

  unsigned long now = millis();
  if (now - this->persistence_dirty_since_ < PERSIST_DEBOUNCE_MS)
    return;

  // Debounce elapsed — check if state actually differs from last save
  SavedState current;
  current.from_ac_state(this->state_, this->gen_);

  if (current.equals(this->last_saved_state_)) {
    // State matches what's already saved — skip write
    this->persistence_dirty_ = false;
    ESP_LOGD(TAG, "Persistence: state unchanged, write skipped");
    return;
  }

  this->last_saved_state_ = current;
  this->pref_state_.save(&current);
  this->persistence_dirty_ = false;
  this->persistence_has_saved_ = true;
  ESP_LOGI(TAG, "Persistence: state saved (mode=0x%02X temp=%.1f power=%d fan=%d beep=%d fahr=%d gen=%d)",
           current.mode, 16.0f + current.target_temp / 2.0f, current.power, current.fan,
           current.beep, current.fahrenheit, current.gen);
}

void TclMinisplit::persistence_restore_on_first_rx_() {
  if (!this->persistence_enabled_) {
    ESP_LOGD(TAG, "Persistence disabled, not restoring");
    return;
  }

  if (!this->persistence_has_saved_) {
    ESP_LOGD(TAG, "No saved state to restore");
    return;
  }

  // Compare saved state with what the AC is currently reporting
  SavedState current;
  current.from_ac_state(this->state_, this->gen_);

  if (current.equals(this->last_saved_state_)) {
    ESP_LOGI(TAG, "AC already in saved state, no restore needed");
    this->persistence_restored_ = true;
    return;
  }

  // AC state differs from saved — send restore command
  ESP_LOGI(TAG, "Restoring saved state: mode=0x%02X temp=%.1f power=%d fan=%d",
           this->last_saved_state_.mode,
           16.0f + this->last_saved_state_.target_temp / 2.0f,
           this->last_saved_state_.power, this->last_saved_state_.fan);

  this->pending_state_ = std::make_unique<AcState>(this->state_);
  this->last_saved_state_.to_ac_state(*this->pending_state_, this->gen_);

  this->persistence_restored_ = true;
}

}  // namespace tcl_minisplit
}  // namespace esphome
