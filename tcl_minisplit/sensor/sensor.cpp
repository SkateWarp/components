#include "sensor.h"

namespace esphome {
namespace tcl_minisplit {

void TclMinisplitSensor::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    float new_val = this->get_value_from_state_(state);
    if (this->get_state() != new_val || std::isnan(this->get_state())) {
      this->publish_state(new_val);
    }
  });
}

float TclMinisplitSensor::get_value_from_state_(const AcState &state) {
  switch (this->purpose_) {
    case SENSOR_COMPRESSOR_CURRENT: return state.compressor_current;
    case SENSOR_SUPPLY_VOLTAGE:     return static_cast<float>(state.supply_voltage);
    case SENSOR_PIPE_IN:            return static_cast<float>(state.pipe_in) - 32.0f;
    case SENSOR_PIPE_OUT:           return static_cast<float>(state.pipe_out) - 32.0f;
    case SENSOR_OUTSIDE_MOTOR:      return static_cast<float>(state.outside_motor);
    case SENSOR_FAN_SPEED_RAW:      return static_cast<float>(state.fan_speed_raw);
    case SENSOR_FAULT_CODE:         return static_cast<float>(state.fault);
    default:                        return NAN;
  }
}

}  // namespace tcl_minisplit
}  // namespace esphome
