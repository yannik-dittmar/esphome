import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv

from .. import desk_fan_ns

DeskFan = desk_fan_ns.class_("DeskFan", cg.Component)

CONFIG_SCHEMA = (
    fan.fan_schema(DeskFan)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(DeskFan),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

cg.add_library("crankyoldgit/IRremoteESP8266", "2.8.6")


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
