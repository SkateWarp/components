from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit

DEPENDENCIES = ["tcl_minisplit"]

CONF_FAN_SPEED = "fan_speed"
CONF_FAULT = "fault"

TclMinisplitTextSensor = tcl_minisplit_ns.class_(
    "TclMinisplitTextSensor", text_sensor.TextSensor, cg.Component
)
TclTextSensorPurpose = tcl_minisplit_ns.enum("TclTextSensorPurpose")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        cv.Optional(CONF_FAN_SPEED): text_sensor.text_sensor_schema(
            TclMinisplitTextSensor, icon="mdi:wind-power"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_FAULT): text_sensor.text_sensor_schema(
            TclMinisplitTextSensor, icon="mdi:alert-circle-outline"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])

    if conf := config.get(CONF_FAN_SPEED):
        var = await text_sensor.new_text_sensor(
            conf, parent, TclTextSensorPurpose.TSENSOR_FAN_SPEED
        )
        await cg.register_component(var, conf)

    if conf := config.get(CONF_FAULT):
        var = await text_sensor.new_text_sensor(
            conf, parent, TclTextSensorPurpose.TSENSOR_FAULT
        )
        await cg.register_component(var, conf)
