from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit

DEPENDENCIES = ["tcl_minisplit"]

CONF_DEEP_SLEEP = "deep_sleep"
CONF_CLEAN_FILTER = "clean_filter"
CONF_TIMER_ACTIVE = "timer_active"

TclMinisplitBinarySensor = tcl_minisplit_ns.class_(
    "TclMinisplitBinarySensor", binary_sensor.BinarySensor, cg.Component
)
TclBinarySensorPurpose = tcl_minisplit_ns.enum("TclBinarySensorPurpose")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        cv.Optional(CONF_DEEP_SLEEP): binary_sensor.binary_sensor_schema(
            TclMinisplitBinarySensor, icon="mdi:sleep"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_CLEAN_FILTER): binary_sensor.binary_sensor_schema(
            TclMinisplitBinarySensor, icon="mdi:air-filter"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_TIMER_ACTIVE): binary_sensor.binary_sensor_schema(
            TclMinisplitBinarySensor, icon="mdi:timer-outline"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])

    if conf := config.get(CONF_DEEP_SLEEP):
        var = await binary_sensor.new_binary_sensor(
            conf, parent, TclBinarySensorPurpose.BSENSOR_DEEP_SLEEP
        )
        await cg.register_component(var, conf)

    if conf := config.get(CONF_CLEAN_FILTER):
        var = await binary_sensor.new_binary_sensor(
            conf, parent, TclBinarySensorPurpose.BSENSOR_CLEAN_FILTER
        )
        await cg.register_component(var, conf)

    if conf := config.get(CONF_TIMER_ACTIVE):
        var = await binary_sensor.new_binary_sensor(
            conf, parent, TclBinarySensorPurpose.BSENSOR_TIMER_ACTIVE
        )
        await cg.register_component(var, conf)
