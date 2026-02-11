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

  // Timer values come from RX state
  this->parent_->register_listener([this](const AcState &state) {
    float new_val = 0;
    switch (this->purpose_) {
      case NUMBER_ON_TIMER:  new_val = state.on_timer_hours; break;
      case NUMBER_OFF_TIMER: new_val = state.off_timer_hours; break;
      default: break;
    }
    if (this->state != new_val) {
      this->publish_state(new_val);
    }
  });
}

void TclMinisplitNumber::control(float value) {
  uint8_t hours = static_cast<uint8_t>(value);

  // Gen is hub-level, not per-command
  if (this->purpose_ == NUMBER_GEN) {
    this->parent_->set_gen(static_cast<uint8_t>(value));
    ESP_LOGD(TAG, "Gen set to %d", static_cast<int>(value));
    this->publish_state(value);
    return;
  }

  this->parent_->prepare_pending_state();
  AcState *pending = this->parent_->get_pending_state();
  if (!pending)
    return;

  switch (this->purpose_) {
    case NUMBER_ON_TIMER:
      pending->on_timer_hours = hours;
      pending->on_timer_enabled = (hours > 0);
      ESP_LOGD(TAG, "On timer: %dh (%s)", hours, hours > 0 ? "enabled" : "disabled");
      break;
    case NUMBER_OFF_TIMER:
      pending->off_timer_hours = hours;
      pending->off_timer_enabled = (hours > 0);
      ESP_LOGD(TAG, "Off timer: %dh (%s)", hours, hours > 0 ? "enabled" : "disabled");
      break;
    default:
      break;
  }

  this->publish_state(value);
}

}  // namespace tcl_minisplit
}  // namespace esphome
