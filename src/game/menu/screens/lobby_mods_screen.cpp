#include "game/menu/screens/lobby_mods_screen.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/alerts.hpp"
#include "engine/globals.hpp"
#include "engine/mod_install.hpp"
#include "engine/mod_host.hpp"
#include "engine/mods.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kModsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 600;
constexpr WidgetId kStatusWidgetId = 601;
constexpr WidgetId kSearchWidgetId = 602;
constexpr WidgetId kPageLabelWidgetId = 603;
constexpr WidgetId kPrevButtonId = 604;
constexpr WidgetId kNextButtonId = 605;
constexpr WidgetId kBackButtonId = 630;
constexpr WidgetId kFirstCardWidgetId = 620;
constexpr const char* kModServerUrl = "http://127.0.0.1:8787";

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_toggle_mod = kMenuIdInvalid;

struct SessionModEntry {
    std::string id;
    std::string title;
    std::string author;
    std::string description;
    std::string version;
    std::string game_version;
    std::vector<std::string> dependencies;
    bool required{false};
    bool installed{false};
    bool enabled{false};
};

struct LobbyModsState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::string status_message;
    std::vector<int> filtered_indices;
    std::string search_query;
    std::string prev_search;
    bool catalog_loaded{false};
    bool busy{false};
    std::vector<ModCatalogEntry> catalog;
    std::vector<SessionModEntry> entries;
};

MenuWidget make_label_widget(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Label;
    w.label = label;
    return w;
}

MenuWidget make_button_widget(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Button;
    w.label = label;
    w.on_select = action;
    return w;
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<LobbyModsState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

bool path_exists(const ModCatalogEntry& entry) {
    std::filesystem::path mods_root = mm && !mm->root.empty()
                                          ? std::filesystem::path(mm->root)
                                          : std::filesystem::path("mods");
    std::string folder = entry.folder.empty() ? entry.id : entry.folder;
    if (folder.empty())
        folder = entry.id;
    std::error_code ec;
    return std::filesystem::exists(mods_root / folder, ec);
}

bool version_compatible(const SessionModEntry& entry) {
    const std::string& required = required_mod_game_version();
    if (required.empty() || entry.game_version.empty())
        return true;
    return entry.game_version == required;
}

SDL_Color badge_color_for(const std::string& status) {
    std::string lower = status;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("incompatible") != std::string::npos || lower.find("needs game") != std::string::npos)
        return SDL_Color{240, 190, 120, 255};
    if (lower.find("core") != std::string::npos)
        return SDL_Color{240, 210, 120, 255};
    if (lower.find("not install") != std::string::npos)
        return SDL_Color{235, 200, 110, 255};
    if (lower.find("not enabled") != std::string::npos)
        return SDL_Color{200, 160, 120, 255};
    if (lower.find("enabled") != std::string::npos)
        return SDL_Color{150, 220, 150, 255};
    return SDL_Color{170, 170, 190, 255};
}

MenuStyle style_for_entry(const SessionModEntry& entry,
                          bool version_ok,
                          bool locked,
                          bool interactive) {
    MenuStyle style;
    style.bg_r = 26;
    style.bg_g = 26;
    style.bg_b = 34;
    style.fg_r = 230;
    style.fg_g = 230;
    style.fg_b = 240;
    style.focus_r = 140;
    style.focus_g = 200;
    style.focus_b = 255;

    if (!version_ok) {
        style.bg_r = 90;
        style.bg_g = 32;
        style.bg_b = 32;
        style.focus_r = 255;
        style.focus_g = 160;
        style.focus_b = 120;
    } else if (entry.required) {
        style.bg_r = 88;
        style.bg_g = 70;
        style.bg_b = 28;
        style.focus_r = 255;
        style.focus_g = 210;
        style.focus_b = 120;
    } else if (entry.enabled) {
        style.bg_r = 30;
        style.bg_g = 60;
        style.bg_b = 42;
        style.focus_r = 120;
        style.focus_g = 230;
        style.focus_b = 170;
    } else if (!entry.installed) {
        style.bg_r = 28;
        style.bg_g = 36;
        style.bg_b = 68;
    }

    if (locked) {
        style.bg_r = static_cast<std::uint8_t>((style.bg_r + 60) / 2);
        style.bg_g = static_cast<std::uint8_t>((style.bg_g + 60) / 2);
        style.bg_b = static_cast<std::uint8_t>((style.bg_b + 60) / 2);
    }

    if (!interactive) {
        style.fg_r = 170;
        style.fg_g = 170;
        style.fg_b = 180;
        style.focus_r = 140;
        style.focus_g = 140;
        style.focus_b = 150;
    }
    return style;
}

int find_catalog_index(const std::vector<ModCatalogEntry>& catalog, const std::string& id) {
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (catalog[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

int find_lobby_entry(const LobbySession& lobby, const std::string& id) {
    for (std::size_t i = 0; i < lobby.mods.size(); ++i) {
        if (lobby.mods[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void ensure_lobby_entry(LobbySession& lobby, const SessionModEntry& entry) {
    int idx = find_lobby_entry(lobby, entry.id);
    if (idx >= 0) {
        LobbyModEntry& target = lobby.mods[static_cast<std::size_t>(idx)];
        target.title = entry.title;
        target.author = entry.author;
        target.description = entry.description;
        target.dependencies = entry.dependencies;
        target.required = entry.required;
        if (target.required)
            target.enabled = true;
        return;
    }
    LobbyModEntry target;
    target.id = entry.id;
    target.title = entry.title;
    target.author = entry.author;
    target.description = entry.description;
    target.dependencies = entry.dependencies;
    target.required = entry.required;
    target.enabled = entry.enabled || entry.required;
    lobby.mods.push_back(std::move(target));
}

bool ensure_catalog_loaded(LobbyModsState& st) {
    if (st.catalog_loaded || st.busy)
        return st.catalog_loaded;
    st.busy = true;
    std::string err;
    if (!fetch_mod_catalog(kModServerUrl, st.catalog, err)) {
        st.status_message = err.empty() ? "Failed to fetch catalog" : err;
        st.busy = false;
        return false;
    }
    st.status_message = "Catalog loaded";
    st.catalog_loaded = true;
    st.busy = false;
    return true;
}

void rebuild_entries(LobbyModsState& st, LobbySession& lobby) {
    st.entries.clear();
    std::unordered_map<std::string, bool> enabled_map;
    enabled_map.reserve(lobby.mods.size());
    for (const auto& mod : lobby.mods)
        enabled_map[mod.id] = mod.enabled || mod.required;

    std::unordered_set<std::string> seen;
    if (st.catalog_loaded) {
        for (const auto& cat : st.catalog) {
            SessionModEntry entry;
            entry.id = cat.id;
            entry.title = cat.title.empty() ? cat.id : cat.title;
            entry.author = cat.author;
            entry.description = cat.description.empty() ? cat.summary : cat.description;
            entry.version = cat.version;
            entry.game_version = cat.game_version;
            entry.dependencies = cat.dependencies;
            entry.required = cat.required || cat.id == "base";
            entry.installed = path_exists(cat);
            entry.enabled = entry.required || enabled_map[entry.id];
            st.entries.push_back(entry);
            seen.insert(entry.id);
        }
    }

    if (mm) {
        for (const auto& mod : mm->mods) {
            if (seen.count(mod.name))
                continue;
            SessionModEntry entry;
            entry.id = mod.name;
            entry.title = mod.title.empty() ? mod.name : mod.title;
            entry.author = mod.author;
            entry.description = mod.description;
            entry.version = mod.version;
            entry.game_version = mod.game_version;
            entry.dependencies = mod.deps;
            entry.required = mod.required || mod.name == "base";
            entry.installed = true;
            entry.enabled = entry.required || enabled_map[entry.id];
            st.entries.push_back(std::move(entry));
            seen.insert(entry.id);
        }
    }

    for (const auto& entry : st.entries)
        ensure_lobby_entry(lobby, entry);
}

bool enable_with_dependencies(LobbyModsState& st,
                              LobbySession& lobby,
                              int entry_index,
                              std::unordered_set<std::string>& visiting,
                              std::string& err) {
    if (entry_index < 0 || entry_index >= static_cast<int>(st.entries.size()))
        return false;
    SessionModEntry& entry = st.entries[static_cast<std::size_t>(entry_index)];
    if (entry.required) {
        ensure_lobby_entry(lobby, entry);
        int lobby_idx = find_lobby_entry(lobby, entry.id);
        if (lobby_idx >= 0)
            lobby.mods[static_cast<std::size_t>(lobby_idx)].enabled = true;
        entry.enabled = true;
        return true;
    }
    if (!version_compatible(entry)) {
        if (!entry.game_version.empty())
            err = "Needs game v" + entry.game_version;
        else
            err = "Incompatible mod";
        return false;
    }
    if (visiting.count(entry.id)) {
        err = "Dependency cycle detected";
        return false;
    }
    visiting.insert(entry.id);
    for (const auto& dep_id : entry.dependencies) {
        int dep_entry_idx = -1;
        for (std::size_t i = 0; i < st.entries.size(); ++i) {
            if (st.entries[i].id == dep_id) {
                dep_entry_idx = static_cast<int>(i);
                break;
            }
        }
        if (dep_entry_idx < 0) {
            err = "Missing dependency: " + dep_id;
            visiting.erase(entry.id);
            return false;
        }
        if (!enable_with_dependencies(st, lobby, dep_entry_idx, visiting, err)) {
            visiting.erase(entry.id);
            return false;
        }
    }

    if (!entry.installed) {
        int cat_idx = find_catalog_index(st.catalog, entry.id);
        if (cat_idx < 0) {
            err = "Mod not installed: " + entry.id;
            visiting.erase(entry.id);
            return false;
        }
        std::string install_err;
        if (!install_mod_from_catalog(kModServerUrl, st.catalog[static_cast<std::size_t>(cat_idx)], install_err)) {
            err = install_err.empty() ? "Install failed" : install_err;
            visiting.erase(entry.id);
            return false;
        }
        lobby_refresh_mods();
        entry.installed = true;
    }

    ensure_lobby_entry(lobby, entry);
    int lobby_idx = find_lobby_entry(lobby, entry.id);
    if (lobby_idx >= 0)
        lobby.mods[static_cast<std::size_t>(lobby_idx)].enabled = true;
    entry.enabled = true;
    visiting.erase(entry.id);
    return true;
}

bool has_enabled_dependents(const LobbySession& lobby, const std::string& id, std::string& out) {
    for (const auto& entry : lobby.mods) {
        if (!entry.enabled || entry.required)
            continue;
        if (std::find(entry.dependencies.begin(), entry.dependencies.end(), id) != entry.dependencies.end()) {
            out = entry.title.empty() ? entry.id : entry.title;
            return true;
        }
    }
    return false;
}

void command_toggle_mod(MenuContext& ctx, std::int32_t index) {
    auto& st = ctx.state<LobbyModsState>();
    if (st.busy)
        return;
    LobbySession& lobby = lobby_state();
    if (index < 0 || index >= static_cast<int>(st.entries.size()))
        return;
    SessionModEntry& entry = st.entries[static_cast<std::size_t>(index)];
    if (entry.required)
        return;

    if (entry.enabled) {
        std::string dependent;
        if (has_enabled_dependents(lobby, entry.id, dependent)) {
            add_alert("Disable dependent mod first: " + dependent);
            return;
        }
        int lobby_idx = find_lobby_entry(lobby, entry.id);
        if (lobby_idx >= 0)
            lobby.mods[static_cast<std::size_t>(lobby_idx)].enabled = false;
        entry.enabled = false;
        return;
    }

    st.busy = true;
    std::unordered_set<std::string> visiting;
    std::string err;
    if (!enable_with_dependencies(st, lobby, index, visiting, err)) {
        add_alert(err.empty() ? "Failed to enable mod" : err);
    }
    st.busy = false;
}

void rebuild_filter(LobbyModsState& st, const std::vector<SessionModEntry>& mods) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (std::size_t i = 0; i < mods.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = mods[i].title + " " + mods[i].author + " " + mods[i].description + " " + mods[i].id;
            std::transform(hay.begin(), hay.end(), hay.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(needle) == std::string::npos)
                continue;
        }
        st.filtered_indices.push_back(static_cast<int>(i));
    }

    int filtered = static_cast<int>(st.filtered_indices.size());
    if (filtered == 0) {
        st.page = 0;
        st.total_pages = 1;
        st.page_text = "Page 0 / 0";
        return;
    }
    st.total_pages = std::max(1, (filtered + kModsPerPage - 1) / kModsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

BuiltScreen build_lobby_mods(MenuContext& ctx) {
    LobbySession& lobby = lobby_state();
    lobby_refresh_mods();

    auto& st = ctx.state<LobbyModsState>();
    ensure_catalog_loaded(st);
    rebuild_entries(st, lobby);
    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st, st.entries);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != st.entries.size()) {
        rebuild_filter(st, st.entries);
    }
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(8);

    widgets.push_back(make_label_widget(kTitleWidgetId, LobbyModsObjectID::TITLE, "Session Mods"));

    st.status_text = std::to_string(st.filtered_indices.size()) +
                     (st.filtered_indices.size() == 1 ? " mod" : " mods");
    MenuWidget status_label = make_label_widget(kStatusWidgetId, LobbyModsObjectID::STATUS, st.status_text.c_str());
    status_label.label = st.status_text.c_str();
    widgets.push_back(status_label);

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = LobbyModsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search mods...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, LobbyModsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, LobbyModsObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, LobbyModsObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kModsPerPage);
    std::size_t cards_offset = widgets.size();

    int start_index = st.page * kModsPerPage;
    for (int i = 0; i < kModsPerPage; ++i) {
        int filtered_idx = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(LobbyModsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (filtered_idx < static_cast<int>(st.filtered_indices.size())) {
            int entry_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            const SessionModEntry& entry = st.entries[static_cast<std::size_t>(entry_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            card.label = entry.title.c_str();
            std::string subtitle;
            subtitle = entry.version.empty() ? "" : ("v" + entry.version);
            if (!entry.author.empty()) {
                if (!subtitle.empty())
                    subtitle += "  ·  ";
                subtitle += "by " + entry.author;
            }
            if (!entry.game_version.empty()) {
                if (!subtitle.empty())
                    subtitle += "  ·  ";
                subtitle += "Game v" + entry.game_version;
            }
            if (!entry.description.empty()) {
                if (!subtitle.empty())
                    subtitle += "  ·  ";
                subtitle += entry.description;
            }
            if (!entry.game_version.empty()) {
                if (!subtitle.empty())
                    subtitle += "  ·  ";
                subtitle += "Game v" + entry.game_version;
            }
            text_cache.emplace_back(std::move(subtitle));
            card.secondary = text_cache.back().c_str();
            std::string dependent_title;
            bool has_dependents = has_enabled_dependents(lobby, entry.id, dependent_title);
            bool version_ok = version_compatible(entry);
            bool can_enable = !entry.required && !entry.enabled && version_ok;
            bool interactive = can_enable || entry.enabled;
            card.on_select = interactive ? MenuAction::run_command(g_cmd_toggle_mod, entry_index)
                                         : MenuAction::none();
            card.style = style_for_entry(entry, version_ok, has_dependents || entry.required, interactive);
            if (!version_ok) {
                if (!entry.game_version.empty()) {
                    text_cache.emplace_back("Needs game v" + entry.game_version);
                    card.badge = text_cache.back().c_str();
                } else {
                    card.badge = "Incompatible";
                }
            } else if (entry.required) {
                card.badge = "Core";
            } else if (!entry.installed) {
                card.badge = "Not installed";
            } else if (entry.enabled) {
                card.badge = "Enabled";
            } else {
                card.badge = "Not enabled";
            }
            if (card.badge)
                card.badge_color = badge_color_for(card.badge);
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, LobbyModsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& search_ref = widgets[search_idx];
    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : back_ref.id;

    search_ref.nav_down = cards_start;
    search_ref.nav_left = search_ref.id;
    search_ref.nav_right = prev_ref.id;
    search_ref.nav_up = search_ref.id;

    prev_ref.nav_left = search_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = search_ref.id;
    prev_ref.nav_down = cards_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = search_ref.id;
    next_ref.nav_down = cards_start;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[cards_offset + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? search_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : search_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::LOBBY_MODS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = !card_ids.empty() ? card_ids.front() : back_ref.id;
    return built;
}

} // namespace

void register_lobby_mods_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_toggle_mod == kMenuIdInvalid)
        g_cmd_toggle_mod = es->menu_commands.register_command(command_toggle_mod);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_MODS;
    def.layout = UILayoutID::LOBBY_MODS_SCREEN;
    def.state_ops = screen_state_ops<LobbyModsState>();
    def.build = build_lobby_mods;
    es->menu_manager.register_screen(def);
}
