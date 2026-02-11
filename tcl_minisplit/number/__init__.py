from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit

DEPENDENCIES = ["tcl_minisplit"]

CONF_ON_TIMER = "on_timer"
CONF_OFF_TIMER = "off_timer"
CONF_GEN = "gen"

TclMinisplitNumber = tcl_minisplit_ns.class_(
    "TclMinisplitNumber", number.Number, cg.Component
)
TclNumberPurpose = tcl_minisplit_ns.enum("TclNumberPurpose")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        cv.Optional(CONF_ON_TIMER): number.number_schema(
            TclMinisplitNumber,
            icon="mdi:timer-play-outline",
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_OFF_TIMER): number.number_schema(
            TclMinisplitNumber,
            icon="mdi:timer-stop-outline",
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_GEN): number.number_schema(
            TclMinisplitNumber,
            icon="mdi:cog-outline",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])

    if conf := config.get(CONF_ON_TIMER):
        var = await number.new_number(
            conf,
            min_value=0,
            max_value=24,
            step=1,
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_purpose(TclNumberPurpose.NUMBER_ON_TIMER))
        await cg.register_component(var, conf)

    if conf := config.get(CONF_OFF_TIMER):
        var = await number.new_number(
            conf,
            min_value=0,
            max_value=24,
            step=1,
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_purpose(TclNumberPurpose.NUMBER_OFF_TIMER))
        await cg.register_component(var, conf)

    if conf := config.get(CONF_GEN):
        var = await number.new_number(
            conf,
            min_value=0,
            max_value=3,
            step=1,
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_purpose(TclNumberPurpose.NUMBER_GEN))
        await cg.register_component(var, conf)
