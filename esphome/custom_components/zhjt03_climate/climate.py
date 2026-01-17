import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter, sensor
from esphome.const import CONF_ID, CONF_NAME

CONF_TRANSMITTER_ID = "transmitter_id"
CONF_AMBIENT_TEMPERATURE_SENSOR = "ambient_temperature_sensor"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_TEMPERATURE_STEP = "temperature_step"

CONF_DEFAULT_MODE = "default_mode"
CONF_DEFAULT_FAN_MODE = "default_fan_mode"
CONF_DEFAULT_TEMPERATURE = "default_temperature"
CONF_DEFAULT_SWING = "default_swing"

zhjt03_ns = cg.esphome_ns.namespace("zhjt03")
ZHJT03Climate = zhjt03_ns.class_("ZHJT03Climate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ZHJT03Climate),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
        cv.Required(CONF_AMBIENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),

        cv.Optional(CONF_MIN_TEMPERATURE, default=16): cv.int_range(min=16, max=31),
        cv.Optional(CONF_MAX_TEMPERATURE, default=31): cv.int_range(min=16, max=31),
        cv.Optional(CONF_TEMPERATURE_STEP, default=1): cv.int_range(min=1, max=5),

        cv.Optional(CONF_DEFAULT_MODE, default="COOL"): cv.one_of("OFF","AUTO","COOL","HEAT","DRY","FAN", upper=True),
        cv.Optional(CONF_DEFAULT_FAN_MODE, default="AUTO"): cv.one_of("AUTO","LOW","MEDIUM","HIGH", upper=True),
        cv.Optional(CONF_DEFAULT_TEMPERATURE, default=24): cv.int_range(min=16, max=31),
        cv.Optional(CONF_DEFAULT_SWING, default="OFF"): cv.one_of("OFF","ON", upper=True),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    tx = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    amb = await cg.get_variable(config[CONF_AMBIENT_TEMPERATURE_SENSOR])

    cg.add(var.set_transmitter(tx))
    cg.add(var.set_ambient_sensor(amb))

    cg.add(var.set_limits(config[CONF_MIN_TEMPERATURE], config[CONF_MAX_TEMPERATURE], config[CONF_TEMPERATURE_STEP]))
    cg.add(var.set_defaults(
        config[CONF_DEFAULT_MODE],
        config[CONF_DEFAULT_FAN_MODE],
        config[CONF_DEFAULT_TEMPERATURE],
        config[CONF_DEFAULT_SWING],
    ))
