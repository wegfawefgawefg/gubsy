#pragma once

#include "config.hpp"
#include "engine/alerts.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "mode_registry.hpp"
#include "user_profiles.hpp"
#include "engine/binds_profiles.hpp"

// managing input settings profiles, saves, and binds profiles
// load all profiles
// save one profile
// load all binds profiles
// save one binds profile
// load all settings profiles
// save one settings profile
// load all saves metadata
// load one save
// save one save


struct LobbySettings {
    std::string session_name{"Local Session"};
    int privacy{0}; // 0=Solo,1=Friends,2=Public
    int scenario_index{0};
    int difficulty{1}; // 0=Easy,1=Normal,2=Hard
    int max_players{1};
    bool allow_join{false};
    std::string seed;
    bool seed_randomized{true};
    int mod_page{0};
    int selected_mod{-1};
    std::vector<LobbyModEntry> mods;
};

struct MenuNavEdges {
    std::array<int, 4> targets{};
    std::array<bool, 4> set{};
    MenuNavEdges() {
        targets.fill(-1);
        set.fill(false);
    }
};

struct EngineState {
    bool running{true};
    
    double now{0.0};
    float dt{0.0f};
    float accumulator{0.0f};
    std::uint64_t frame{0};
    
    std::string mode{"none"};
    std::vector<ModeDesc> modes;
    std::unordered_map<std::string, std::size_t> mode_lookup;

    // user profiles
    std::vector<UserProfile> user_profiles;
    // binds profiles
    std::vector<BindsProfile> binds_profiles;
    // settings profiles
    std::vector<SettingsProfile> settings_profiles;

    struct MenuState {
        // Pages: 0=Main, 1=Settings hub
        int page{0};
        int focus_id{-1};
        bool mouse_last_used{true};
        int page_index{0};
        bool mouse_left_prev{false};
        int dragging_id{-1};
        bool ignore_mouse_until_release{false};

        // Audio sliders
        float vol_master{1.0f};
        float vol_music{1.0f};
        float vol_sfx{1.0f};

        // Video
        int video_res_index{0};
        int window_size_index{0};
        int window_mode_index{0};
        bool vsync{true};
        int frame_limit_index{0};
        float ui_scale{0.5f};

        // Controls
        float screen_shake{1.0f};
        float mouse_sens{0.5f};
        float controller_sens{0.5f};
        bool invert_x{false};
        bool invert_y{false};
        bool vibration_enabled{true};
        float vibration_magnitude{0.5f};

        // Binds capture + presets state
        int capture_action_id{-1};
        int binds_keys_page{0};
        int binds_list_page{0};
        std::string binds_current_preset;
        InputBindings binds_snapshot{};
        bool binds_dirty{false};
        std::vector<InputProfileInfo> binds_profiles;
        bool binds_text_input_active{false};
        int binds_text_input_mode{0};
        std::string binds_text_input_buffer;
        int binds_text_input_limit{0};
        std::string binds_text_input_target;
        std::string binds_text_input_error;
        float binds_text_input_error_timer{0.0f};
        int last_input_source{0};
        std::string binds_toast;
        float binds_toast_timer{0.0f};

        // Players presets selection
        int players_page{0};
        int players_count{1};
        std::vector<std::string> players_presets;

        // Mods browser state
        std::unordered_map<std::string, bool> mods_install_state;
        std::vector<int> mods_visible_indices;
        int mods_catalog_page{0};
        int mods_total_pages{1};
        int mods_filtered_count{0};
        std::string mods_search_query;
        std::vector<ModCatalogEntry> mods_catalog;
        bool mods_catalog_loaded{false};
        bool mods_catalog_loading{false};
        std::string mods_catalog_error;
        std::string mods_server_url{"http://127.0.0.1:8787"};

        // Input hold-repeat for menu navigation/adjustment
        bool hold_left{false}, hold_right{false}, hold_up{false}, hold_down{false};
        float rpt_left{0.0f}, rpt_right{0.0f}, rpt_up{0.0f}, rpt_down{0.0f};
        bool prev_hold_left{false}, prev_hold_right{false}, prev_hold_up{false}, prev_hold_down{false};
        bool suppress_confirm_until_release{false};
        bool suppress_back_until_release{false};
        std::string text_input_title;
        struct {
            std::string page;
            std::string section;
            std::unordered_map<std::string, RectNdc> rects;
        } layout_cache{};
        struct {
            bool enabled{false};
            bool dragging{false};
            bool resizing{false};
            bool dirty{false};
            bool snap{false};
            std::string active_id;
            glm::vec2 grab_offset{0.0f, 0.0f};
        } layout_edit{};
        struct GuiObjectDesc {
            std::string id;
            RectNdc rect;
            std::string label;
        };
        struct {
            std::string page;
            std::string section;
            std::vector<GuiObjectDesc> items;
            bool dirty{false};
            int next_id{1};
        } objects_cache{};
        struct {
            bool enabled{false};
            bool dragging{false};
            bool resizing{false};
            bool snap{false};
            int selected{-1};
            glm::vec2 grab_offset{0.0f, 0.0f};
            std::string rename_target;
        } objects_edit{};
        struct {
            enum class Mode { None, Layout, Nav, Object, Warp };
            enum class View { Screens, Screen };
            bool active{false};
            Page page{MAIN};
            Page prev_page{MAIN};
            Mode mode{Mode::None};
            View view{View::Screens};
            int selection{0};
            std::array<bool, 4> dir_prev{{false, false, false, false}};
            std::string status;
            bool show_help{false};
            bool snap_enabled{false};
        } gui_editor{};
        struct {
            std::string page;
            std::string section;
            std::unordered_map<int, MenuNavEdges> edges;
            bool loaded{false};
        } nav_cache{};
        struct {
            bool enabled{false};
            bool dirty{false};
            int selected_id{-1};
            int pending_dir{-1};
            std::array<bool, 4> dir_held{};
            float status_timer{0.0f};
            std::string status;
        } nav_edit{};
        LobbySettings lobby{};
    };

    InputBindings input_binds{};

    std::vector<Alert> alerts{};
    MenuState menu{};
};

bool init_engine_state();
void cleanup_engine_state();
