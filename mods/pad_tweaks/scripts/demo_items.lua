-- Poke existing defs from the base mod.
patch_item{
    id = "base:teleport_pad",
    label = "Crystal Pad",
    color = { r = 0.4, g = 0.8, b = 1.0 },
    sprite = "base:keycard",
    radius = 0.8
}

patch_item{
    id = "base:bonk_toggle",
    position = { x = -4.2, y = -2.0 },
    color = { r = 1.0, g = 0.55, b = 0.2 }
}

register_item{
    id = "pad_tweaks:restore_defaults",
    label = "Reset Pads",
    sprite = "base:bandage",
    position = { x = 0.0, y = -3.5 },
    radius = 0.65,
    color = { r = 0.6, g = 0.7, b = 1.0 },
    on_use = function(info)
        patch_item{
            id = "base:teleport_pad",
            label = "Teleport Pad",
            color = { r = 0.25, g = 0.65, b = 1.0 },
            sprite = "base:medkit",
            radius = 0.7
        }
        patch_item{
            id = "base:bonk_toggle",
            position = { x = -3.5, y = -1.0 },
            color = { r = 0.95, g = 0.75, b = 0.25 }
        }
        api:alert("Base pads restored")
        api:play_sound("base:ui_confirm")
    end
}
