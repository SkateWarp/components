#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

static const char *const TAG = "tcl_minisplit.switch";

// ─── AC Control Switches ────────────────────────────────────────

void TclMinisplitSwitch::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    if (!state.power) return;  // Don't update switches when AC is off
    bool new_state = false;
    switch (this->purpose_) {
      case SWITCH_DISPLAY:    new_state = state.display;    break;
      case SWITCH_BEEP:       new_state = state.beep;       break;
      case SWITCH_HEALTH:     new_state = state.health;     break;
      case SWITCH_FAHRENHEIT: new_state = state.fahrenheit;  break;
    }
    if (this->state != new_state) {
      this->publish_state(new_state);
    }
  });
}

void TclMinisplitSwitch::write_state(bool state) {
  // Update live state immediately — critical for TX-only fields (beep, fahrenheit)
  // that RX never overwrites, so state_ must track the user's choice.
  // For RX-reflected fields (display, health), the next RX will correct if needed.
  AcState &live = this->parent_->get_state();
  switch (this->purpose_) {
    case SWITCH_DISPLAY:    live.display = state;    break;
    case SWITCH_BEEP:       live.beep = state;       break;
    case SWITCH_HEALTH:     live.health = state;     break;
    case SWITCH_FAHRENHEIT: live.fahrenheit = state;  break;
  }

  // Create pending command from (now updated) state
  this->parent_->prepare_pending_state();
  this->publish_state(state);
}

// ─── Persistence Switch ─────────────────────────────────────────

void TclPersistenceSwitch::setup() {
  // Publish the current persistence state (loaded from NVS in hub setup())
  this->publish_state(this->parent_->get_persistence_enabled());
}

void TclPersistenceSwitch::write_state(bool state) {
  ESP_LOGI(TAG, "Persistence %s", state ? "enabled" : "disabled");
  this->parent_->set_persistence_enabled(state);
  this->publish_state(state);
}

}  // namespace tcl_minisplit
}  // namespace esphome
