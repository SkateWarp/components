#pragma once

#include "esphome/components/tcl_minisplit/tcl_minisplit.h"
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace tcl_minisplit {

class TclMinisplitClimate : public Component, public climate::Climate {
 public:
  TclMinisplitClimate(TclMinisplit *parent) : parent_(parent) {}

 protected:
  void setup() override;
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  // Conversion helpers
  climate::ClimateMode ac_to_esphome_mode_(uint8_t mode, bool power);
  void esphome_to_ac_mode_(climate::ClimateMode mode, uint8_t &ac_mode, bool &ac_power);
  climate::ClimateFanMode ac_to_esphome_fan_(uint8_t fan, bool mute);
  void esphome_to_ac_fan_(climate::ClimateFanMode fan, uint8_t &ac_fan, bool &ac_mute);
  climate::ClimateSwingMode ac_to_esphome_swing_(bool h, bool v);
  climate::ClimateAction ac_to_esphome_action_(uint8_t compressor_state, bool power);
  climate::ClimatePreset ac_to_esphome_preset_(bool eco, bool turbo, bool sleep);

  TclMinisplit *parent_;
};

}  // namespace tcl_minisplit
}  // namespace esphome
