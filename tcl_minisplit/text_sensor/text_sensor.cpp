#include "text_sensor.h"
#include <cstdio>

namespace esphome {
namespace tcl_minisplit {

void TclMinisplitTextSensor::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    std::string new_text = this->get_text_from_state_(state);
    if (this->get_state() != new_text) {
      this->publish_state(new_text);
    }
  });
}

std::string TclMinisplitTextSensor::get_text_from_state_(const AcState &state) {
  if (this->purpose_ == TSENSOR_FAN_SPEED) {
    uint8_t speed = state.fan_speed_raw;
    if (speed == 0)   return "OFF";
    if (speed < 86)   return "LOW";
    if (speed < 99)   return "MEDIUM";
    if (speed < 111)  return "HIGH";
    if (speed > 117)  return "TURBO";
    return "HIGH";  // 111-117 range
  }

  if (this->purpose_ == TSENSOR_FAULT) {
    if (state.fault == 0)
      return "OK";
    char buf[16];
    snprintf(buf, sizeof(buf), "FAULT 0x%02X", state.fault);
    return std::string(buf);
  }

  return "";
}

}  // namespace tcl_minisplit
}  // namespace esphome
