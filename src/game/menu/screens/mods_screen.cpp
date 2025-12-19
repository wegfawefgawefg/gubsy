#include "game/menu/screens/mods_screen.hpp"

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/mod_install.hpp"
#include "engine/mods.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <unordered_set>
#include <SDL2/SDL.h>

namespace {

constexpr const char* kModServerUrl = "http://127.0.0.1:8787";
constexpr int kModsPerPage = 4;

MenuCommandId g_cmd_prev_page = kMenuIdInvalid;
MenuCommandId g_cmd_next_page = kMenuIdInvalid;
MenuCommandId g_cmd_install = kMenuIdInvalid;
MenuCommandId g_cmd_uninstall = kMenuIdInvalid;

struct ModsScreenState {
    bool loaded = false;
    bool busy = false;
    std::string status{"Loading catalog..."};
    std::string page_text{"Page 0 / 0"};
    std::string search_query;
    std::string prev_search;
    int page = 0;
    std::vector<ModCatalogEntry> catalog;
    std::vector<int> filtered_indices;
    std::array<std::string, kModsPerPage> label_cache;
    std::array<std::string, kModsPerPage> summary_cache;
};

bool path_exists(const ModCatalogEntry& entry) {
    std::filesystem::path mods_root = mm && !mm->root.empty()
                                          ? std::filesystem::path(mm->root)
                                          : std::filesystem::path("mods");
    std::string folder = entry.folder.empty() ? entry.id : entry.folder;
    std::error_code ec;
    return std::filesystem::exists(mods_root / folder, ec);
}

void update_install_flags(ModsScreenState& state) {
    for (auto& entry : state.catalog) {
        if (entry.required) {
            entry.installed = true;
            entry.status_text = "Core";
            continue;
        }
        entry.installed = path_exists(entry);
        if (entry.installed)
            entry.status_text = "Installed";
        else if (entry.status_text.empty())
            entry.status_text = "Not installed";
    }
}

std::string summarize(const std::string& src, std::size_t max_len = 140) {
    if (src.size() <= max_len)
        return src;
    if (max_len < 4)
        return src.substr(0, max_len);
    std::string out = src.substr(0, max_len - 3);
    out += "...";
    return out;
}

SDL_Color badge_color_for(const std::string& status) {
    std::string lower = status;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("installing") != std::string::npos)
        return SDL_Color{120, 170, 255, 255};
    if (lower.find("remove") != std::string::npos || lower.find("failed") != std::string::npos)
        return SDL_Color{230, 120, 120, 255};
    if (lower.find("core") != std::string::npos)
        return SDL_Color{240, 210, 120, 255};
    if (lower.find("not install") != std::string::npos)
        return SDL_Color{235, 200, 110, 255};
    if (lower.find("installed") != std::string::npos)
        return SDL_Color{150, 220, 150, 255};
    return SDL_Color{170, 170, 190, 255};
}

void recalc_page_text(ModsScreenState& state) {
    int filtered = static_cast<int>(state.filtered_indices.size());
    if (filtered == 0) {
        state.page = 0;
        state.page_text = "Page 0 / 0";
        return;
    }
    int max_page = std::max(0, (filtered - 1) / kModsPerPage);
    state.page = std::clamp(state.page, 0, max_page);
    state.page_text = "Page " + std::to_string(state.page + 1) + " / " + std::to_string(max_page + 1);
}

int find_entry_index(const ModsScreenState& state, const std::string& id) {
    for (std::size_t i = 0; i < state.catalog.size(); ++i) {
        if (state.catalog[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void rebuild_filter(ModsScreenState& state) {
    state.filtered_indices.clear();
    std::string needle = state.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (std::size_t i = 0; i < state.catalog.size(); ++i) {
        const auto& entry = state.catalog[i];
        if (!needle.empty()) {
            std::string hay = entry.title + " " + entry.summary + " " + entry.author;
            std::transform(hay.begin(), hay.end(), hay.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(needle) == std::string::npos)
                continue;
        }
        state.filtered_indices.push_back(static_cast<int>(i));
    }
    int filtered = static_cast<int>(state.filtered_indices.size());
    if (filtered == 0) {
        state.page = 0;
        state.page_text = "Page 0 / 0";
        return;
    }
    int max_page = std::max(0, (filtered - 1) / kModsPerPage);
    state.page = std::clamp(state.page, 0, max_page);
    state.page_text = "Page " + std::to_string(state.page + 1) + " / " + std::to_string(max_page + 1);
}

bool ensure_catalog_loaded(ModsScreenState& state) {
    if (state.loaded || state.busy)
        return true;
    state.busy = true;
    std::string err;
    if (!fetch_mod_catalog(kModServerUrl, state.catalog, err)) {
        state.status = err.empty() ? "Failed to fetch catalog" : err;
        state.busy = false;
        return false;
    }
    update_install_flags(state);
    rebuild_filter(state);
    state.status = "Catalog loaded";
    state.loaded = true;
    state.busy = false;
    return true;
}

bool install_recursive(ModsScreenState& state,
                       int catalog_idx,
                       std::unordered_set<std::string>& visiting,
                       std::string& err) {
    if (catalog_idx < 0 || catalog_idx >= static_cast<int>(state.catalog.size()))
        return false;
    ModCatalogEntry& entry = state.catalog[static_cast<std::size_t>(catalog_idx)];
    if (entry.required)
        return true;
    if (entry.installed)
        return true;
    if (visiting.count(entry.id)) {
        err = "Dependency cycle detected";
        return false;
    }
    visiting.insert(entry.id);
    for (const auto& dep_id : entry.dependencies) {
        int dep_idx = find_entry_index(state, dep_id);
        if (dep_idx < 0) {
            err = "Missing dependency: " + dep_id;
            visiting.erase(entry.id);
            return false;
        }
        if (!install_recursive(state, dep_idx, visiting, err)) {
            visiting.erase(entry.id);
            return false;
        }
    }
    entry.status_text = "Installing...";
    bool ok = install_mod_from_catalog(kModServerUrl, entry, err);
    entry.installed = ok;
    entry.status_text = ok ? "Installed" : "Install failed";
    visiting.erase(entry.id);
    return ok;
}

bool uninstall_with_checks(ModsScreenState& state, int catalog_idx, std::string& err) {
    if (catalog_idx < 0 || catalog_idx >= static_cast<int>(state.catalog.size()))
        return false;
    ModCatalogEntry& entry = state.catalog[static_cast<std::size_t>(catalog_idx)];
    if (entry.required) {
        err = "Cannot remove core mod";
        return false;
    }
    for (const auto& other : state.catalog) {
        if (!other.installed)
            continue;
        if (std::find(other.dependencies.begin(), other.dependencies.end(), entry.id) != other.dependencies.end()) {
            err = "Required by " + other.title;
            return false;
        }
    }
    entry.status_text = "Removing...";
    if (!uninstall_mod(entry, err)) {
        entry.status_text = "Remove failed";
        return false;
    }
    entry.installed = false;
    entry.status_text = "Not installed";
    return true;
}

void command_prev(MenuContext& ctx, std::int32_t) {
    auto& state = ctx.state<ModsScreenState>();
    if (state.page > 0)
        state.page -= 1;
    recalc_page_text(state);
}

void command_next(MenuContext& ctx, std::int32_t) {
    auto& state = ctx.state<ModsScreenState>();
    int max_page = 0;
    if (!state.filtered_indices.empty())
        max_page = std::max(0, (static_cast<int>(state.filtered_indices.size()) - 1) / kModsPerPage);
    if (state.page < max_page)
        state.page += 1;
    recalc_page_text(state);
}

void command_install(MenuContext& ctx, std::int32_t payload) {
    auto& state = ctx.state<ModsScreenState>();
    if (state.busy)
        return;
    state.busy = true;
    std::string err;
    std::unordered_set<std::string> visiting;
    if (!install_recursive(state, payload, visiting, err)) {
        state.status = err.empty() ? "Install failed" : err;
    } else {
        state.status = "Installed mod";
    }
    update_install_flags(state);
    recalc_page_text(state);
    state.busy = false;
}

void command_uninstall(MenuContext& ctx, std::int32_t payload) {
    auto& state = ctx.state<ModsScreenState>();
    if (state.busy)
        return;
    state.busy = true;
    std::string err;
    if (!uninstall_with_checks(state, payload, err))
        state.status = err.empty() ? "Uninstall failed" : err;
    else
        state.status = "Removed mod";
    update_install_flags(state);
    recalc_page_text(state);
    state.busy = false;
}

BuiltScreen build_mods_screen(MenuContext& ctx) {
    auto& state = ctx.state<ModsScreenState>();
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    widgets.clear();
    frame_actions.clear();

    if (!ensure_catalog_loaded(state)) {
        state.loaded = false;
    }

    if (state.search_query != state.prev_search) {
        state.prev_search = state.search_query;
        rebuild_filter(state);
    }

    MenuWidget title;
    title.id = 500;
    title.slot = ModsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Mods Browser";
    widgets.push_back(title);

    MenuWidget status;
    status.id = 501;
    status.slot = ModsObjectID::STATUS;
    status.type = WidgetType::Label;
    status.label = state.status.c_str();
    widgets.push_back(status);

    MenuWidget page_label;
    page_label.id = 505;
    page_label.slot = ModsObjectID::PAGE;
    page_label.type = WidgetType::Label;
    page_label.label = state.page_text.c_str();
    widgets.push_back(page_label);

    MenuWidget search;
    search.id = 502;
    search.slot = ModsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &state.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search mods...";
    widgets.push_back(search);
    const std::size_t search_idx = widgets.size() - 1;

    MenuWidget prev_btn;
    prev_btn.id = 503;
    prev_btn.slot = ModsObjectID::PREV;
    prev_btn.type = WidgetType::Button;
    prev_btn.label = "<";
    prev_btn.on_select = MenuAction::run_command(g_cmd_prev_page);
    prev_btn.on_left = MenuAction::run_command(g_cmd_prev_page);
    prev_btn.on_right = MenuAction::run_command(g_cmd_next_page);
    widgets.push_back(prev_btn);
    const std::size_t prev_idx = widgets.size() - 1;

    MenuWidget next_btn;
    next_btn.id = 504;
    next_btn.slot = ModsObjectID::NEXT;
    next_btn.type = WidgetType::Button;
    next_btn.label = ">";
    next_btn.on_select = MenuAction::run_command(g_cmd_next_page);
    next_btn.on_left = MenuAction::run_command(g_cmd_prev_page);
    next_btn.on_right = MenuAction::run_command(g_cmd_next_page);
    widgets.push_back(next_btn);
    const std::size_t next_idx = widgets.size() - 1;

    const int start = state.page * kModsPerPage;
    WidgetId first_card_id = kMenuIdInvalid;
    WidgetId last_card_id = kMenuIdInvalid;
    constexpr WidgetId kCardIdBase = 520;
    const std::size_t cards_offset = widgets.size();
    for (int i = 0; i < kModsPerPage; ++i) {
        int absolute = start + i;
        MenuWidget card;
        card.id = static_cast<WidgetId>(kCardIdBase + static_cast<WidgetId>(i));
        card.slot = static_cast<UILayoutObjectId>(ModsObjectID::CARD0 + i);
        card.type = WidgetType::Button;
        if (absolute >= static_cast<int>(state.filtered_indices.size())) {
            card.label = "Empty";
            card.on_select = MenuAction::none();
            widgets.push_back(card);
            if (first_card_id == kMenuIdInvalid)
                first_card_id = card.id;
            last_card_id = card.id;
            continue;
        }
        int catalog_idx = state.filtered_indices[static_cast<std::size_t>(absolute)];
        ModCatalogEntry& entry = state.catalog[static_cast<std::size_t>(catalog_idx)];
        std::string& label_text = state.label_cache[static_cast<std::size_t>(i)];
        label_text = entry.title + "  v" + entry.version + "  by " + entry.author;
        std::string& summary_text = state.summary_cache[static_cast<std::size_t>(i)];
        summary_text = summarize(entry.summary, 96);
        card.label = label_text.c_str();
        card.secondary = summary_text.c_str();
        bool installed = entry.installed;
        card.on_select = MenuAction::run_command(installed ? g_cmd_uninstall : g_cmd_install,
                                                 catalog_idx);
        card.badge = entry.status_text.c_str();
        card.badge_color = badge_color_for(entry.status_text);
        card.on_left = MenuAction::run_command(g_cmd_prev_page);
        card.on_right = MenuAction::run_command(g_cmd_next_page);
        widgets.push_back(card);
        if (first_card_id == kMenuIdInvalid)
            first_card_id = card.id;
        last_card_id = card.id;
    }

    MenuWidget back_btn;
    back_btn.id = 560;
    back_btn.slot = ModsObjectID::BACK;
    back_btn.type = WidgetType::Button;
    back_btn.label = "Back";
    back_btn.on_select = MenuAction::pop();
    widgets.push_back(back_btn);
    const std::size_t back_idx = widgets.size() - 1;

    MenuWidget& search_ref = widgets[search_idx];
    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    search_ref.on_left = MenuAction::run_command(g_cmd_prev_page);
    search_ref.on_right = MenuAction::run_command(g_cmd_next_page);
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : back_ref.id;
    search_ref.nav_down = cards_start;
    search_ref.nav_left = prev_ref.id;
    search_ref.nav_right = next_ref.id;

    prev_ref.nav_left = search_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = search_ref.id;
    prev_ref.nav_down = cards_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = search_ref.id;
    next_ref.nav_down = cards_start;

    for (int i = 0; i < kModsPerPage; ++i) {
        MenuWidget& card = widgets[cards_offset + static_cast<std::size_t>(i)];
        WidgetId up_id = (i == 0) ? search_ref.id
                                  : widgets[cards_offset + static_cast<std::size_t>(i - 1)].id;
        WidgetId down_id = (i == kModsPerPage - 1)
                               ? back_ref.id
                               : widgets[cards_offset + static_cast<std::size_t>(i + 1)].id;
        card.nav_up = up_id;
        card.nav_down = down_id;
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
    }

    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : search_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::MODS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = search.id;
    return built;
}

} // namespace

void register_mods_menu_screen() {
    if (!es)
        return;
    g_cmd_prev_page = es->menu_commands.register_command(command_prev);
    g_cmd_next_page = es->menu_commands.register_command(command_next);
    g_cmd_install = es->menu_commands.register_command(command_install);
    g_cmd_uninstall = es->menu_commands.register_command(command_uninstall);

    MenuScreenDef def;
    def.id = MenuScreenID::MODS;
    def.layout = UILayoutID::MODS_SCREEN;
    def.state_ops = screen_state_ops<ModsScreenState>();
    def.build = build_mods_screen;
    es->menu_manager.register_screen(def);
}
