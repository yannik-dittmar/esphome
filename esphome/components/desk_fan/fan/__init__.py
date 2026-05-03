import esphome.codegen as cg
from esphome.components import fan, light, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT, CONF_OUTPUT_ID

from .. import desk_fan_ns

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["remote_base", "light"]

DeskFan = desk_fan_ns.class_(
    "DeskFan",
    cg.Component,
    fan.Fan,
    remote_base.RemoteTransmittable,
    remote_base.RemoteReceiverListener,
)

DeskFanLight = desk_fan_ns.class_("DeskFanLight", light.LightOutput)

CONFIG_SCHEMA = (
    fan.fan_schema(DeskFan)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(DeskFan),
            cv.Optional(remote_base.CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
            cv.Optional(CONF_LIGHT): light.light_schema(
                DeskFanLight,
                light.LightType.BINARY,
                default_restore_mode="ALWAYS_ON",
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await remote_base.register_transmittable(var, config)
    if remote_base.CONF_RECEIVER_ID in config:
        await remote_base.register_listener(var, config)

    if CONF_LIGHT in config:
        light_config = config[CONF_LIGHT]
        light_var = await light.new_light(light_config)
        cg.add(light_var.set_fan(var))
        cg.add(var.set_light_output(light_var))
