//interface is probably something like
// and that first checks the players input profile, then goes through each source they have to see if any source satisfies the button press
// same for axis values
// was_pressed(player_index, game_button)
// was_released(player_index, game_button)
// is_down(player_index, game_button)
// get_axis(player_index, game_axis) -> float
// get_axis_delta(player_index, game_axis) -> float
// likely mouse specific ones
// get_mouse_pos(player_index) -> vec2
// get_mouse_delta(player_index) -> vec2
// get_mouse_pos_window(player_index) -> vec2 // normalized 0-1 in window
// get_mouse_pos_screen(player_index) -> vec2 // normalized 0-1 in screen
// get_mouse_wheel_delta(player_index) -> int


// managing profiles, saves, and binds profiles, and settings profiles
// load all profiles
// save one profile
// load all binds profiles
// save one binds profile
// load all settings profiles
// save one settings profile

// or are settings the same as profiles?

// binds are easily managed by gubsy, int -> button or axis (recommend to the dev to make an enum of bindable actions for easy access)
// settings are per game, like, int -> value [float, int, bool, string] (recommend to the dev to make an enum of settings keys for easy access)

// saves might be too complex to be handled by gubsy, unless its just a blob of data per profile that gubsy saves and loads for the game to interpret..