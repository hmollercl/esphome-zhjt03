import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter, sensor
from esphome.const import CONF_ID

CONF_TRANSMITTER_ID = "transmitter_id"
CONF_SENSOR = "sensor"

zhjt03_ns = cg.esphome_ns.namespace("zhjt03")
ZHJT03Climate = zhjt03_ns.class_("ZHJT03Climate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.climate_schema(ZHJT03Climate).extend(
    {
        cv.GenerateID(): cv.declare_id(ZHJT03Climate),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    tx = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(tx))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_temperature_sensor(sens))
