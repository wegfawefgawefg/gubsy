#include "globals.hpp"
#include "types.hpp"
#include "engine/input_defs.hpp"
#include "engine/input.hpp"
#include "main_menu/menu.hpp"
#include "config.hpp"

#include <algorithm>
#include <cctype>

// Forward declaration used within this translation unit
void collect_menu_inputs();

static bool is_down(SDL_Scancode sc) {
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    return ks[sc] != 0;
}


void process_event(SDL_Event& ev) {
    switch (ev.type) {
        case SDL_QUIT:
            ss->running = false;
            break;
        case SDL_KEYDOWN:
            if (menu_is_text_input_active()) {
                SDL_Keycode key = ev.key.keysym.sym;
                SDL_Keymod mods = static_cast<SDL_Keymod>(ev.key.keysym.mod);
                bool shift = (mods & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
                bool caps = (mods & KMOD_CAPS) != 0;
                bool upper = shift ^ caps;
                if (key == SDLK_ESCAPE) {
                    ss->menu.suppress_back_until_release = true;
                    menu_text_input_cancel();
                } else if (key == SDLK_RETURN) {
                    menu_text_input_submit();
                } else if (key == SDLK_BACKSPACE) {
                    menu_text_input_backspace();
                } else {
                    char c = 0;
                    if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
                        c = static_cast<char>(key);
                        if (c >= 'a' && c <= 'z' && upper)
                            c = static_cast<char>(std::toupper(c));
                        else if (c >= 'A' && c <= 'Z' && !upper)
                            c = static_cast<char>(std::tolower(c));
                    } else if ((key >= '0' && key <= '9') || (key >= SDLK_KP_0 && key <= SDLK_KP_9)) {
                        if (key >= '0' && key <= '9')
                            c = static_cast<char>(key);
                        else
                            c = static_cast<char>('0' + (key - SDLK_KP_0));
                    } else if (key == SDLK_MINUS) {
                        c = shift ? '_' : '-';
                    }
                    if (c != 0)
                        menu_text_input_append(c);
                }
                break;
            }
            // Binds capture: assign the pressed key to the selected action
            if (ss->mode == modes::TITLE && ss->menu.capture_action_id >= 0) {
                SDL_Scancode sc = ev.key.keysym.scancode;
                if (sc == SDL_SCANCODE_ESCAPE) {
                    ss->menu.capture_action_id = -1; // cancel
                    ss->menu.suppress_back_until_release = true;
                    break;
                }
                if (!ensure_active_binds_profile_writeable()) {
                    ss->menu.capture_action_id = -1;
                    break;
                }
                InputBindings& ib = ss->input_binds;
                BindAction a = static_cast<BindAction>(ss->menu.capture_action_id);
                bind_ref(ib, a) = sc;
                ss->menu.capture_action_id = -1;
                autosave_active_binds_profile();
                ss->menu.binds_dirty = !bindings_equal(ss->input_binds, ss->menu.binds_snapshot);
            }
            break;
        case SDL_MOUSEMOTION:
            // Track last-used device for menu focus policy
            ss->menu.mouse_last_used = true;
            ss->menu.last_input_source = 0;
            break;
        case SDL_MOUSEWHEEL:
            ss->input_state.wheel_delta += static_cast<float>(ev.wheel.preciseY);
            ss->menu.mouse_last_used = true;
            ss->menu.last_input_source = 0;
            break;
        default:
            break;
    }
    int mx = 0, my = 0;
    Uint32 mbtn = SDL_GetMouseState(&mx, &my);
    ss->mouse_inputs.left = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    ss->mouse_inputs.right = (mbtn & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    ss->mouse_inputs.pos = {mx, my};
    if (ss->mouse_inputs.left || ss->mouse_inputs.right) {
        ss->menu.mouse_last_used = true;
    }
}

void collect_inputs() {
    // Transfer wheel delta to one-frame mouse scroll and reset the accumulator
    ss->mouse_inputs.scroll = ss->input_state.wheel_delta;
    ss->input_state.wheel_delta = 0.0f;
    collect_menu_inputs();
}

void collect_menu_inputs() {
    // Ensure a game controller is opened if present
    static SDL_GameController* gpad = nullptr;
    auto ensure_gamepad = [&]() {
        if (gpad && SDL_GameControllerGetAttached(gpad)) return;
        // Close stale
        if (gpad) { SDL_GameControllerClose(gpad); gpad = nullptr; }
        int nj = SDL_NumJoysticks();
        for (int i = 0; i < nj; ++i) {
            if (SDL_IsGameController(i)) {
                gpad = SDL_GameControllerOpen(i);
                if (gpad) break;
            }
        }
    };
    ensure_gamepad();

    // Compose current button states from keyboard + controller
    auto kb = [&](SDL_Scancode sc){ return is_down(sc); };
    auto gp_btn = [&](SDL_GameControllerButton b){ return gpad && SDL_GameControllerGetButton(gpad, b) != 0; };
    auto gp_axis = [&](SDL_GameControllerAxis a){ return gpad ? SDL_GameControllerGetAxis(gpad, a) : 0; };

    const int DZ = 12000; // deadzone for axes
    // Current states per source
    bool kb_left = kb(SDL_SCANCODE_LEFT) || kb(SDL_SCANCODE_A);
    bool kb_right = kb(SDL_SCANCODE_RIGHT) || kb(SDL_SCANCODE_D);
    bool kb_up = kb(SDL_SCANCODE_UP) || kb(SDL_SCANCODE_W);
    bool kb_down = kb(SDL_SCANCODE_DOWN) || kb(SDL_SCANCODE_S);
    bool kb_confirm = kb(SDL_SCANCODE_RETURN) || kb(SDL_SCANCODE_SPACE);
    bool kb_back = kb(SDL_SCANCODE_ESCAPE) || kb(SDL_SCANCODE_BACKSPACE);
    bool kb_page_prev = kb(SDL_SCANCODE_Q);
    bool kb_page_next = kb(SDL_SCANCODE_E);

    bool gp_left = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTX) < -DZ);
    bool gp_right = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTX) > DZ);
    bool gp_up = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_UP) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTY) < -DZ);
    bool gp_down = gp_btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || (gp_axis(SDL_CONTROLLER_AXIS_LEFTY) > DZ);
    bool gp_confirm = gp_btn(SDL_CONTROLLER_BUTTON_A);
    bool gp_back = gp_btn(SDL_CONTROLLER_BUTTON_B);
    bool gp_page_prev = gp_btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || (gp_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > DZ);
    bool gp_page_next = gp_btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || (gp_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > DZ);

    // Build held (raw) states and edge events for confirm/back; directions used for repeat in menu logic
    MenuInputs held{};
    held.left = kb_left || gp_left;
    held.right = kb_right || gp_right;
    held.up = kb_up || gp_up;
    held.down = kb_down || gp_down;
    held.confirm = kb_confirm || gp_confirm;
    held.back = kb_back || gp_back;
    held.page_prev = kb_page_prev || gp_page_prev;
    held.page_next = kb_page_next || gp_page_next;

    if (menu_is_text_input_active()) {
        held.left = held.right = held.up = held.down = false;
        held.confirm = held.back = false;
        held.page_prev = held.page_next = false;
    }
    if (ss->menu.suppress_confirm_until_release) {
        if (!held.confirm)
            ss->menu.suppress_confirm_until_release = false;
        else
            held.confirm = false;
    }
    if (ss->menu.suppress_back_until_release) {
        if (!held.back)
            ss->menu.suppress_back_until_release = false;
        else
            held.back = false;
    }

    struct Prev { bool confirm=false, back=false, pprev=false, pnext=false; };
    static Prev prev{};
    auto edge = [](bool now, bool prevv){ return now && !prevv; };

    MenuInputs out{};
    out.left = held.left;
    out.right = held.right;
    out.up = held.up;
    out.down = held.down;
    out.confirm = edge(held.confirm, prev.confirm);
    out.back = edge(held.back, prev.back);
    out.page_prev = edge(held.page_prev, prev.pprev);
    out.page_next = edge(held.page_next, prev.pnext);
    prev = {held.confirm, held.back, held.page_prev, held.page_next};

    // If any controller/keyboard input occurred, mark last-used device and source
    if (held.left || held.right || held.up || held.down || out.confirm || out.back || held.page_prev || held.page_next) {
        ss->menu.mouse_last_used = false;
        bool kb_any = kb_left || kb_right || kb_up || kb_down || kb_confirm || kb_back || kb_page_prev || kb_page_next;
        ss->menu.last_input_source = kb_any ? 1 : 2;
    }

    ss->menu_inputs = out;
    // Expose raw held states to menu for hold-repeat
    ss->menu.hold_left = held.left;
    ss->menu.hold_right = held.right;
    ss->menu.hold_up = held.up;
    ss->menu.hold_down = held.down;
}

void process_inputs() {
    if (is_down(SDL_SCANCODE_ESCAPE) && ss->mode != modes::TITLE)
        ss->running = false;

    if (ss->mode == modes::TITLE) {
        process_inputs_title();
    } else if (ss->mode == modes::PLAYING) {
        process_inputs_playing();
    }
}



void process_inputs_title() {
    // Handle all title/settings menu input reactions here (per-frame),
    // not in fixed-step, to avoid dropping edge-triggered inputs.
    int w = 0, h = 0;
    if (gg && gg->renderer) {
        SDL_GetRendererOutputSize(gg->renderer, &w, &h);
    }
    step_menu_logic(w, h);
}

void process_inputs_playing() {
    PlayingInputs pi{};
    const InputBindings& b = ss->input_binds;
    pi.left = is_down(b.left);
    pi.right = is_down(b.right);
    pi.up = is_down(b.up);
    pi.down = is_down(b.down);
    pi.inventory_prev = is_down(SDL_SCANCODE_COMMA);
    pi.inventory_next = is_down(SDL_SCANCODE_PERIOD);
    pi.mouse_pos = glm::vec2(ss->mouse_inputs.pos);
    pi.mouse_down[0] = ss->mouse_inputs.left;
    pi.mouse_down[1] = ss->mouse_inputs.right;
    pi.use_left = is_down(b.use_left);
    pi.use_right = is_down(b.use_right);
    pi.use_up = is_down(b.use_up);
    pi.use_down = is_down(b.use_down);
    pi.use_center = is_down(b.use_center) || is_down(SDL_SCANCODE_SPACE);
    // Manual pickup key
    pi.pick_up = is_down(b.pick_up);
    pi.drop = is_down(b.drop);
    pi.reload = is_down(b.reload);
    pi.dash = is_down(b.dash);
    // number row keys 1..0
    pi.num_row_1 = is_down(SDL_SCANCODE_1);
    pi.num_row_2 = is_down(SDL_SCANCODE_2);
    pi.num_row_3 = is_down(SDL_SCANCODE_3);
    pi.num_row_4 = is_down(SDL_SCANCODE_4);
    pi.num_row_5 = is_down(SDL_SCANCODE_5);
    pi.num_row_6 = is_down(SDL_SCANCODE_6);
    pi.num_row_7 = is_down(SDL_SCANCODE_7);
    pi.num_row_8 = is_down(SDL_SCANCODE_8);
    pi.num_row_9 = is_down(SDL_SCANCODE_9);
    pi.num_row_0 = is_down(SDL_SCANCODE_0);

    // Zoom with mouse wheel (use per-frame scroll delta)
    if (ss->mouse_inputs.scroll != 0.0f) {
        const float ZOOM_INCREMENT = 0.25f;
        const float MIN_ZOOM = 0.5f;
        const float MAX_ZOOM = 32.0f;
        float dir = (ss->mouse_inputs.scroll > 0.0f) ? 1.0f : -1.0f;
        if (gg) {
            gg->play_cam.zoom =
                std::clamp(gg->play_cam.zoom + dir * ZOOM_INCREMENT, MIN_ZOOM, MAX_ZOOM);
        }
    }

    static KeyEdge c;
    c.toggle(is_down(SDL_SCANCODE_C), ss->show_character_panel);

    static KeyEdge v;
    v.toggle(is_down(SDL_SCANCODE_V), ss->show_gun_panel);

    ss->playing_input_debounce_timers.step();
    ss->playing_inputs = ss->playing_input_debounce_timers.debounce(pi);
}
