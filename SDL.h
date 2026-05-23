#pragma once

#include <cstddef>

#define SDL_ENABLE_OLD_NAMES 1
#include <SDL3/SDL.h>

#ifndef SDL_WINDOWPOS_CENTERED
#define SDL_WINDOWPOS_CENTERED 0
#endif

#ifndef SDL_WINDOW_FULLSCREEN_DESKTOP
#define SDL_WINDOW_FULLSCREEN_DESKTOP SDL_WINDOW_FULLSCREEN
#endif

#ifndef SDL_RENDERER_ACCELERATED
#define SDL_RENDERER_ACCELERATED 0
#endif

#ifdef __cplusplus
#ifdef SDL_IsGameController
#undef SDL_IsGameController
#endif
#ifdef SDL_GameControllerOpen
#undef SDL_GameControllerOpen
#endif

inline SDL_Window* SDL_CreateWindow(const char* title,
                                    int,
                                    int,
                                    int w,
                                    int h,
                                    Uint32 flags) {
    return SDL_CreateWindow(title, w, h, static_cast<SDL_WindowFlags>(flags));
}

inline SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int, Uint32) {
    return SDL_CreateRenderer(window, nullptr);
}

inline bool SDL_SetWindowFullscreen(SDL_Window* window, Uint32 flags) {
    return SDL_SetWindowFullscreen(window, flags != 0);
}

inline int SDL_setenv(const char* name, const char* value, int overwrite) {
    return SDL_setenv_unsafe(name, value, overwrite);
}

inline int SDL_NumJoysticks() {
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
    SDL_free(joysticks);
    return count;
}

inline SDL_JoystickID SDL_JoystickIDForIndex(int device_index) {
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
    SDL_JoystickID id = 0;
    if (joysticks && device_index >= 0 && device_index < count)
        id = joysticks[device_index];
    SDL_free(joysticks);
    return id;
}

inline bool SDL_IsGameController(int device_index) {
    SDL_JoystickID id = SDL_JoystickIDForIndex(device_index);
    return id != 0 && SDL_IsGamepad(id);
}

inline SDL_Gamepad* SDL_GameControllerOpen(int device_index) {
    SDL_JoystickID id = SDL_JoystickIDForIndex(device_index);
    return id != 0 ? SDL_OpenGamepad(id) : nullptr;
}

inline bool SDL_StartTextInput() {
    return SDL_StartTextInput(SDL_GetKeyboardFocus());
}

inline bool SDL_StopTextInput() {
    return SDL_StopTextInput(SDL_GetKeyboardFocus());
}

inline int SDL_QueryTexture(SDL_Texture* texture, Uint32*, int*, int* w, int* h) {
    float fw = 0.0f;
    float fh = 0.0f;
    if (!SDL_GetTextureSize(texture, &fw, &fh))
        return -1;
    if (w)
        *w = static_cast<int>(fw);
    if (h)
        *h = static_cast<int>(fh);
    return 0;
}

inline bool SDL_RenderTexture(SDL_Renderer* renderer,
                              SDL_Texture* texture,
                              const SDL_Rect* srcrect,
                              const SDL_Rect* dstrect) {
    SDL_FRect fsrc{};
    SDL_FRect fdst{};
    const SDL_FRect* fsrc_ptr = nullptr;
    const SDL_FRect* fdst_ptr = nullptr;
    if (srcrect) {
        fsrc = SDL_FRect{static_cast<float>(srcrect->x),
                         static_cast<float>(srcrect->y),
                         static_cast<float>(srcrect->w),
                         static_cast<float>(srcrect->h)};
        fsrc_ptr = &fsrc;
    }
    if (dstrect) {
        fdst = SDL_FRect{static_cast<float>(dstrect->x),
                         static_cast<float>(dstrect->y),
                         static_cast<float>(dstrect->w),
                         static_cast<float>(dstrect->h)};
        fdst_ptr = &fdst;
    }
    return SDL_RenderTexture(renderer, texture, fsrc_ptr, fdst_ptr);
}

inline bool SDL_RenderTexture(SDL_Renderer* renderer,
                              SDL_Texture* texture,
                              std::nullptr_t,
                              std::nullptr_t) {
    return SDL_RenderTexture(renderer,
                             texture,
                             static_cast<const SDL_FRect*>(nullptr),
                             static_cast<const SDL_FRect*>(nullptr));
}

inline bool SDL_RenderLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    return SDL_RenderLine(renderer,
                          static_cast<float>(x1),
                          static_cast<float>(y1),
                          static_cast<float>(x2),
                          static_cast<float>(y2));
}

inline bool SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    if (!rect)
        return SDL_RenderFillRect(renderer, static_cast<const SDL_FRect*>(nullptr));
    SDL_FRect frect{static_cast<float>(rect->x),
                    static_cast<float>(rect->y),
                    static_cast<float>(rect->w),
                    static_cast<float>(rect->h)};
    return SDL_RenderFillRect(renderer, &frect);
}

inline bool SDL_RenderRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    if (!rect)
        return SDL_RenderRect(renderer, static_cast<const SDL_FRect*>(nullptr));
    SDL_FRect frect{static_cast<float>(rect->x),
                    static_cast<float>(rect->y),
                    static_cast<float>(rect->w),
                    static_cast<float>(rect->h)};
    return SDL_RenderRect(renderer, &frect);
}
#endif
