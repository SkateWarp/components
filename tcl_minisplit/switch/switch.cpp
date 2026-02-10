#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

void TclMinisplitSwitch::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    bool new_state = false;
    switch (this->purpose_) {
      case SWITCH_DISPLAY: new_state = state.display; break;
      case SWITCH_BEEP:    new_state = state.beep;    break;
      case SWITCH_HEALTH:  new_state = state.health;  break;
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
    case SWITCH_DISPLAY: pending->display = state; break;
    case SWITCH_BEEP:    pending->beep = state;    break;
    case SWITCH_HEALTH:  pending->health = state;  break;
  }

  this->publish_state(state);
}

}  // namespace tcl_minisplit
}  // namespace esphome
