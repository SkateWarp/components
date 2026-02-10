#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace tcl_minisplit {

enum TclTextSensorPurpose : uint8_t {
  TSENSOR_FAN_SPEED,
  TSENSOR_FAULT,
};

class TclMinisplitTextSensor : public Component, public text_sensor::TextSensor {
 public:
  TclMinisplitTextSensor(TclMinisplit *parent, TclTextSensorPurpose purpose)
      : parent_(parent), purpose_(purpose) {}
  void setup() override;

 protected:
  std::string get_text_from_state_(const AcState &state);
  TclMinisplit *parent_;
  TclTextSensorPurpose purpose_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
