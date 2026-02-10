#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace tcl_minisplit {

enum TclSensorPurpose : uint8_t {
  SENSOR_COMPRESSOR_CURRENT,
  SENSOR_SUPPLY_VOLTAGE,
  SENSOR_PIPE_IN,
  SENSOR_PIPE_OUT,
  SENSOR_OUTSIDE_MOTOR,
  SENSOR_FAN_SPEED_RAW,
  SENSOR_FAULT_CODE,
};

class TclMinisplitSensor : public Component, public sensor::Sensor {
 public:
  TclMinisplitSensor(TclMinisplit *parent, TclSensorPurpose purpose)
      : parent_(parent), purpose_(purpose) {}
  void setup() override;

 protected:
  float get_value_from_state_(const AcState &state);
  TclMinisplit *parent_;
  TclSensorPurpose purpose_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
