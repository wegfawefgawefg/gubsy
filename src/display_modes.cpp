#include "src/display_modes.hpp"

#include "src/engine_state.hpp"
#include "src/graphics.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>

namespace {

std::string fullscreen_mode_value(const SDL_DisplayMode& mode) {
    int numerator = mode.refresh_rate_numerator;
    int denominator = mode.refresh_rate_denominator;
    if (numerator <= 0 || denominator <= 0) {
        numerator = static_cast<int>(std::lround(mode.refresh_rate * 1000.0f));
        denominator = 1000;
    }
    if (numerator <= 0 || denominator <= 0) {
        numerator = 0;
        denominator = 1;
    }
    return std::to_string(mode.w) + "x" + std::to_string(mode.h) + "@" + std::to_string(numerator) +
           "/" + std::to_string(denominator);
}

std::string fullscreen_mode_label(const SDL_DisplayMode& mode) {
    float refresh = mode.refresh_rate;
    if (refresh <= 0.0f && mode.refresh_rate_numerator > 0 && mode.refresh_rate_denominator > 0) {
        refresh = static_cast<float>(mode.refresh_rate_numerator) /
                  static_cast<float>(mode.refresh_rate_denominator);
    }

    char buffer[80]{};
    if (refresh > 0.0f) {
        float rounded = std::round(refresh);
        if (std::fabs(refresh - rounded) < 0.01f) {
            std::snprintf(buffer, sizeof(buffer), "%dx%d @ %.0f Hz", mode.w, mode.h,
                          static_cast<double>(rounded));
        } else {
            std::snprintf(buffer, sizeof(buffer), "%dx%d @ %.2f Hz", mode.w, mode.h,
                          static_cast<double>(refresh));
        }
    } else {
        std::snprintf(buffer, sizeof(buffer), "%dx%d", mode.w, mode.h);
    }
    return buffer;
}

bool parse_fullscreen_mode_value(const std::string& value, int& width, int& height, int& numerator,
                                 int& denominator) {
    std::size_t x_pos = value.find('x');
    std::size_t at_pos = value.find('@', x_pos == std::string::npos ? 0 : x_pos + 1);
    std::size_t slash_pos = value.find('/', at_pos == std::string::npos ? 0 : at_pos + 1);
    if (x_pos == std::string::npos || at_pos == std::string::npos ||
        slash_pos == std::string::npos) {
        return false;
    }
    width = std::atoi(value.substr(0, x_pos).c_str());
    height = std::atoi(value.substr(x_pos + 1, at_pos - x_pos - 1).c_str());
    numerator = std::atoi(value.substr(at_pos + 1, slash_pos - at_pos - 1).c_str());
    denominator = std::atoi(value.substr(slash_pos + 1).c_str());
    return width > 0 && height > 0 && numerator >= 0 && denominator > 0;
}

bool display_mode_matches_value(const SDL_DisplayMode& mode, int width, int height, int numerator,
                                int denominator) {
    if (mode.w != width || mode.h != height)
        return false;
    int mode_numerator = mode.refresh_rate_numerator;
    int mode_denominator = mode.refresh_rate_denominator;
    if (mode_numerator <= 0 || mode_denominator <= 0) {
        mode_numerator = static_cast<int>(std::lround(mode.refresh_rate * 1000.0f));
        mode_denominator = 1000;
    }
    if (mode_numerator <= 0 || mode_denominator <= 0) {
        mode_numerator = 0;
        mode_denominator = 1;
    }
    return mode_numerator == numerator && mode_denominator == denominator;
}

SDL_DisplayID display_for_window(SDL_Window* window) {
    SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : 0;
    if (display == 0)
        display = SDL_GetPrimaryDisplay();
    return display;
}

} // namespace

std::vector<SettingOption> fullscreen_display_mode_options(EngineState& engine) {
    std::string desktop_label = desktop_display_mode_label(engine);
    if (desktop_label.empty())
        desktop_label = "Desktop Default";
    else
        desktop_label = "Desktop Default (" + desktop_label + ")";
    std::vector<SettingOption> options{SettingOption{"desktop", desktop_label}};
    SDL_Window* window = current_graphics(engine) ? current_graphics(engine)->window : nullptr;
    if (!window || !SDL_WasInit(SDL_INIT_VIDEO))
        return options;

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display_for_window(window), &count);
    if (!modes)
        return options;
    if (count <= 0) {
        SDL_free(modes);
        return options;
    }

    std::unordered_set<std::string> seen;
    seen.insert("desktop");
    for (int i = 0; i < count; ++i) {
        if (!modes[i] || modes[i]->w <= 0 || modes[i]->h <= 0)
            continue;
        std::string value = fullscreen_mode_value(*modes[i]);
        if (!seen.insert(value).second)
            continue;
        options.push_back(SettingOption{value, fullscreen_mode_label(*modes[i])});
    }
    SDL_free(modes);
    return options;
}

std::string configured_fullscreen_display_mode(const EngineState& engine) {
    auto it = engine.top_level_game_settings.settings.find("gubsy.video.fullscreen_display_mode");
    if (it == engine.top_level_game_settings.settings.end())
        return "desktop";
    if (const std::string* sv = std::get_if<std::string>(&it->second))
        return sv->empty() ? std::string("desktop") : *sv;
    return "desktop";
}

std::string desktop_display_mode_label(EngineState& engine) {
    SDL_Window* window = current_graphics(engine) ? current_graphics(engine)->window : nullptr;
    if (!window || !SDL_WasInit(SDL_INIT_VIDEO))
        return {};

    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(display_for_window(window));
    return mode ? fullscreen_mode_label(*mode) : std::string{};
}

bool apply_fullscreen_display_mode(SDL_Window* window, const std::string& value,
                                   std::string& applied_value) {
    if (!window)
        return false;
    if (value.empty() || value == "desktop") {
        if (!SDL_SetWindowFullscreenMode(window, nullptr)) {
            std::fprintf(stderr, "Failed to set desktop fullscreen mode: %s\n", SDL_GetError());
            return false;
        }
        applied_value = "desktop";
        return true;
    }

    int width = 0;
    int height = 0;
    int numerator = 0;
    int denominator = 1;
    if (!parse_fullscreen_mode_value(value, width, height, numerator, denominator)) {
        std::fprintf(stderr, "Invalid fullscreen display mode '%s'; using desktop default.\n",
                     value.c_str());
        return apply_fullscreen_display_mode(window, "desktop", applied_value);
    }

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display_for_window(window), &count);
    bool ok = false;
    if (modes && count > 0) {
        for (int i = 0; i < count; ++i) {
            if (!modes[i])
                continue;
            if (!display_mode_matches_value(*modes[i], width, height, numerator, denominator))
                continue;
            ok = SDL_SetWindowFullscreenMode(window, modes[i]);
            break;
        }
    }
    SDL_free(modes);

    if (!ok) {
        std::fprintf(stderr,
                     "Fullscreen display mode '%s' is unavailable; using desktop default.\n",
                     value.c_str());
        return apply_fullscreen_display_mode(window, "desktop", applied_value);
    }

    applied_value = value;
    return true;
}
