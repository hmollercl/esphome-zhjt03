import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter, sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["climate"]

zhjt03_ns = cg.esphome_ns.namespace("zhjt03")
ZHJT03Climate = zhjt03_ns.class_("ZHJT03Climate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ZHJT03Climate),
        cv.Optional("sensor"): cv.use_id(sensor.Sensor),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    if "sensor" in config:
        sens = await cg.get_variable(config["sensor"])
        cg.add(var.set_temperature_sensor(sens))
