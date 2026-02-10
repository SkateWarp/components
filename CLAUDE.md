# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESPHome external component for controlling TCL mini-split air conditioners via UART serial communication. It provides Home Assistant integration through ESPHome's component system.

## Architecture

### Core Component (`tcl_minisplit/`)

**TclMinisplit** ([tcl_minisplit.h](tcl_minisplit/tcl_minisplit.h)) - Main hub class that:
- Inherits from `uart::UARTDevice` for serial communication
- Maintains `AcState` struct with all AC parameters (power, mode, temperature, fan, etc.)
- Implements state machine parser for RX packets (61-byte status responses)
- Builds TX packets (35 bytes) with mode/fan mapping quirks between RX and TX protocols
- Uses listener pattern to notify sub-components of state changes
- Sends heartbeat every 500ms to keep connection alive

### Serial Protocol

- Packet format: `[0xBB] [ver_hi] [ver_lo] [type] [len] [payload...] [xor_checksum]`
- RX status packets: 61 bytes, type 0x04
- TX command packets: 35 bytes, type 0x03
- Mode values differ between RX and TX (e.g., RX cool=0x01 → TX cool=0x03)
- Fan values also require mapping (e.g., RX low=1 → TX low=2)

### Sub-Components

Each exposes entities to Home Assistant via parent `TclMinisplit` reference:

| Directory | Class | Purpose |
|-----------|-------|---------|
| `climate/` | TclMinisplitClimate | Main thermostat control (mode, temp, fan, swing, presets) |
| `sensor/` | TclMinisplitSensor | Numeric values: compressor current, voltage, pipe temps, fault codes |
| `switch/` | TclMinisplitSwitch | Boolean controls: display, beep, health mode |
| `binary_sensor/` | TclMinisplitBinarySensor | Deep sleep detection |
| `text_sensor/` | TclMinisplitTextSensor | Human-readable fan speed, fault descriptions |

### State Flow

1. Parent calls `register_listener()` on sub-components during setup
2. Serial RX triggers `parse_rx_packet_()` → updates `state_` → `notify_listeners_()`
3. Sub-components receive callback with new `AcState`, update their entities
4. Commands: sub-component calls `prepare_pending_state()`, modifies `pending_state_`, next `loop()` sends TX

## Development

This component is compiled by ESPHome. To use in a project:

```yaml
external_components:
  - source: /path/to/components

uart:
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600

tcl_minisplit:

climate:
  - platform: tcl_minisplit
    name: "AC"
```

No standalone build/test commands - validation happens through ESPHome compilation.
