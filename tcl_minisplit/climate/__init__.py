from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit
from esphome.const import CONF_ID

DEPENDENCIES = ["tcl_minisplit"]

CONF_SUPPORTS_HEAT = "supports_heat"
CONF_SUPPORTS_SWING_H = "supports_swing_h"
CONF_SUPPORTS_HALF_DEGREE = "supports_half_degree"
CONF_SUPPORTS_FIVE_FAN = "supports_five_fan_speeds"

TclMinisplitClimate = tcl_minisplit_ns.class_(
    "TclMinisplitClimate", cg.Component, climate.Climate
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(TclMinisplitClimate).extend(
        {
            cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
            cv.Optional(CONF_SUPPORTS_HEAT, default=False): cv.boolean,
            cv.Optional(CONF_SUPPORTS_SWING_H, default=False): cv.boolean,
            cv.Optional(CONF_SUPPORTS_HALF_DEGREE, default=False): cv.boolean,
            cv.Optional(CONF_SUPPORTS_FIVE_FAN, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    cg.add(var.set_supports_swing_h(config[CONF_SUPPORTS_SWING_H]))
    cg.add(var.set_supports_half_degree(config[CONF_SUPPORTS_HALF_DEGREE]))
    # Five fan speeds is set on the hub (affects protocol TX/RX)
    cg.add(parent.set_five_fan_speeds(config[CONF_SUPPORTS_FIVE_FAN]))
