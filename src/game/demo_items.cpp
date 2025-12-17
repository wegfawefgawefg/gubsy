#include "demo_items.hpp"

#include "engine/audio.hpp"
#include "engine/graphics.hpp"
#include "engine/globals.hpp"
#include "game/mod_api/demo_items_internal.hpp"
#include "state.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sol/sol.hpp>
#include "engine/alerts.hpp"
#include <engine/globals.hpp>

namespace {

constexpr std::size_t kMaxDemoItemInstances = 32;

struct DemoItemRecord {
    DemoItemDef def;
    sol::protected_function on_use;
    std::string owner_mod;
};

std::vector<DemoItemRecord> g_records;
std::vector<DemoItemDef> g_public_defs;
DemoItemPool g_item_pool{kMaxDemoItemInstances};
std::unordered_map<std::string, std::size_t> g_lookup;
bool g_demo_items_active = false;

DemoPlayer* ensure_primary_player() {
    if (!ss)
        return nullptr;
    if (ss->players.empty())
        ss->players.push_back(DemoPlayer{});
    return &ss->players[0];
}

struct DemoApi {
    void alert(const std::string& text) const {
        Alert al;
        al.text = text;
        al.ttl = 1.4f;
        add_alert(al.text);
    }
    void set_player_position(float x, float y) const {
        if (DemoPlayer* player = ensure_primary_player())
            player->pos = glm::vec2{x, y};
    }
    void play_sound(const std::string& key) const {
        if (!key.empty())
            ::play_sound(key);
    }
    void set_bonk_enabled(bool enabled) const {
        ss->bonk.enabled = enabled;
    }
    void set_bonk_position(float x, float y) const {
        ss->bonk.pos = glm::vec2{x, y};
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

std::string make_default_id(const std::string& mod_id) {
    std::string base = mod_id.empty() ? "item" : mod_id;
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

sol::table make_item_table(sol::state_view lua, const DemoItemDef& item,
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

namespace demo_items_internal {

void expose_runtime_api(sol::state& lua) {
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

void register_item(const std::string& mod_id, const sol::table& t) {
    DemoItemRecord rec;
    rec.owner_mod = mod_id;
    rec.def.id = t.get_or("id", make_default_id(mod_id));
    if (g_lookup.find(rec.def.id) != g_lookup.end()) {
        std::fprintf(stderr, "[demo_items] duplicate id '%s'\n", rec.def.id.c_str());
        return;
    }

    rec.def.label = t.get_or("label", rec.def.id);
    rec.def.position = read_vec2(t.get<sol::object>("position"), rec.def.position);
    rec.def.radius = t.get_or("radius", rec.def.radius);
    rec.def.color = read_color(t.get<sol::object>("color"), rec.def.color);
    rec.def.sprite_name = t.get_or("sprite", rec.def.sprite_name);
    rec.def.sprite_id = rec.def.sprite_name.empty() ? -1 : try_get_sprite_id(rec.def.sprite_name);

    if (auto cb = t.get<sol::optional<sol::protected_function>>("on_use"))
        rec.on_use = *cb;

    g_lookup.emplace(rec.def.id, g_records.size());
    g_records.push_back(std::move(rec));
    std::printf("[demo_items] %s registered '%s' at (%.2f, %.2f)\n",
                mod_id.c_str(), g_records.back().def.id.c_str(),
                static_cast<double>(g_records.back().def.position.x),
                static_cast<double>(g_records.back().def.position.y));
}

bool patch_item(const std::string& mod_id, const sol::table& patch) {
    std::string id = patch.get_or("id", std::string{});
    if (id.empty()) {
        std::fprintf(stderr, "[demo_items] %s patch_item missing id\n", mod_id.c_str());
        return false;
    }
    DemoItemRecord* rec = find_record_by_id(id);
    if (!rec) {
        std::fprintf(stderr, "[demo_items] %s patch_item unknown id '%s'\n",
                     mod_id.c_str(), id.c_str());
        return false;
    }
    apply_def_patch(*rec, patch);
    std::fprintf(stderr, "[demo_items] %s patched '%s'\n", mod_id.c_str(), id.c_str());
    return true;
}

void remove_mod_items(const std::string& mod_id) {
    if (g_records.empty())
        return;
    std::vector<DemoItemRecord> kept;
    kept.reserve(g_records.size());
    for (auto& rec : g_records) {
        if (rec.owner_mod == mod_id)
            continue;
        kept.push_back(std::move(rec));
    }
    if (kept.size() == g_records.size())
        return;
    g_records = std::move(kept);
    g_lookup.clear();
    for (std::size_t i = 0; i < g_records.size(); ++i)
        g_lookup[g_records[i].def.id] = i;
    rebuild_public_items();
    g_demo_items_active = !g_records.empty();
}

void finalize_items() {
    rebuild_public_items();
    clear_instances();
    for (std::size_t i = 0; i < g_public_defs.size(); ++i) {
        const auto& def = g_public_defs[i];
        spawn_instance(i, def.position);
    }
    g_demo_items_active = !g_public_defs.empty();
}

bool has_active_items() {
    return g_demo_items_active;
}

} // namespace demo_items_internal

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
    if (inst.def_index < 0 || static_cast<std::size_t>(inst.def_index) >= g_records.size())
        return;
    DemoItemRecord& rec = g_records[static_cast<std::size_t>(inst.def_index)];
    if (!rec.on_use.valid())
        return;

    sol::state_view lua(rec.on_use.lua_state());
    sol::table info = make_item_table(lua, rec.def, &inst);
    sol::protected_function_result r;
    try {
        r = rec.on_use(info);
    } catch (const sol::error& e) {
        std::fprintf(stderr, "[demo_items] on_use exception (%s): %s\n",
                     rec.def.id.c_str(), e.what());
        add_alert(std::string("Pad failed: ") + rec.def.label);
        return;
    }
    if (!r.valid()) {
        sol::error e = r;
        std::fprintf(stderr, "[demo_items] on_use error (%s): %s\n",
                     rec.def.id.c_str(), e.what());
        add_alert(std::string("Pad error: ") + rec.def.label);
    } else {
        std::printf("[demo_items] triggered '%s' (%s)\n",
                    rec.def.id.c_str(), rec.owner_mod.c_str());
    }
}

bool demo_items_active() {
    return demo_items_internal::has_active_items();
}

bool load_demo_item_defs() {
    return demo_items_active();
}

void unload_demo_item_defs() {
    // Legacy stub retained for menu code; mods now stay loaded via ModHost.
}

void set_demo_item_mod_filter(const std::vector<std::string>&) {
    // Legacy stub retained for session lobby. Filtering handled by ModHost in future work.
}
