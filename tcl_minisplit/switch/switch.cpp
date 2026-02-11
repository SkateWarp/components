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
  this->parent_->prepare_pending_state();
  AcState *pending = this->parent_->get_pending_state();
  if (!pending)
    return;

  switch (this->purpose_) {
    case SWITCH_DISPLAY:    pending->display = state;    break;
    case SWITCH_BEEP:       pending->beep = state;       break;
    case SWITCH_HEALTH:     pending->health = state;     break;
    case SWITCH_FAHRENHEIT: pending->fahrenheit = state;  break;
  }

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
