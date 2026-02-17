#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

static const char *const TAG = "tcl_minisplit.number";

void TclMinisplitNumber::setup() {
  // Gen is hub-level, publish initial value once
  if (this->purpose_ == NUMBER_GEN) {
    this->publish_state(static_cast<float>(this->parent_->get_gen()));
    return;
  }

  // Sleep mode and vane positions come from state
  this->parent_->register_listener([this](const AcState &state) {
    float new_val = 0;
    switch (this->purpose_) {
      case NUMBER_SLEEP_MODE: new_val = static_cast<float>(state.sleep_mode); break;
      case NUMBER_VSWING_POS: new_val = static_cast<float>(state.vswing_pos_tx); break;
      case NUMBER_HSWING_POS: new_val = static_cast<float>(state.hswing_pos_tx); break;
      default: break;
    }
    if (this->state != new_val) {
      this->publish_state(new_val);
    }
  });
}

void TclMinisplitNumber::control(float value) {
  uint8_t val = static_cast<uint8_t>(value);

  // Gen is hub-level, not per-command
  if (this->purpose_ == NUMBER_GEN) {
    this->parent_->set_gen(val);
    ESP_LOGD(TAG, "Gen set to %d", val);
    this->publish_state(value);
    return;
  }

  // Update live state immediately — these are TX-only fields that RX never
  // overwrites, so state_ must track the user's choice for future commands.
  AcState &live = this->parent_->get_state();
  switch (this->purpose_) {
    case NUMBER_SLEEP_MODE:
      live.sleep_mode = val & 0x03;
      ESP_LOGD(TAG, "Sleep mode: %d", val);
      break;
    case NUMBER_VSWING_POS:
      live.vswing_pos_tx = val;
      ESP_LOGD(TAG, "VSwing pos TX: 0x%02X", val);
      break;
    case NUMBER_HSWING_POS:
      live.hswing_pos_tx = val;
      ESP_LOGD(TAG, "HSwing pos TX: 0x%02X", val);
      break;
    default:
      break;
  }

  // Create pending command from (now updated) state
  this->parent_->prepare_pending_state();
  this->publish_state(value);
}

}  // namespace tcl_minisplit
}  // namespace esphome
