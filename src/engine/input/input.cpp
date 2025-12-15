#include "engine/input_defs.hpp"
#include "main_menu/menu.hpp"
#include "config.hpp"

#include <algorithm>
#include <cctype>
#include "mode_registry.hpp"
#include <engine/mode_registry.hpp>
#include <SDL_keyboard.h>
#include <engine/globals.hpp>


void process_inputs() {
    // Dispatch to current mode's input processor
    if (const ModeDesc* mode = find_mode(es->mode)) {
        if (mode->process_inputs_fn) {
            mode->process_inputs_fn();
        }
    }
}

// namespace {
// void ensure_default_input_profile() {
//     migrate_legacy_input_config();
//     ensure_input_profiles_dir();
//     auto profiles = list_input_profiles();
//     if (profiles.empty()) {
//         InputBindings defaults{};
//         save_input_profile(default_input_profile_name(), defaults, true);
//         profiles = list_input_profiles();
//     }
//     auto profile_exists = [&](const std::string& name) {
//         return std::any_of(profiles.begin(), profiles.end(),
//                            [&](const InputProfileInfo& info) { return info.name == name; });
//     };

//     std::string active_profile = load_active_input_profile_name();
//     if (active_profile.empty() || !profile_exists(active_profile))
//         active_profile = default_input_profile_name();

//     InputBindings loaded_binds{};
//     if (!load_input_profile(active_profile, &loaded_binds)) {
//         active_profile = default_input_profile_name();
//         load_input_profile(active_profile, &loaded_binds);
//     }

//     ss->input_binds = loaded_binds;
//     ss->menu.binds_current_preset = active_profile;
//     ss->menu.binds_snapshot = loaded_binds;
//     ss->menu.binds_dirty = false;
//     refresh_binds_profiles();
//     save_active_input_profile_name(active_profile);
// }
// } // namespace


// old shit
// void process_event(SDL_Event& ev) {
//     switch (ev.type) {
//         case SDL_QUIT:
//             ss->running = false;
//             break;
//         case SDL_KEYDOWN:
//             if (menu_is_text_input_active()) {
//                 SDL_Keycode key = ev.key.keysym.sym;
//                 SDL_Keymod mods = static_cast<SDL_Keymod>(ev.key.keysym.mod);
//                 bool shift = (mods & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
//                 bool caps = (mods & KMOD_CAPS) != 0;
//                 bool upper = shift ^ caps;
//                 if (key == SDLK_ESCAPE) {
//                     ss->menu.suppress_back_until_release = true;
//                     menu_text_input_cancel();
//                 } else if (key == SDLK_RETURN) {
//                     menu_text_input_submit();
//                 } else if (key == SDLK_BACKSPACE) {
//                     menu_text_input_backspace();
//                 } else {
//                     char c = 0;
//                     if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
//                         c = static_cast<char>(key);
//                         if (c >= 'a' && c <= 'z' && upper)
//                             c = static_cast<char>(std::toupper(c));
//                         else if (c >= 'A' && c <= 'Z' && !upper)
//                             c = static_cast<char>(std::tolower(c));
//                     } else if ((key >= '0' && key <= '9') || (key >= SDLK_KP_0 && key <= SDLK_KP_9)) {
//                         if (key >= '0' && key <= '9')
//                             c = static_cast<char>(key);
//                         else
//                             c = static_cast<char>('0' + (key - SDLK_KP_0));
//                     } else if (key == SDLK_MINUS) {
//                         c = shift ? '_' : '-';
//                     }
//                     if (c != 0)
//                         menu_text_input_append(c);
//                 }
//                 break;
//             }
//             // Binds capture: assign the pressed key to the selected action
//             if (ss->mode == modes::TITLE && ss->menu.capture_action_id >= 0) {
//                 SDL_Scancode sc = ev.key.keysym.scancode;
//                 if (sc == SDL_SCANCODE_ESCAPE) {
//                     ss->menu.capture_action_id = -1; // cancel
//                     ss->menu.suppress_back_until_release = true;
//                     break;
//                 }
//                 if (!ensure_active_binds_profile_writeable()) {
//                     ss->menu.capture_action_id = -1;
//                     break;
//                 }
//                 InputBindings& ib = ss->input_binds;
//                 BindAction a = static_cast<BindAction>(ss->menu.capture_action_id);
//                 bind_ref(ib, a) = sc;
//                 ss->menu.capture_action_id = -1;
//                 autosave_active_binds_profile();
//                 ss->menu.binds_dirty = !bindings_equal(ss->input_binds, ss->menu.binds_snapshot);
//             }
//             break;
//         case SDL_MOUSEMOTION:
//             // Track last-used device for menu focus policy
//             ss->menu.mouse_last_used = true;
//             ss->menu.last_input_source = 0;
//             break;
//         case SDL_MOUSEWHEEL:
//             ss->input_state.wheel_delta += static_cast<float>(ev.wheel.preciseY);
//             ss->menu.mouse_last_used = true;
//             ss->menu.last_input_source = 0;
//             break;
//         default:
//             break;
//     }
//     int mx = 0, my = 0;
//     Uint32 mbtn = SDL_GetMouseState(&mx, &my);
//     ss->mouse_inputs.left = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
//     ss->mouse_inputs.right = (mbtn & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
//     ss->mouse_inputs.pos = {mx, my};
//     if (ss->mouse_inputs.left || ss->mouse_inputs.right) {
//         ss->menu.mouse_last_used = true;
//     }
// }



// old shit
// void collect_menu_inputs() {
//     // Ensure a game controller is opened if present
//     static SDL_GameController* gpad = nullptr;
//     auto ensure_gamepad = [&]() {
//         if (gpad && SDL_GameControllerGetAttached(gpad)) return;
//         // Close stale
//         if (gpad) { SDL_GameControllerClose(gpad); gpad = nullptr; }
//         int nj = SDL_NumJoysticks();
//         for (int i = 0; i < nj; ++i) {
//             if (SDL_IsGameController(i)) {
//                 gpad = SDL_GameControllerOpen(i);
//                 if (gpad) break;
//             }
//         }
//     };
//     ensure_gamepad();

//     // Compose current button states from keyboard + controller
//     auto kb = [&](SDL_Scancode sc){ return is_down(sc); };
//     auto gp_btn = [&](SDL_GameControllerButton b){ return gpad && SDL_GameControllerGetButton(gpad, b) != 0; };
//     auto gp_axis = [&](SDL_GameControllerAxis a){ return gpad ? SDL_GameControllerGetAxis(gpad, a) : 0; };

//     const int DZ = 12000; // deadzone for axes
//     // Current states per source
//     bool kb_left = kb(SDL_SCANCODE_LEFT) || kb(SDL_SCANCODE_A);
//     bool kb_right = kb(SDL_SCANCODE_RIGHT) || kb(SDL_SCANCODE_D);
//     bool kb_up = kb(SDL_SCANCODE_UP) || kb(SDL_SCANCODE_W);
//     bool kb_down = kb(SDL_SCANCODE_DOWN) || kb(SDL_SCANCODE_S);
//     bool kb_confirm = kb(SDL_SCANCODE_RETURN) || kb(SDL_SCANCODE_SPACE);
//     bool kb_back = kb(SDL_SCANCODE_ESCAPE) || kb(SDL_SCANCODE_BACKSPACE);
//     bool kb_page_prev = kb(SDL_SCANCODE_Q);
//     bool kb_page_next = kb(SDL_SCANCODE_E);
//     bool kb_ctrl = kb(SDL_SCANCODE_LCTRL) || kb(SDL_SCANCODE_RCTRL);
//     bool kb_layout_toggle = kb(SDL_SCANCODE_L);
//     bool kb_nav_toggle = kb(SDL_SCANCODE_N);
//     bool kb_object_toggle = kb(SDL_SCANCODE_O);
//     bool kb_warp_toggle = kb(SDL_SCANCODE_Z);
//     bool kb_label_edit = kb(SDL_SCANCODE_F) || kb(SDL_SCANCODE_RETURN);
//     bool kb_help_toggle = kb(SDL_SCANCODE_H) || (kb_ctrl && kb(SDL_SCANCODE_H));
//     bool kb_snap_toggle = kb(SDL_SCANCODE_X);
//     bool kb_layout_reset = kb_ctrl && kb(SDL_SCANCODE_R);
//     bool kb_duplicate = kb(SDL_SCANCODE_D);
//     bool kb_delete_key = kb(SDL_SCANCODE_DELETE);
//     bool kb_editor_toggle = kb_ctrl && kb(SDL_SCANCODE_E);

//     bool gp_left = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTX) < -DZ);
//     bool gp_right = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTX) > DZ);
//     bool gp_up = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_UP) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTY) < -DZ);
//     bool gp_down = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTY) > DZ);
//     bool gp_confirm = gp_btn(SDL_CONTROLLER_BUTTON_A);
//     bool gp_back = gp_btn(SDL_CONTROLLER_BUTTON_B);
//     bool gp_page_prev = gp_btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || (gp_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > DZ);
//     bool gp_page_next = gp_btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || (gp_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > DZ);

//     // Build held (raw) states and edge events for confirm/back; directions used for repeat in menu logic
//     MenuInputs held{};
//     held.left = kb_left || gp_left;
//     held.right = kb_right || gp_right;
//     held.up = kb_up || gp_up;
//     held.down = kb_down || gp_down;
//     held.confirm = kb_confirm || gp_confirm;
//     held.back = kb_back || gp_back;
//     held.page_prev = kb_page_prev || gp_page_prev;
//     held.page_next = kb_page_next || gp_page_next;
//     held.layout_toggle = kb_layout_toggle;
//     held.nav_toggle = kb_nav_toggle;
//     held.object_toggle = kb_object_toggle;
//     held.warp_toggle = kb_warp_toggle;
//     held.label_edit = kb_label_edit;
//     held.help_toggle = kb_help_toggle;
//     held.snap_toggle = kb_snap_toggle;
//     held.layout_reset = kb_layout_reset;
//     held.duplicate_key = kb_duplicate;
//     held.delete_key = kb_delete_key;
//     held.editor_toggle = kb_editor_toggle;

//     if (menu_is_text_input_active()) {
//         held.left = held.right = held.up = held.down = false;
//         held.confirm = held.back = false;
//         held.page_prev = held.page_next = false;
//         held.layout_toggle = false;
//         held.nav_toggle = false;
//         held.object_toggle = false;
//         held.warp_toggle = false;
//         held.label_edit = false;
//         held.help_toggle = false;
//         held.snap_toggle = false;
//         held.layout_reset = false;
//         held.duplicate_key = false;
//         held.delete_key = false;
//         held.editor_toggle = false;
//     }
//     if (ss->menu.suppress_confirm_until_release) {
//         if (!held.confirm)
//             ss->menu.suppress_confirm_until_release = false;
//         else
//             held.confirm = false;
//     }
//     if (ss->menu.suppress_back_until_release) {
//         if (!held.back)
//             ss->menu.suppress_back_until_release = false;
//         else
//             held.back = false;
//     }

//     struct Prev {
//         bool confirm=false, back=false, pprev=false, pnext=false;
//         bool layout=false, nav=false, object=false, warp=false;
//         bool label=false, help=false, snap=false, layout_reset=false;
//         bool duplicate=false, del=false, editor=false;
//     };
//     static Prev prev{};
//     auto edge = [](bool now, bool prevv){ return now && !prevv; };

//     MenuInputs out{};
//     out.left = held.left;
//     out.right = held.right;
//     out.up = held.up;
//     out.down = held.down;
//     out.confirm = edge(held.confirm, prev.confirm);
//     out.back = edge(held.back, prev.back);
//     out.page_prev = edge(held.page_prev, prev.pprev);
//     out.page_next = edge(held.page_next, prev.pnext);
//     out.layout_toggle = edge(held.layout_toggle, prev.layout);
//     out.nav_toggle = edge(held.nav_toggle, prev.nav);
//     out.object_toggle = edge(held.object_toggle, prev.object);
//     out.warp_toggle = edge(held.warp_toggle, prev.warp);
//     out.label_edit = edge(held.label_edit, prev.label);
//     out.help_toggle = edge(held.help_toggle, prev.help);
//     out.snap_toggle = edge(held.snap_toggle, prev.snap);
//     out.layout_reset = edge(held.layout_reset, prev.layout_reset);
//     out.duplicate_key = edge(held.duplicate_key, prev.duplicate);
//     out.delete_key = edge(held.delete_key, prev.del);
//     out.editor_toggle = edge(held.editor_toggle, prev.editor);
//     prev = {held.confirm, held.back, held.page_prev, held.page_next,
//             held.layout_toggle, held.nav_toggle, held.object_toggle, held.warp_toggle,
//             held.label_edit, held.help_toggle, held.snap_toggle, held.layout_reset,
//             held.duplicate_key, held.delete_key, held.editor_toggle};

//     // If any controller/keyboard input occurred, mark last-used device and source
//     if (held.left || held.right || held.up || held.down || out.confirm || out.back || held.page_prev || held.page_next) {
//         ss->menu.mouse_last_used = false;
//         bool kb_any = kb_left || kb_right || kb_up || kb_down || kb_confirm || kb_back || kb_page_prev || kb_page_next;
//         ss->menu.last_input_source = kb_any ? 1 : 2;
//     }

//     ss->menu_inputs = out;
//     // Expose raw held states to menu for hold-repeat
//     ss->menu.hold_left = held.left;
//     ss->menu.hold_right = held.right;
//     ss->menu.hold_up = held.up;
//     ss->menu.hold_down = held.down;
// }
