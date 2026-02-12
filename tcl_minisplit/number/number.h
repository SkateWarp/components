#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace tcl_minisplit {

enum TclNumberPurpose : uint8_t {
  NUMBER_GEN,
  NUMBER_SLEEP_MODE,
  NUMBER_VSWING_POS,
  NUMBER_HSWING_POS,
};

class TclMinisplitNumber : public Component, public number::Number {
 public:
  void set_parent(TclMinisplit *parent) { parent_ = parent; }
  void set_purpose(TclNumberPurpose purpose) { purpose_ = purpose; }

 protected:
  void setup() override;
  void control(float value) override;
  TclMinisplit *parent_{nullptr};
  TclNumberPurpose purpose_{NUMBER_ON_TIMER};
};

}  // namespace tcl_minisplit
}  // namespace esphome
