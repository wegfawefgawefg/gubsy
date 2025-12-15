#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include "globals.hpp"
#include "engine/input.hpp"

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

static bool parse_input_bindings_stream(std::istream& f, InputBindings& out_bindings) {
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
    out_bindings = b;
    return true;
}

bool load_input_bindings_from_ini(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return false;
    InputBindings tmp{};
    if (!parse_input_bindings_stream(f, tmp))
        return false;
    ss->input_binds = tmp;
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

std::string input_profiles_dir() {
    return "config/input_profiles";
}

bool ensure_input_profiles_dir() {
    return ensure_dir(input_profiles_dir());
}

std::string sanitize_profile_name(const std::string& raw) {
    std::string name;
    name.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            name.push_back(c);
        }
    }
    if (name.empty()) name = "Preset";
    if (name.size() > 24) name.resize(24);
    return name;
}

std::string make_unique_profile_name(const std::string& base) {
    auto profiles = list_input_profiles();
    auto exists = [&](const std::string& n) {
        return std::any_of(profiles.begin(), profiles.end(), [&](const InputProfileInfo& info){ return info.name == n; });
    };
    std::string sanitized = sanitize_profile_name(base);
    if (sanitized.empty())
        sanitized = "Preset";
    if (!exists(sanitized))
        return sanitized;
    int suffix = 2;
    while (suffix < 1000) {
        std::string attempt = sanitize_profile_name(sanitized + "_" + std::to_string(suffix));
        ++suffix;
        if (attempt.empty())
            continue;
        if (!exists(attempt))
            return attempt;
    }
    return sanitized;
}

static std::string profile_path(const std::string& name) {
    namespace fs = std::filesystem;
    return (fs::path(input_profiles_dir()) / (name + ".ini")).string();
}

std::vector<InputProfileInfo> list_input_profiles() {
    namespace fs = std::filesystem;
    std::vector<InputProfileInfo> out;
    ensure_input_profiles_dir();
    std::error_code ec;
    fs::path dir(input_profiles_dir());
    if (!fs::exists(dir, ec)) return out;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() != ".ini") continue;
        InputProfileInfo info;
        info.name = p.stem().string();
        info.read_only = (info.name == "Default");
        out.push_back(info);
    }
    std::sort(out.begin(), out.end(), [](const InputProfileInfo& a, const InputProfileInfo& b){
        if (a.name == "Default") return true;
        if (b.name == "Default") return false;
        return a.name < b.name;
    });
    return out;
}

bool load_input_profile(const std::string& name, InputBindings* out) {
    if (!out) return false;
    std::ifstream f(profile_path(name));
    if (!f.is_open()) return false;
    return parse_input_bindings_stream(f, *out);
}

bool save_input_profile(const std::string& name, const InputBindings& b, bool allow_overwrite) {
    if (!ensure_input_profiles_dir()) return false;
    namespace fs = std::filesystem;
    fs::path path = profile_path(name);
    if (!allow_overwrite && fs::exists(path)) return false;
    return save_input_bindings_to_ini(path.string(), b);
}

bool delete_input_profile(const std::string& name) {
    if (name == "Default") return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove(profile_path(name), ec);
    return !ec;
}

bool rename_input_profile(const std::string& from, const std::string& to) {
    if (from == "Default" || to == "Default") return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::rename(profile_path(from), profile_path(to), ec);
    return !ec;
}

std::string default_input_profile_name() {
    return "Default";
}

std::string load_active_input_profile_name() {
    std::ifstream f("config/input_active_profile.txt");
    if (!f.is_open()) return {};
    std::string line;
    std::getline(f, line);
    line = sanitize_profile_name(line);
    if (line.empty()) return {};
    return line;
}

void save_active_input_profile_name(const std::string& name) {
    if (name.empty()) return;
    ensure_dir("config");
    std::ofstream f("config/input_active_profile.txt");
    if (!f.is_open()) return;
    f << name << "\n";
}

void migrate_legacy_input_config() {
    namespace fs = std::filesystem;
    if (!fs::exists("config/input.ini")) return;
    std::ifstream f("config/input.ini");
    if (!f.is_open()) return;
    InputBindings tmp{};
    if (!parse_input_bindings_stream(f, tmp)) return;
    ensure_input_profiles_dir();
    save_input_profile("Default", tmp, true);
    fs::remove("config/input.ini");
}
