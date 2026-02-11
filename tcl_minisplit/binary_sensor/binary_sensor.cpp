#include "binary_sensor.h"

namespace esphome {
namespace tcl_minisplit {

void TclMinisplitBinarySensor::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    bool new_state = false;
    switch (this->purpose_) {
      case BSENSOR_DEEP_SLEEP:    new_state = state.deep_sleep; break;
      case BSENSOR_CLEAN_FILTER:  new_state = state.clean_filter; break;
      case BSENSOR_TIMER_ACTIVE:  new_state = state.timer_active; break;
    }
    if (this->state != new_state) {
      this->publish_state(new_state);
    }
  });
}

}  // namespace tcl_minisplit
}  // namespace esphome
