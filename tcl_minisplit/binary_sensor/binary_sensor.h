#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace tcl_minisplit {

enum TclBinarySensorPurpose : uint8_t {
  BSENSOR_DEEP_SLEEP,
};

class TclMinisplitBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  TclMinisplitBinarySensor(TclMinisplit *parent, TclBinarySensorPurpose purpose)
      : parent_(parent), purpose_(purpose) {}
  void setup() override;

 protected:
  TclMinisplit *parent_;
  TclBinarySensorPurpose purpose_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
