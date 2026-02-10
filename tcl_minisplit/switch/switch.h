#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace tcl_minisplit {

enum TclSwitchPurpose : uint8_t {
  SWITCH_DISPLAY,
  SWITCH_BEEP,
  SWITCH_HEALTH,
};

class TclMinisplitSwitch : public Component, public switch_::Switch {
 public:
  TclMinisplitSwitch(TclMinisplit *parent, TclSwitchPurpose purpose)
      : parent_(parent), purpose_(purpose) {}

 protected:
  void setup() override;
  void write_state(bool state) override;
  TclMinisplit *parent_;
  TclSwitchPurpose purpose_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
