api:alert("[bonus_pack] loaded")
register_item{
    id = "bonus_pad",
    label = "Bonus Pad",
    position = { x = -5, y = 2 },
    radius = 0.6,
    color = { r = 0.3, g = 0.8, b = 0.6 },
    on_use = function(info)
        api:alert("Bonus pad triggered")
        local pos = api:get_item_position("patch_pad")
        if pos then
            api:set_item_position("patch_pad", pos.x + 0.5, pos.y)
        end
    end
}
