api:alert("[patch_core] loaded")
register_item{
    id = "patch_pad",
    label = "Patch Pad",
    position = { x = -3, y = -1 },
    radius = 0.5,
    color = { r = 0.8, g = 0.4, b = 0.5 },
    on_use = function(info)
        api:alert("Patch Pad activated!")
        api:set_bonk_position(0, 0)
    end
}
