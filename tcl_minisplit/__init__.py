import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@anyelo"]
AUTO_LOAD = ["climate", "sensor", "binary_sensor", "switch", "text_sensor"]

CONF_TCL_MINISPLIT_ID = "tcl_minisplit_id"

tcl_minisplit_ns = cg.esphome_ns.namespace("tcl_minisplit")
TclMinisplit = tcl_minisplit_ns.class_("TclMinisplit", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TclMinisplit),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
