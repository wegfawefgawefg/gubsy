#include "demo_content.hpp"

#include "globals.hpp"
#include "mods.hpp"
#include "state.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

#include <glm/glm.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {
glm::vec2 read_vec2(lua_State* L, int index, const glm::vec2& fallback) {
    glm::vec2 result = fallback;
    int abs = lua_absindex(L, index);
    if (!lua_istable(L, abs))
        return result;
    lua_getfield(L, abs, "x");
    if (lua_isnumber(L, -1))
        result.x = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, abs, "y");
    if (lua_isnumber(L, -1))
        result.y = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return result;
}

float read_number_field(lua_State* L, int index, const char* key, float fallback) {
    float value = fallback;
    int abs = lua_absindex(L, index);
    lua_getfield(L, abs, key);
    if (lua_isnumber(L, -1))
        value = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return value;
}

std::string read_string_field(lua_State* L, int index, const char* key, const std::string& fallback) {
    std::string value = fallback;
    int abs = lua_absindex(L, index);
    lua_getfield(L, abs, key);
    if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        if (s)
            value = s;
    }
    lua_pop(L, 1);
    return value;
}
} // namespace

bool load_demo_content() {
    if (!ss || !mm)
        return false;

    namespace fs = std::filesystem;
    lua_State* L = luaL_newstate();
    if (!L)
        return false;
    luaL_openlibs(L);

    bool loaded = false;
    for (const auto& mod : mm->mods) {
        fs::path script = fs::path(mod.path) / "scripts" / "demo.lua";
        if (!fs::exists(script))
            continue;

        if (luaL_dofile(L, script.string().c_str()) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[demo] failed to load %s: %s\n",
                         script.string().c_str(), err ? err : "(unknown)");
            lua_pop(L, 1);
            continue;
        }

        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        // Copy current values before overriding
        auto player = ss->player;
        auto bonk = ss->bonk;

        lua_getfield(L, -1, "player");
        if (lua_istable(L, -1)) {
            player.speed_units_per_sec =
                read_number_field(L, -1, "speed", player.speed_units_per_sec);
            lua_getfield(L, -1, "start");
            if (lua_istable(L, -1))
                player.pos = read_vec2(L, -1, player.pos);
            lua_pop(L, 1);
            lua_getfield(L, -1, "size");
            if (lua_istable(L, -1))
                player.half_size = read_vec2(L, -1, player.half_size);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "bonk");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "position");
            if (lua_istable(L, -1))
                bonk.pos = read_vec2(L, -1, bonk.pos);
            lua_pop(L, 1);
            lua_getfield(L, -1, "size");
            if (lua_istable(L, -1))
                bonk.half_size = read_vec2(L, -1, bonk.half_size);
            lua_pop(L, 1);
            bonk.sound_key = read_string_field(L, -1, "sound", bonk.sound_key);
        }
        lua_pop(L, 1);

        lua_pop(L, 1); // pop root table

        ss->player = player;
        ss->bonk = bonk;
        loaded = true;
        std::printf("[demo] loaded content from %s\n", script.string().c_str());
        break;
    }

    lua_close(L);
    if (!loaded)
        std::fprintf(stderr, "[demo] no demo.lua found; using defaults\n");
    return loaded;
}
