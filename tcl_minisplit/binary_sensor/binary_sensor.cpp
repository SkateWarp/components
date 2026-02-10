#include "binary_sensor.h"

namespace esphome {
namespace tcl_minisplit {

void TclMinisplitBinarySensor::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    if (this->purpose_ == BSENSOR_DEEP_SLEEP) {
      bool new_state = state.deep_sleep;
      if (this->state != new_state) {
        this->publish_state(new_state);
      }
    }
  });
}

}  // namespace tcl_minisplit
}  // namespace esphome
