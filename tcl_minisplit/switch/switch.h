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
  SWITCH_FAHRENHEIT,
};

// Switches that control the AC unit (send TX commands)
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

// Persistence switch — controls the hub's save/restore behavior, not the AC
class TclPersistenceSwitch : public Component, public switch_::Switch {
 public:
  TclPersistenceSwitch(TclMinisplit *parent) : parent_(parent) {}

 protected:
  void setup() override;
  void write_state(bool state) override;
  TclMinisplit *parent_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
