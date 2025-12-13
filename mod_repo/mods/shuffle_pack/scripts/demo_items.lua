register_item{
    id = "shuffle:teleport_shuffler",
    label = "Shuffle Pads",
    sprite = "base:boots",
    position = { x = 2.0, y = -2.5 },
    radius = 0.7,
    color = { r = 0.55, g = 0.4, b = 0.95 },
    on_use = function(info)
        local pad_pos = api:get_item_position("base:teleport_pad")
        if not pad_pos then
            api:alert("Teleport Pad missing")
            api:play_sound("base:ui_cant")
            return
        end
        local dx = math.random(-2, 2)
        local dy = math.random(-2, 2)
        pad_pos.x = pad_pos.x + dx * 0.5
        pad_pos.y = pad_pos.y + dy * 0.5
        api:set_item_position("base:teleport_pad", pad_pos.x, pad_pos.y)
        api:alert(string.format("Moved Teleport Pad to (%.1f, %.1f)", pad_pos.x, pad_pos.y))
        api:play_sound("base:ui_confirm")
    end
}

register_item{
    id = "shuffle:swap_spots",
    label = "Swap Spots",
    sprite = "base:medkit",
    position = { x = 4.0, y = 2.0 },
    radius = 0.6,
    color = { r = 0.3, g = 0.6, b = 0.95 },
    on_use = function(info)
        local pad_pos = api:get_item_position("base:teleport_pad")
        if not pad_pos then
            api:alert("Teleport Pad missing")
            api:play_sound("base:ui_cant")
            return
        end
        local target_x = pad_pos.x
        local target_y = pad_pos.y
        api:set_item_position("base:teleport_pad", info.x, info.y)
        api:set_player_position(target_x, target_y)
        api:alert("Swapped positions with the Teleport Pad!")
        api:play_sound("base:super_confirm")
    end
}
