#include "demo_items.hpp"

#include "engine/audio.hpp"
#include "engine/graphics.hpp"
#include "globals.hpp"
#include "mods.hpp"
#include "state.hpp"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sol/sol.hpp>

namespace {

constexpr std::size_t kMaxDemoItemInstances = 32;

struct DemoItemRecord {
    DemoItemDef def;
    sol::protected_function on_use;
};

std::unique_ptr<sol::state> g_lua;
std::vector<DemoItemRecord> g_records;
std::vector<DemoItemDef> g_public_defs;
DemoItemPool g_item_pool{kMaxDemoItemInstances};
std::unordered_map<std::string, std::size_t> g_lookup;
std::string g_current_mod;
bool g_demo_items_active = false;
std::unordered_set<std::string> g_mod_filter;
bool g_use_mod_filter = false;

struct DemoApi {
    void alert(const std::string& text) const {
        if (!ss) return;
        State::Alert al;
        al.text = text;
        al.ttl = 1.4f;
        ss->alerts.push_back(al);
    }
    void set_player_position(float x, float y) const {
        if (ss) ss->player.pos = glm::vec2{x, y};
    }
    void play_sound(const std::string& key) const {
        if (!key.empty())
            ::play_sound(key);
    }
    void set_bonk_enabled(bool enabled) const {
        if (ss) ss->bonk.enabled = enabled;
    }
    void set_bonk_position(float x, float y) const {
        if (ss) ss->bonk.pos = glm::vec2{x, y};
    }
    bool set_item_position(const std::string& def_id, float x, float y) const;
    sol::object get_item_position(const std::string& def_id, sol::this_state state) const;
};

DemoApi g_api{};

DemoItemRecord* find_record_by_id(const std::string& id) {
    auto it = g_lookup.find(id);
    if (it == g_lookup.end())
        return nullptr;
    if (it->second >= g_records.size())
        return nullptr;
    return &g_records[it->second];
}

void clear_instances() {
    g_item_pool.clear();
}

DemoItemPool::Entry* find_instance_by_def_index(std::size_t def_index) {
    for (auto& entry : g_item_pool.entries()) {
        if (entry.active && entry.value.def_index == static_cast<int>(def_index))
            return &entry;
    }
    return nullptr;
}

DemoItemPool::Entry* find_instance_by_def_id(const std::string& def_id) {
    auto it = g_lookup.find(def_id);
    if (it == g_lookup.end())
        return nullptr;
    return find_instance_by_def_index(it->second);
}

bool DemoApi::set_item_position(const std::string& def_id, float x, float y) const {
    if (auto* entry = find_instance_by_def_id(def_id)) {
        entry->value.position = glm::vec2{x, y};
        return true;
    }
    return false;
}

sol::object DemoApi::get_item_position(const std::string& def_id, sol::this_state state) const {
    sol::state_view lua{state};
    if (auto* entry = find_instance_by_def_id(def_id)) {
        sol::table tbl = lua.create_table();
        tbl["x"] = entry->value.position.x;
        tbl["y"] = entry->value.position.y;
        return sol::make_object(lua, tbl);
    }
    return sol::make_object(lua, sol::lua_nil);
}

bool spawn_instance(std::size_t def_index, const glm::vec2& pos) {
    if (auto* entry = g_item_pool.acquire()) {
        entry->value.def_index = static_cast<int>(def_index);
        entry->value.position = pos;
        return true;
    }
    std::fprintf(stderr, "[demo_items] no free instance slots (max %zu)\n", kMaxDemoItemInstances);
    return false;
}

glm::vec2 read_vec2(const sol::object& obj, glm::vec2 fallback) {
    if (!obj.is<sol::table>()) return fallback;
    sol::table t = obj.as<sol::table>();
    glm::vec2 out = fallback;
    out.x = t.get_or("x", fallback.x);
    out.y = t.get_or("y", fallback.y);
    return out;
}

glm::vec3 read_color(const sol::object& obj, glm::vec3 fallback) {
    if (!obj.is<sol::table>()) return fallback;
    sol::table t = obj.as<sol::table>();
    glm::vec3 out = fallback;
    out.r = t.get_or("r", fallback.r);
    out.g = t.get_or("g", fallback.g);
    out.b = t.get_or("b", fallback.b);
    if (out.r > 1.001f || out.g > 1.001f || out.b > 1.001f) {
        out /= 255.0f;
    }
    out = glm::clamp(out, glm::vec3(0.0f), glm::vec3(1.0f));
    return out;
}

void apply_def_patch(DemoItemRecord& rec, const sol::table& patch) {
    if (auto label = patch.get<sol::optional<std::string>>("label"))
        rec.def.label = *label;
    if (auto radius = patch.get<sol::optional<double>>("radius"))
        rec.def.radius = static_cast<float>(*radius);
    if (auto pos_obj = patch.get<sol::object>("position"); pos_obj.valid())
        rec.def.position = read_vec2(pos_obj, rec.def.position);
    if (auto color_obj = patch.get<sol::object>("color"); color_obj.valid())
        rec.def.color = read_color(color_obj, rec.def.color);
    if (auto sprite_name = patch.get<sol::optional<std::string>>("sprite")) {
        rec.def.sprite_name = *sprite_name;
        rec.def.sprite_id = rec.def.sprite_name.empty() ? -1 : try_get_sprite_id(rec.def.sprite_name);
    }
}

std::string make_default_id() {
    std::string base = g_current_mod.empty() ? "item" : g_current_mod;
    base += "_";
    base += std::to_string(g_records.size());
    return base;
}

void rebuild_public_items() {
    g_public_defs.clear();
    g_public_defs.reserve(g_records.size());
    for (auto const& rec : g_records)
        g_public_defs.push_back(rec.def);
}

void register_api(sol::state& lua) {
    lua.new_usertype<DemoApi>("DemoAPI",
        "alert", &DemoApi::alert,
        "set_player_position", &DemoApi::set_player_position,
        "play_sound", &DemoApi::play_sound,
        "set_bonk_enabled", &DemoApi::set_bonk_enabled,
        "set_bonk_position", &DemoApi::set_bonk_position,
        "set_item_position", &DemoApi::set_item_position,
        "get_item_position", &DemoApi::get_item_position);
    lua["api"] = &g_api;
}

void register_bindings(sol::state& lua) {
    lua.set_function("register_item", [](sol::table t) {
        DemoItemRecord rec;

        rec.def.id = t.get_or("id", make_default_id());
        if (g_lookup.find(rec.def.id) != g_lookup.end()) {
            std::fprintf(stderr, "[demo_items] duplicate id '%s'\n", rec.def.id.c_str());
            return;
        }

        rec.def.label = t.get_or("label", rec.def.id);
        rec.def.position = read_vec2(t.get<sol::object>("position"), rec.def.position);
        rec.def.radius = t.get_or("radius", rec.def.radius);
        rec.def.color = read_color(t.get<sol::object>("color"), rec.def.color);

        rec.def.sprite_name = t.get_or("sprite", std::string{});
        rec.def.sprite_id = rec.def.sprite_name.empty() ? -1 : try_get_sprite_id(rec.def.sprite_name);

        sol::object cb = t.get<sol::object>("on_use");
        if (cb.is<sol::function>())
            rec.on_use = cb.as<sol::protected_function>();

        g_lookup.emplace(rec.def.id, g_records.size());
        g_records.push_back(std::move(rec));
    });

    lua.set_function("patch_item", [](sol::table t) {
        std::string id = t.get_or("id", std::string{});
        if (id.empty()) {
            std::fprintf(stderr, "[demo_items] patch_item missing id\n");
            return;
        }
        DemoItemRecord* rec = find_record_by_id(id);
        if (!rec) {
            std::fprintf(stderr, "[demo_items] patch_item unknown id '%s'\n", id.c_str());
            return;
        }
        apply_def_patch(*rec, t);
    });
}

sol::table make_item_table(sol::state& lua, const DemoItemDef& item,
                           const DemoItemInstance* inst) {
    sol::table tbl = lua.create_table();
    tbl["id"] = item.id;
    tbl["label"] = item.label;
    if (inst) {
        tbl["x"] = inst->position.x;
        tbl["y"] = inst->position.y;
    } else {
        tbl["x"] = item.position.x;
        tbl["y"] = item.position.y;
    }
    tbl["radius"] = item.radius;
    tbl["sprite"] = item.sprite_name;
    return tbl;
}

} // namespace

bool load_demo_item_defs() {
    unload_demo_item_defs();
    if (!mm)
        return false;

    g_lua = std::make_unique<sol::state>();
    auto& lua = *g_lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::os);
    register_api(lua);
    register_bindings(lua);

    auto mod_allowed = [](const std::string& id) {
        if (!g_use_mod_filter)
            return true;
        return g_mod_filter.find(id) != g_mod_filter.end();
    };

    namespace fs = std::filesystem;
    bool loaded_any = false;
    for (const auto& mod : mm->mods) {
        if (!mod_allowed(mod.name))
            continue;
        fs::path script = fs::path(mod.path) / "scripts" / "demo_items.lua";
        if (!fs::exists(script))
            continue;
        g_current_mod = mod.name;
        try {
            sol::protected_function_result r = lua.safe_script_file(script.string());
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[demo_items] %s\n", e.what());
            } else {
                loaded_any = true;
            }
        } catch (const sol::error& e) {
            std::fprintf(stderr, "[demo_items] exception loading %s: %s\n",
                         script.string().c_str(), e.what());
            State::Alert al;
            al.text = std::string("Mod load failed: ") + mod.name;
            al.ttl = 2.0f;
            al.purge_eof = true;
            if (ss)
                ss->alerts.push_back(al);
        }
        g_current_mod.clear();
    }

    rebuild_public_items();

    // Spawn demo instances: one per def at its authored position.
    clear_instances();
    for (std::size_t i = 0; i < g_public_defs.size(); ++i) {
        const auto& def = g_public_defs[i];
        spawn_instance(i, def.position);
    }

    if (!loaded_any)
        std::fprintf(stderr, "[demo_items] no demo_items.lua files found\n");
    g_demo_items_active = loaded_any;
    return loaded_any;
}

void unload_demo_item_defs() {
    g_lookup.clear();
    g_records.clear();
    g_public_defs.clear();
    g_current_mod.clear();
    g_lua.reset();
    g_demo_items_active = false;
}

const std::vector<DemoItemDef>& demo_item_defs() {
    return g_public_defs;
}

const std::vector<DemoItemPool::Entry>& demo_item_instance_slots() {
    return g_item_pool.entries();
}

const DemoItemDef* demo_item_def(const DemoItemInstance& inst) {
    if (inst.def_index < 0) return nullptr;
    if (static_cast<std::size_t>(inst.def_index) >= g_public_defs.size())
        return nullptr;
    return &g_public_defs[static_cast<std::size_t>(inst.def_index)];
}

void trigger_demo_item_use(const DemoItemInstance& inst) {
    if (!g_lua)
        return;
    if (inst.def_index < 0 || static_cast<std::size_t>(inst.def_index) >= g_records.size())
        return;
    DemoItemRecord& rec = g_records[static_cast<std::size_t>(inst.def_index)];
    if (!rec.on_use.valid())
        return;

    sol::table info = make_item_table(*g_lua, rec.def, &inst);
    sol::protected_function_result r;
    try {
        r = rec.on_use(info);
    } catch (const sol::error& e) {
        std::fprintf(stderr, "[demo_items] on_use exception (%s): %s\n",
                     rec.def.id.c_str(), e.what());
        State::Alert al;
        al.text = std::string("Pad failed: ") + rec.def.label;
        al.ttl = 1.5f;
        if (ss)
            ss->alerts.push_back(al);
        return;
    }
    if (!r.valid()) {
        sol::error e = r;
        std::fprintf(stderr, "[demo_items] on_use error (%s): %s\n",
                     rec.def.id.c_str(), e.what());
        State::Alert al;
        al.text = std::string("Pad error: ") + rec.def.label;
        al.ttl = 1.5f;
        if (ss)
            ss->alerts.push_back(al);
    }
}

bool demo_items_active() {
    return g_demo_items_active;
}

void set_demo_item_mod_filter(const std::vector<std::string>& ids) {
    g_mod_filter.clear();
    g_use_mod_filter = true;
    for (const auto& id : ids) {
        if (!id.empty())
            g_mod_filter.insert(id);
    }
}
