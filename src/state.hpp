#pragma once

#include "config.hpp"
#include "engine/input.hpp"
#include "engine/input_defs.hpp"
#include "modes.hpp"
#include "settings.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct State {
    struct Alert {
        std::string text;
        float age{0.0f};
        float ttl{2.0f};
        bool purge_eof{false};
    };

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
        bool audio_settings_dirty{false};
        int audio_nav_active_id{-1};
        uint8_t audio_nav_active_mask{0};
        float audio_slider_preview_cooldown[3]{0.0f, 0.0f, 0.0f};
        float audio_slider_preview_anchor[3]{0.0f, 0.0f, 0.0f};
        bool audio_slider_preview_anchor_valid[3]{false, false, false};

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

        // Mods browser mock state
        std::unordered_map<std::string, bool> mods_mock_install_state;
        std::vector<int> mods_visible_indices;
        int mods_catalog_page{0};
        int mods_total_pages{1};
        int mods_filtered_count{0};
        std::string mods_search_query;

        // Input hold-repeat for menu navigation/adjustment
        bool hold_left{false}, hold_right{false}, hold_up{false}, hold_down{false};
        float rpt_left{0.0f}, rpt_right{0.0f}, rpt_up{0.0f}, rpt_down{0.0f};
        bool prev_hold_left{false}, prev_hold_right{false}, prev_hold_up{false}, prev_hold_down{false};
        bool suppress_confirm_until_release{false};
        bool suppress_back_until_release{false};
        std::string text_input_title;
    };

    struct DemoPlayer {
        glm::vec2 pos{0.0f, 0.0f};
        glm::vec2 half_size{PLAYER_HALF_SIZE_UNITS, PLAYER_HALF_SIZE_UNITS};
        float speed_units_per_sec{PLAYER_MOVE_SPEED_UNITS};
    };

    struct BonkTarget {
        glm::vec2 pos{2.5f, 0.0f};
        glm::vec2 half_size{BONK_HALF_SIZE_UNITS, BONK_HALF_SIZE_UNITS};
        float cooldown{0.0f};
        std::string sound_key{"base:ui_confirm"};
        bool enabled{true};
    };

    bool running{true};
    std::string mode{modes::TITLE};

    double now{0.0};
    float dt{0.0f};
    float accumulator{0.0f};
    std::uint64_t frame{0};

    MouseInputs mouse_inputs{};
    MenuInputs menu_inputs{};
    MenuInputDebounceTimers menu_input_debounce_timers{};
    PlayingInputs playing_inputs{};
    PlayingInputDebounceTimers playing_input_debounce_timers{};
    InputState input_state{};
    InputBindings input_binds{};

    std::vector<Alert> alerts;
    MenuState menu{};

    DemoPlayer player{};
    BonkTarget bonk{};
    bool use_interact_prev{false};

    bool show_character_panel{false};
    bool show_gun_panel{true};
};

bool init_state();
void cleanup_state();
