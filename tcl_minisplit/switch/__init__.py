from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import ICON_BRIGHTNESS_5
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit

DEPENDENCIES = ["tcl_minisplit"]

CONF_DISPLAY = "display_switch"
CONF_BEEP = "beep"
CONF_HEALTH = "health"
CONF_FAHRENHEIT = "fahrenheit"
CONF_PERSISTENCE = "persistence"

TclMinisplitSwitch = tcl_minisplit_ns.class_(
    "TclMinisplitSwitch", switch.Switch, cg.Component
)
TclPersistenceSwitch = tcl_minisplit_ns.class_(
    "TclPersistenceSwitch", switch.Switch, cg.Component
)
TclSwitchPurpose = tcl_minisplit_ns.enum("TclSwitchPurpose")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            TclMinisplitSwitch, icon=ICON_BRIGHTNESS_5
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_BEEP): switch.switch_schema(
            TclMinisplitSwitch,
            icon="mdi:volume-high",
            default_restore_mode="RESTORE_DEFAULT_ON",
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_HEALTH): switch.switch_schema(
            TclMinisplitSwitch, icon="mdi:medical-bag"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_FAHRENHEIT): switch.switch_schema(
            TclMinisplitSwitch, icon="mdi:temperature-fahrenheit"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_PERSISTENCE): switch.switch_schema(
            TclPersistenceSwitch,
            icon="mdi:content-save-cog",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])

    if conf := config.get(CONF_DISPLAY):
        var = await switch.new_switch(conf, parent, TclSwitchPurpose.SWITCH_DISPLAY)
        await cg.register_component(var, conf)

    if conf := config.get(CONF_BEEP):
        var = await switch.new_switch(conf, parent, TclSwitchPurpose.SWITCH_BEEP)
        await cg.register_component(var, conf)

    if conf := config.get(CONF_HEALTH):
        var = await switch.new_switch(conf, parent, TclSwitchPurpose.SWITCH_HEALTH)
        await cg.register_component(var, conf)

    if conf := config.get(CONF_FAHRENHEIT):
        var = await switch.new_switch(conf, parent, TclSwitchPurpose.SWITCH_FAHRENHEIT)
        await cg.register_component(var, conf)

    if conf := config.get(CONF_PERSISTENCE):
        var = await switch.new_switch(conf, parent)
        await cg.register_component(var, conf)
