#include "tcl_minisplit.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

static const char *const TAG = "tcl_minisplit";

// Static constexpr member definition (needed for C++14)
constexpr uint8_t TclMinisplit::TX_BASE[TX_LENGTH];

void TclMinisplit::loop() {
  this->read_serial_data_();
  this->send_pending_command_();
  this->send_heartbeat_if_needed_();
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

  // We expect 61-byte status responses with type 0x04
  if (len == 61 && this->rx_buffer_[3] == 0x04) {
    if (this->validate_checksum_(this->rx_buffer_, len)) {
      this->log_hex_("RX", this->rx_buffer_, len);
      this->parse_rx_packet_(this->rx_buffer_, len);
      this->awaiting_response_ = false;
    } else {
      ESP_LOGW(TAG, "Invalid checksum on %d-byte packet", len);
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
  this->state_.fan         = (data[8] >> 4) & 0x07;
  this->state_.target_temp = (data[8] & 0x0F) + 16;

  // Byte 9: health
  this->state_.health = (data[9] >> 2) & 1;

  // Byte 10: swing
  this->state_.swing_v = (data[10] >> 6) & 1;
  this->state_.swing_h = (data[10] >> 5) & 1;

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
}

void TclMinisplit::build_tx_packet_(const AcState &state, uint8_t *cmd, size_t len) {
  memcpy(cmd, TX_BASE, len);

  // Byte 7: [eco, display, beep, x, x, power, 0, 0]
  cmd[7] = 0;
  cmd[7] |= (state.eco     ? 1 : 0) << 7;
  cmd[7] |= (state.display ? 1 : 0) << 6;
  cmd[7] |= (state.beep    ? 1 : 0) << 5;
  cmd[7] |= (state.power   ? 1 : 0) << 2;

  // Byte 8: [mute, 0, turbo, health, mode(4)]
  // Mode mapping: RX→TX (protocol quirk)
  //   RX 0x01 (cool) → TX 0x03
  //   RX 0x03 (dry)  → TX 0x02
  //   RX 0x02 (fan)  → TX 0x07
  //   RX 0x05 (auto) → TX 0x08
  static const uint8_t mode_map[] = {0, 0x03, 0x07, 0x02, 0, 0x08};
  uint8_t tx_mode = (state.mode < sizeof(mode_map)) ? mode_map[state.mode] : 0x03;
  cmd[8] = tx_mode;
  cmd[8] |= (state.turbo  ? 1 : 0) << 6;
  cmd[8] |= (state.mute   ? 1 : 0) << 7;
  cmd[8] |= (state.health ? 1 : 0) << 4;

  // Byte 9: temperature (31 - target = encoded)
  uint8_t clamped_temp = std::max(uint8_t(16), std::min(uint8_t(31), state.target_temp));
  cmd[9] = 31 - clamped_temp;

  // Byte 10: [x, x, swing_v(3), fan(3)]
  // Fan mapping: RX→TX
  //   0 (auto) → 0, 1 (low) → 2, 2 (med) → 3, 3 (high) → 5
  static const uint8_t fan_map[] = {0, 2, 3, 5};
  uint8_t tx_fan = (state.fan < sizeof(fan_map)) ? fan_map[state.fan] : 0;
  cmd[10] = tx_fan;
  if (state.swing_v) {
    cmd[10] |= 0x07 << 3;  // bits 3,4,5 = vswing
  }

  // Byte 14: [x, x, halfdegree, x, swingh, x, x, x]
  cmd[14] = 0;
  if (state.swing_h) {
    cmd[14] |= (1 << 3);  // Only bit 3 is hswing (fixed Ryan's bug)
  }

  // Byte 19: sleep
  cmd[19] = state.sleep ? 1 : 0;
}

void TclMinisplit::send_heartbeat_if_needed_() {
  unsigned long now = millis();
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
  char str[200] = {0};
  char *p = str;
  size_t max_bytes = std::min(len, size_t(60));  // Prevent overflow
  for (size_t i = 0; i < max_bytes; i++) {
    p += sprintf(p, "%02X ", data[i]);
  }
  ESP_LOGD(TAG, "%s: %s", prefix, str);
}

}  // namespace tcl_minisplit
}  // namespace esphome
