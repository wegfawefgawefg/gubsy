#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include "globals.hpp"

static std::unordered_map<std::string, SDL_Scancode> make_scancode_map() {
    using S = SDL_Scancode;
    std::unordered_map<std::string, S> m;
    auto put = [&](const char* name, S sc) { m.emplace(name, sc); };
    // letters
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string k(1, c);
        put(k.c_str(), static_cast<S>(SDL_SCANCODE_A + (c - 'A')));
    }
    // digits top row
    put("0", SDL_SCANCODE_0);
    put("1", SDL_SCANCODE_1);
    put("2", SDL_SCANCODE_2);
    put("3", SDL_SCANCODE_3);
    put("4", SDL_SCANCODE_4);
    put("5", SDL_SCANCODE_5);
    put("6", SDL_SCANCODE_6);
    put("7", SDL_SCANCODE_7);
    put("8", SDL_SCANCODE_8);
    put("9", SDL_SCANCODE_9);
    // numpad
    put("KP_0", SDL_SCANCODE_KP_0);
    put("KP_1", SDL_SCANCODE_KP_1);
    put("KP_2", SDL_SCANCODE_KP_2);
    put("KP_3", SDL_SCANCODE_KP_3);
    put("KP_4", SDL_SCANCODE_KP_4);
    put("KP_5", SDL_SCANCODE_KP_5);
    put("KP_6", SDL_SCANCODE_KP_6);
    put("KP_7", SDL_SCANCODE_KP_7);
    put("KP_8", SDL_SCANCODE_KP_8);
    put("KP_9", SDL_SCANCODE_KP_9);
    // arrows
    put("LEFT", SDL_SCANCODE_LEFT);
    put("RIGHT", SDL_SCANCODE_RIGHT);
    put("UP", SDL_SCANCODE_UP);
    put("DOWN", SDL_SCANCODE_DOWN);
    // misc
    put("SPACE", SDL_SCANCODE_SPACE);
    put("RETURN", SDL_SCANCODE_RETURN);
    put("ESCAPE", SDL_SCANCODE_ESCAPE);
    put("BACKSPACE", SDL_SCANCODE_BACKSPACE);
    put(",", SDL_SCANCODE_COMMA);
    put(".", SDL_SCANCODE_PERIOD);
    put("-", SDL_SCANCODE_MINUS);
    put("=", SDL_SCANCODE_EQUALS);
    // modifiers
    put("LSHIFT", SDL_SCANCODE_LSHIFT);
    put("RSHIFT", SDL_SCANCODE_RSHIFT);
    put("LCTRL", SDL_SCANCODE_LCTRL);
    put("RCTRL", SDL_SCANCODE_RCTRL);
    put("LALT", SDL_SCANCODE_LALT);
    put("RALT", SDL_SCANCODE_RALT);
    return m;
}

static std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

bool load_input_bindings_from_ini(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return false;
    auto map = make_scancode_map();
    InputBindings b{};
    std::string line;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // uppercase val for map
        std::transform(val.begin(), val.end(), val.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        auto it = map.find(val);
        if (it == map.end())
            continue;
        SDL_Scancode sc = it->second;
        if (key == "left")
            b.left = sc;
        else if (key == "right")
            b.right = sc;
        else if (key == "up")
            b.up = sc;
        else if (key == "down")
            b.down = sc;
        else if (key == "use_left")
            b.use_left = sc;
        else if (key == "use_right")
            b.use_right = sc;
        else if (key == "use_up")
            b.use_up = sc;
        else if (key == "use_down")
            b.use_down = sc;
        else if (key == "use_center")
            b.use_center = sc;
        else if (key == "pick_up")
            b.pick_up = sc;
        else if (key == "drop")
            b.drop = sc;
        else if (key == "reload")
            b.reload = sc;
        else if (key == "dash")
            b.dash = sc;
    }
    ss->input_binds = b;
    return true;
}

static bool parse_float(const std::string& text, float& out) {
    try {
        size_t idx = 0;
        float v = std::stof(text, &idx);
        if (idx == 0)
            return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

static float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

bool load_audio_settings_from_ini(const std::string& path) {
    if (!ss)
        return false;
    std::ifstream f(path);
    if (!f.is_open())
        return false;
    bool master_set = false, music_set = false, sfx_set = false;
    std::string line;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        float parsed = 0.0f;
        if (!parse_float(val, parsed))
            continue;
        if (key == "master_volume") {
            ss->menu.vol_master = clamp01(parsed);
            master_set = true;
        } else if (key == "music_volume") {
            ss->menu.vol_music = clamp01(parsed);
            music_set = true;
        } else if (key == "sfx_volume") {
            ss->menu.vol_sfx = clamp01(parsed);
            sfx_set = true;
        }
    }
    return master_set || music_set || sfx_set;
}

static std::string scancode_to_token(SDL_Scancode sc) {
    using S = SDL_Scancode;
    // Letters
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        char c = char('A' + (int(sc) - int(SDL_SCANCODE_A)));
        return std::string(1, c);
    }
    // Digits
    if (sc >= SDL_SCANCODE_0 && sc <= SDL_SCANCODE_9) {
        char c = char('0' + (int(sc) - int(SDL_SCANCODE_0)));
        return std::string(1, c);
    }
    // Numpad
    if (sc >= SDL_SCANCODE_KP_0 && sc <= SDL_SCANCODE_KP_9) {
        char c = char('0' + (int(sc) - int(SDL_SCANCODE_KP_0)));
        return std::string("KP_") + c;
    }
    switch (sc) {
        case S::SDL_SCANCODE_LEFT: return "LEFT";
        case S::SDL_SCANCODE_RIGHT: return "RIGHT";
        case S::SDL_SCANCODE_UP: return "UP";
        case S::SDL_SCANCODE_DOWN: return "DOWN";
        case S::SDL_SCANCODE_SPACE: return "SPACE";
        case S::SDL_SCANCODE_RETURN: return "RETURN";
        case S::SDL_SCANCODE_ESCAPE: return "ESCAPE";
        case S::SDL_SCANCODE_BACKSPACE: return "BACKSPACE";
        case S::SDL_SCANCODE_COMMA: return ",";
        case S::SDL_SCANCODE_PERIOD: return ".";
        case S::SDL_SCANCODE_MINUS: return "-";
        case S::SDL_SCANCODE_EQUALS: return "=";
        case S::SDL_SCANCODE_LSHIFT: return "LSHIFT";
        case S::SDL_SCANCODE_RSHIFT: return "RSHIFT";
        case S::SDL_SCANCODE_LCTRL: return "LCTRL";
        case S::SDL_SCANCODE_RCTRL: return "RCTRL";
        case S::SDL_SCANCODE_LALT: return "LALT";
        case S::SDL_SCANCODE_RALT: return "RALT";
        default: break;
    }
    // Fallback to SDL name uppercased (may not round-trip via loader)
    std::string s = SDL_GetScancodeName(sc);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    if (s.empty()) s = "UNKNOWN";
    return s;
}

bool ensure_dir(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (dir.empty()) return false;
    if (fs::exists(dir, ec)) return true;
    return fs::create_directories(dir, ec);
}

bool save_input_bindings_to_ini(const std::string& path, const struct InputBindings& b) {
    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.has_parent_path()) {
        if (!ensure_dir(p.parent_path().string())) return false;
    }
    std::ofstream f(path);
    if (!f.is_open()) return false;
    auto W = [&](const char* key, SDL_Scancode sc){ f << key << "=" << scancode_to_token(sc) << "\n"; };
    W("left", b.left);
    W("right", b.right);
    W("up", b.up);
    W("down", b.down);
    W("use_left", b.use_left);
    W("use_right", b.use_right);
    W("use_up", b.use_up);
    W("use_down", b.use_down);
    W("use_center", b.use_center);
    W("pick_up", b.pick_up);
    W("drop", b.drop);
    W("reload", b.reload);
    W("dash", b.dash);
    return true;
}

bool save_audio_settings_to_ini(const std::string& path) {
    if (!ss)
        return false;
    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.has_parent_path()) {
        if (!ensure_dir(p.parent_path().string()))
            return false;
    }
    std::ofstream f(path);
    if (!f.is_open())
        return false;
    auto write_line = [&](const char* key, float v) {
        f << key << "=" << clamp01(v) << "\n";
    };
    write_line("master_volume", ss->menu.vol_master);
    write_line("music_volume", ss->menu.vol_music);
    write_line("sfx_volume", ss->menu.vol_sfx);
    return true;
}

std::vector<std::string> list_bind_presets(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        auto p = e.path();
        if (p.extension() == ".ini" || p.extension() == ".txt") {
            out.push_back(p.stem().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
