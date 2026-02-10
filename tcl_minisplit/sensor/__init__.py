from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from .. import tcl_minisplit_ns, CONF_TCL_MINISPLIT_ID, TclMinisplit

DEPENDENCIES = ["tcl_minisplit"]

CONF_COMPRESSOR_CURRENT = "compressor_current"
CONF_SUPPLY_VOLTAGE = "supply_voltage"
CONF_PIPE_IN = "pipe_in_temperature"
CONF_PIPE_OUT = "pipe_out_temperature"
CONF_OUTSIDE_MOTOR = "outside_motor"
CONF_FAN_SPEED_RAW = "fan_speed_raw"
CONF_FAULT_CODE = "fault_code"

TclMinisplitSensor = tcl_minisplit_ns.class_(
    "TclMinisplitSensor", sensor.Sensor, cg.Component
)
TclSensorPurpose = tcl_minisplit_ns.enum("TclSensorPurpose")

SENSOR_TYPES = {
    CONF_COMPRESSOR_CURRENT: {
        "purpose": "SENSOR_COMPRESSOR_CURRENT",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:current-ac",
            unit_of_measurement="A",
            accuracy_decimals=1,
            state_class="measurement",
            device_class="current",
        ),
    },
    CONF_SUPPLY_VOLTAGE: {
        "purpose": "SENSOR_SUPPLY_VOLTAGE",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:flash",
            unit_of_measurement="V",
            state_class="measurement",
            device_class="voltage",
        ),
    },
    CONF_PIPE_IN: {
        "purpose": "SENSOR_PIPE_IN",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:pipe",
            unit_of_measurement="°C",
            state_class="measurement",
            device_class="temperature",
        ),
    },
    CONF_PIPE_OUT: {
        "purpose": "SENSOR_PIPE_OUT",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:pipe",
            unit_of_measurement="°C",
            state_class="measurement",
            device_class="temperature",
        ),
    },
    CONF_OUTSIDE_MOTOR: {
        "purpose": "SENSOR_OUTSIDE_MOTOR",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:heat-pump",
            state_class="measurement",
        ),
    },
    CONF_FAN_SPEED_RAW: {
        "purpose": "SENSOR_FAN_SPEED_RAW",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:fan",
            state_class="measurement",
        ),
    },
    CONF_FAULT_CODE: {
        "purpose": "SENSOR_FAULT_CODE",
        "schema": sensor.sensor_schema(
            TclMinisplitSensor,
            icon="mdi:alert-circle",
        ),
    },
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TCL_MINISPLIT_ID): cv.use_id(TclMinisplit),
        **{
            cv.Optional(key): info["schema"].extend(cv.COMPONENT_SCHEMA)
            for key, info in SENSOR_TYPES.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TCL_MINISPLIT_ID])
    for key, info in SENSOR_TYPES.items():
        if conf := config.get(key):
            var = await sensor.new_sensor(
                conf, parent, getattr(TclSensorPurpose, info["purpose"])
            )
            await cg.register_component(var, conf)
