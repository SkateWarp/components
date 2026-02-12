#include "climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_minisplit {

static const char *const TAG = "tcl_climate";

void TclMinisplitClimate::setup() {
  this->parent_->register_listener([this](const AcState &state) {
    bool changed = false;

    // Mode
    auto new_mode = this->ac_to_esphome_mode_(state.mode, state.power);
    if (this->mode != new_mode) {
      this->mode = new_mode;
      changed = true;
    }

    // Target temperature
    if (this->target_temperature != state.target_temp) {
      this->target_temperature = state.target_temp;
      changed = true;
    }

    // Current temperature
    if (!std::isnan(state.current_temp) &&
        (std::isnan(this->current_temperature) ||
         std::abs(this->current_temperature - state.current_temp) > 0.01f)) {
      this->current_temperature = state.current_temp;
      changed = true;
    }

    // Fan mode (includes QUIET via mute flag)
    auto new_fan = this->ac_to_esphome_fan_(state.fan, state.mute);
    if (this->fan_mode != new_fan) {
      this->fan_mode = new_fan;
      changed = true;
    }

    // Swing mode
    auto new_swing = this->ac_to_esphome_swing_(state.swing_h, state.swing_v);
    if (this->swing_mode != new_swing) {
      this->swing_mode = new_swing;
      changed = true;
    }

    // Action - fixed: properly handles each compressor state value
    auto new_action = this->ac_to_esphome_action_(state.compressor_state, state.power);
    if (this->action != new_action) {
      this->action = new_action;
      changed = true;
    }

    // Preset
    auto new_preset = this->ac_to_esphome_preset_(state.eco, state.turbo, state.sleep);
    if (this->preset != new_preset) {
      this->preset = new_preset;
      changed = true;
    }

    if (changed)
      this->publish_state();
  });

  // Restore previous state if available
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  }
}

void TclMinisplitClimate::control(const climate::ClimateCall &call) {
  this->parent_->prepare_pending_state();
  AcState *pending = this->parent_->get_pending_state();
  if (!pending)
    return;

  // Mode
  if (call.get_mode().has_value()) {
    climate::ClimateMode mode = *call.get_mode();
    if (mode == climate::CLIMATE_MODE_OFF) {
      pending->power = false;
      pending->sleep_mode = 0;
      pending->turbo = false;
      pending->eco = false;
    } else {
      pending->power = true;
      this->esphome_to_ac_mode_(mode, pending->mode, pending->power);
    }
  }

  // Temperature
  if (call.get_target_temperature().has_value()) {
    pending->target_temp = *call.get_target_temperature();
  }

  // Fan mode
  if (call.get_fan_mode().has_value()) {
    this->esphome_to_ac_fan_(*call.get_fan_mode(), pending->fan, pending->mute);
    pending->turbo = false;  // Clear turbo when changing fan
  }

  // Swing
  if (call.get_swing_mode().has_value()) {
    auto swing = *call.get_swing_mode();
    pending->swing_v = (swing == climate::CLIMATE_SWING_VERTICAL || swing == climate::CLIMATE_SWING_BOTH);
    pending->swing_h = (swing == climate::CLIMATE_SWING_HORIZONTAL || swing == climate::CLIMATE_SWING_BOTH);
  }

  // Preset (independent if, not else-if — fixes Anyelo's original bug)
  if (call.get_preset().has_value()) {
    auto preset = *call.get_preset();
    pending->eco = false;
    pending->turbo = false;
    pending->sleep_mode = 0;
    pending->mute = false;

    switch (preset) {
      case climate::CLIMATE_PRESET_ECO:
        pending->eco = true;
        pending->fan = 0;
        break;
      case climate::CLIMATE_PRESET_BOOST:
        pending->turbo = true;
        pending->fan = 3;
        break;
      case climate::CLIMATE_PRESET_SLEEP:
        pending->sleep_mode = 1;  // default sleep mode
        break;
      case climate::CLIMATE_PRESET_NONE:
      default:
        pending->fan = 0;
        break;
    }
  }
}

climate::ClimateTraits TclMinisplitClimate::traits() {
  auto traits = climate::ClimateTraits();

  // Feature flags (replaces deprecated set_supports_* methods)
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);

  // Modes
  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
  if (this->supports_heat_) {
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  }

  // Fan modes — always standard ESPHome modes
  // 3-speed AC: LOW=speed1, MEDIUM=speed2, HIGH=speed3
  // 5-speed AC: LOW=speed1, MEDIUM=speed3, HIGH=speed5 (same UI, different wire mapping)
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_QUIET);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);

  // Swing modes
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  if (this->supports_swing_h_) {
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
  }

  // Presets
  traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
  traits.add_supported_preset(climate::CLIMATE_PRESET_BOOST);
  traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);
  traits.add_supported_preset(climate::CLIMATE_PRESET_SLEEP);

  traits.set_visual_min_temperature(16.0);
  traits.set_visual_max_temperature(31.0);
  traits.set_visual_target_temperature_step(this->supports_half_degree_ ? 0.5f : 1.0f);
  return traits;
}

// ─── Conversion helpers ─────────────────────────────────────────

// TCL RX modes: 0x01=cool, 0x02=fan, 0x03=dry, 0x04=heat, 0x05=auto
climate::ClimateMode TclMinisplitClimate::ac_to_esphome_mode_(uint8_t mode, bool power) {
  if (!power) return climate::CLIMATE_MODE_OFF;
  switch (mode) {
    case 0x01: return climate::CLIMATE_MODE_COOL;
    case 0x02: return climate::CLIMATE_MODE_FAN_ONLY;
    case 0x03: return climate::CLIMATE_MODE_DRY;
    case 0x04: return this->supports_heat_ ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_AUTO;
    case 0x05: return climate::CLIMATE_MODE_AUTO;
    default:
      ESP_LOGW(TAG, "Unknown AC mode: 0x%02X", mode);
      return climate::CLIMATE_MODE_AUTO;
  }
}

void TclMinisplitClimate::esphome_to_ac_mode_(climate::ClimateMode mode, uint8_t &ac_mode, bool &ac_power) {
  ac_power = true;
  switch (mode) {
    case climate::CLIMATE_MODE_OFF:      ac_power = false; ac_mode = 0x01; break;
    case climate::CLIMATE_MODE_COOL:     ac_mode = 0x01; break;
    case climate::CLIMATE_MODE_FAN_ONLY: ac_mode = 0x02; break;
    case climate::CLIMATE_MODE_DRY:      ac_mode = 0x03; break;
    case climate::CLIMATE_MODE_HEAT:     ac_mode = 0x04; break;
    case climate::CLIMATE_MODE_AUTO:     ac_mode = 0x05; break;
    default:                             ac_mode = 0x01; break;
  }
}

climate::ClimateFanMode TclMinisplitClimate::ac_to_esphome_fan_(uint8_t fan, bool mute) {
  if (mute) return climate::CLIMATE_FAN_QUIET;
  switch (fan) {
    case 0: return climate::CLIMATE_FAN_AUTO;
    case 1: return climate::CLIMATE_FAN_LOW;
    case 2: return climate::CLIMATE_FAN_MEDIUM;
    case 3: return climate::CLIMATE_FAN_HIGH;
    default: return climate::CLIMATE_FAN_AUTO;
  }
}

void TclMinisplitClimate::esphome_to_ac_fan_(climate::ClimateFanMode fan, uint8_t &ac_fan, bool &ac_mute) {
  ac_mute = false;
  switch (fan) {
    case climate::CLIMATE_FAN_QUIET:  ac_fan = 1; ac_mute = true; break;
    case climate::CLIMATE_FAN_AUTO:   ac_fan = 0; break;
    case climate::CLIMATE_FAN_LOW:    ac_fan = 1; break;
    case climate::CLIMATE_FAN_MEDIUM: ac_fan = 2; break;
    case climate::CLIMATE_FAN_HIGH:   ac_fan = 3; break;
    default:                          ac_fan = 0; break;
  }
}

climate::ClimateSwingMode TclMinisplitClimate::ac_to_esphome_swing_(bool h, bool v) {
  if (h && v) return climate::CLIMATE_SWING_BOTH;
  if (v) return climate::CLIMATE_SWING_VERTICAL;
  if (h) return climate::CLIMATE_SWING_HORIZONTAL;
  return climate::CLIMATE_SWING_OFF;
}

// Fixed: Anyelo's original had `== (0x80 || 0xC0)` which evaluated to `== 1`
climate::ClimateAction TclMinisplitClimate::ac_to_esphome_action_(uint8_t compressor_state, bool power) {
  if (!power) return climate::CLIMATE_ACTION_OFF;
  switch (compressor_state) {
    case 0x8A: return climate::CLIMATE_ACTION_COOLING;
    case 0xCA: return climate::CLIMATE_ACTION_HEATING;  // In case unit reports it
    case 0x80:
    case 0xC0: return climate::CLIMATE_ACTION_IDLE;
    default:   return climate::CLIMATE_ACTION_IDLE;
  }
}

climate::ClimatePreset TclMinisplitClimate::ac_to_esphome_preset_(bool eco, bool turbo, bool sleep) {
  if (eco)   return climate::CLIMATE_PRESET_ECO;
  if (turbo) return climate::CLIMATE_PRESET_BOOST;
  if (sleep) return climate::CLIMATE_PRESET_SLEEP;
  return climate::CLIMATE_PRESET_NONE;
}

}  // namespace tcl_minisplit
}  // namespace esphome
