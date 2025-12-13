api.alert("[super_suite] loaded")
local bonk_on = true
register_item{
    id = "super_button",
    label = "Super Button",
    position = { x = -6, y = -2 },
    radius = 0.6,
    color = { r = 0.9, g = 0.7, b = 0.2 },
    on_use = function(info)
        bonk_on = not bonk_on
        api.set_bonk_enabled(bonk_on)
        api.alert("Super Suite toggled bonk to " .. (bonk_on and "ON" or "OFF"))
    end
}
