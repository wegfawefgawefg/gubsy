math.randomseed(os.time())

local bonk_enabled = true

register_item{
    id = "base:teleport_pad",
    label = "Teleport Pad",
    sprite = "base:medkit",
    position = { x = 3.0, y = 0.5 },
    radius = 0.7,
    color = { r = 0.25, g = 0.65, b = 1.0 },
    on_use = function(info)
        api:alert("Teleporting via " .. info.label)
        api:set_player_position(0.0, 0.0)
        api:play_sound("base:super_confirm")
    end
}

register_item{
    id = "base:bonk_toggle",
    label = "Toggle Bonk",
    sprite = "base:bandage",
    position = { x = -3.5, y = -1.0 },
    radius = 0.7,
    color = { r = 0.95, g = 0.75, b = 0.25 },
    on_use = function(info)
        bonk_enabled = not bonk_enabled
        api:set_bonk_enabled(bonk_enabled)
        if bonk_enabled then
            api:alert("Bonk square enabled")
        else
            api:alert("Bonk square hidden")
        end
        api:play_sound("base:ui_confirm")
    end
}

register_item{
    id = "base:bonk_move",
    label = "Move Bonk",
    sprite = "base:conductor_hat",
    position = { x = -1.5, y = 2.5 },
    radius = 0.7,
    color = { r = 0.9, g = 0.4, b = 0.3 },
    on_use = function(info)
        local x = math.random(-4, 4)
        local y = math.random(-3, 3)
        api:set_bonk_position(x, y)
        api:alert(string.format("Bonk moved to (%d, %d)", x, y))
        api:play_sound("base:ui_super_confirm")
    end
}

register_item{
    id = "base:item_shuffle",
    label = "Shuffle Pads",
    sprite = "base:boots",
    position = { x = 2.0, y = -2.5 },
    radius = 0.7,
    color = { r = 0.55, g = 0.4, b = 0.95 },
    on_use = function(info)
        local pad_pos = api:get_item_position("base:teleport_pad")
        if pad_pos then
            local dx = math.random(-2, 2)
            local dy = math.random(-2, 2)
            pad_pos.x = pad_pos.x + dx * 0.5
            pad_pos.y = pad_pos.y + dy * 0.5
            api:set_item_position("base:teleport_pad", pad_pos.x, pad_pos.y)
            api:alert(string.format("Moved Teleport Pad to (%.1f, %.1f)", pad_pos.x, pad_pos.y))
            api:play_sound("base:ui_confirm")
        else
            api:alert("Teleport Pad not found")
        end
    end
}
