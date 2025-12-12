#pragma once

#include "engine/graphics.hpp"
#include "settings.hpp"

#include <SDL2/SDL.h>

struct KeyEdge {
    bool prev = false;
    bool toggle(bool now, bool& flag) {
        if (now && !prev) flag = !flag;
        prev = now;
        return flag;
    }
};

struct InputBindings {
    SDL_Scancode left = SDL_SCANCODE_A;
    SDL_Scancode right = SDL_SCANCODE_D;
    SDL_Scancode up = SDL_SCANCODE_W;
    SDL_Scancode down = SDL_SCANCODE_S;

    SDL_Scancode use_left = SDL_SCANCODE_LEFT;
    SDL_Scancode use_right = SDL_SCANCODE_RIGHT;
    SDL_Scancode use_up = SDL_SCANCODE_UP;
    SDL_Scancode use_down = SDL_SCANCODE_DOWN;
    SDL_Scancode use_center = SDL_SCANCODE_SPACE;

    SDL_Scancode pick_up = SDL_SCANCODE_F;
    SDL_Scancode drop = SDL_SCANCODE_Q;
    SDL_Scancode reload = SDL_SCANCODE_R;
    SDL_Scancode dash = SDL_SCANCODE_LSHIFT;
};

// Bindable actions used by the menu to map rows to fields in InputBindings
enum BindAction : int {
    BA_LEFT = 0,
    BA_RIGHT,
    BA_UP,
    BA_DOWN,
    BA_USE_LEFT,
    BA_USE_RIGHT,
    BA_USE_UP,
    BA_USE_DOWN,
    BA_USE_CENTER,
    BA_PICK_UP,
    BA_DROP,
    BA_RELOAD,
    BA_DASH,
    BA_COUNT
};

inline SDL_Scancode& bind_ref(InputBindings& b, BindAction a) {
    switch (a) {
        case BA_LEFT: return b.left;
        case BA_RIGHT: return b.right;
        case BA_UP: return b.up;
        case BA_DOWN: return b.down;
        case BA_USE_LEFT: return b.use_left;
        case BA_USE_RIGHT: return b.use_right;
        case BA_USE_UP: return b.use_up;
        case BA_USE_DOWN: return b.use_down;
        case BA_USE_CENTER: return b.use_center;
        case BA_PICK_UP: return b.pick_up;
        case BA_DROP: return b.drop;
        case BA_RELOAD: return b.reload;
        case BA_DASH: return b.dash;
        default: return b.left; // fallback, should not happen
    }
}

inline SDL_Scancode bind_get(const InputBindings& b, BindAction a) {
    return const_cast<SDL_Scancode&>(bind_ref(const_cast<InputBindings&>(b), a));
}

inline const char* bind_label(BindAction a) {
    switch (a) {
        case BA_LEFT: return "Move Left";
        case BA_RIGHT: return "Move Right";
        case BA_UP: return "Move Up";
        case BA_DOWN: return "Move Down";
        case BA_USE_LEFT: return "Use Left";
        case BA_USE_RIGHT: return "Use Right";
        case BA_USE_UP: return "Use Up";
        case BA_USE_DOWN: return "Use Down";
        case BA_USE_CENTER: return "Interact / Use";
        case BA_PICK_UP: return "Pick Up";
        case BA_DROP: return "Drop";
        case BA_RELOAD: return "Reload";
        case BA_DASH: return "Dash";
        default: return "";
    }
}

inline bool bindings_equal(const InputBindings& a, const InputBindings& b) {
    return a.left == b.left && a.right == b.right && a.up == b.up && a.down == b.down &&
        a.use_left == b.use_left && a.use_right == b.use_right && a.use_up == b.use_up && a.use_down == b.use_down &&
        a.use_center == b.use_center && a.pick_up == b.pick_up && a.drop == b.drop &&
        a.reload == b.reload && a.dash == b.dash;
}

struct InputState {
    float wheel_delta{0.0f};
};

void process_event(SDL_Event& ev);
void collect_inputs();
void process_inputs();

void process_inputs_title();
void process_inputs_playing();
