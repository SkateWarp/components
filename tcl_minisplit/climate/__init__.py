from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit
from esphome.const import CONF_ID

DEPENDENCIES = ["tcl_minisplit"]

TclMinisplitClimate = tcl_minisplit_ns.class_(
    "TclMinisplitClimate", cg.Component, climate.Climate
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(TclMinisplitClimate).extend(
        {
            cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
