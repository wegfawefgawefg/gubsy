#include "src/lobby_state.hpp"

#include "src/alerts.hpp"
#include "src/binds_profiles.hpp"
#include "src/engine_state.hpp"
#include "src/input_settings_profiles.hpp"
#include "src/input_sources.hpp"
#include "src/player.hpp"
#include "src/user_profiles.hpp"

#include <algorithm>

namespace {

constexpr int kMaxLocalPlayers = 32;

template <typename T, typename Pred> T* find_if_ptr(std::vector<T>& items, Pred pred) {
    auto it = std::find_if(items.begin(), items.end(), pred);
    if (it == items.end())
        return nullptr;
    return &*it;
}

template <typename T, typename Pred> const T* find_if_ptr(const std::vector<T>& items, Pred pred) {
    auto it = std::find_if(items.begin(), items.end(), pred);
    if (it == items.end())
        return nullptr;
    return &*it;
}

UserProfile& ensure_default_user_profile(EngineState& engine) {
    if (engine.user_profiles_pool.empty())
        engine.user_profiles_pool.push_back(create_default_user_profile());
    return engine.user_profiles_pool.front();
}

BindsProfile& ensure_default_binds_profile(EngineState& engine) {
    if (engine.binds_profiles.empty())
        engine.binds_profiles.push_back(create_default_binds_profile());
    return engine.binds_profiles.front();
}

InputSettingsProfile& ensure_default_input_settings_profile(EngineState& engine) {
    if (engine.input_settings_profiles.empty())
        engine.input_settings_profiles.push_back(create_default_input_settings_profile());
    return engine.input_settings_profiles.front();
}

UserProfile* find_user_profile(EngineState& engine, int id) {
    return find_if_ptr(engine.user_profiles_pool,
                       [id](const UserProfile& profile) { return profile.id == id; });
}

const UserProfile* find_user_profile(const EngineState& engine, int id) {
    return find_if_ptr(engine.user_profiles_pool,
                       [id](const UserProfile& profile) { return profile.id == id; });
}

BindsProfile* find_binds_profile(EngineState& engine, int id) {
    return find_if_ptr(engine.binds_profiles,
                       [id](const BindsProfile& profile) { return profile.id == id; });
}

InputSettingsProfile* find_input_settings_profile(EngineState& engine, int id) {
    return find_if_ptr(engine.input_settings_profiles,
                       [id](const InputSettingsProfile& profile) { return profile.id == id; });
}

bool same_device(GubsyLobbyDeviceAssignment a, GubsyLobbyDeviceAssignment b) {
    return a.type == b.type && a.device_id == b.device_id;
}

const InputSource* first_input_source_of_type(const EngineState& engine, InputSourceType type) {
    auto it = std::find_if(engine.input_sources.begin(), engine.input_sources.end(),
                           [type](const InputSource& source) { return source.type == type; });
    return it == engine.input_sources.end() ? nullptr : &*it;
}

void assign_default_devices(EngineState& engine, GubsyLobbyPlayer& player, bool primary_player) {
    if (!player.devices.empty())
        return;

    if (const InputSource* keyboard = first_input_source_of_type(engine, InputSourceType::Keyboard))
        player.devices.push_back(gubsy_lobby_device_from_input_source(*keyboard));

    if (primary_player) {
        if (const InputSource* gamepad =
                first_input_source_of_type(engine, InputSourceType::Gamepad))
            player.devices.push_back(gubsy_lobby_device_from_input_source(*gamepad));
    }
}

void persist_user_profile_choice(UserProfile& profile, const GubsyLobbyPlayer& player) {
    profile.last_binds_profile_id = player.binds_profile_id;
    profile.last_input_settings_profile_id = player.input_settings_profile_id;
    (void)save_user_profile(profile);
}

void sync_engine_players_from_lobby(EngineState& engine) {
    engine.players.clear();
    for (const GubsyLobbyPlayer& lobby_player : engine.lobby.local_players) {
        Player player;
        if (const UserProfile* profile = find_user_profile(engine, lobby_player.user_profile_id)) {
            player.has_active_profile = true;
            player.profile = *profile;
            player.user_profile_id = profile->id;
        }
        player.binds_profile_id = lobby_player.binds_profile_id;
        player.input_settings_profile_id = lobby_player.input_settings_profile_id;
        player.devices = lobby_player.devices;
        engine.players.push_back(std::move(player));
    }
}

void fill_missing_player_choices(EngineState& engine, GubsyLobbyPlayer& player,
                                 bool primary_player) {
    UserProfile& fallback_user = ensure_default_user_profile(engine);
    BindsProfile& fallback_binds = ensure_default_binds_profile(engine);
    InputSettingsProfile& fallback_input = ensure_default_input_settings_profile(engine);

    if (!find_user_profile(engine, player.user_profile_id))
        player.user_profile_id = fallback_user.id;

    UserProfile* selected_user = find_user_profile(engine, player.user_profile_id);
    int preferred_binds = selected_user ? selected_user->last_binds_profile_id : -1;
    int preferred_input = selected_user ? selected_user->last_input_settings_profile_id : -1;

    if (!find_binds_profile(engine, player.binds_profile_id)) {
        player.binds_profile_id =
            find_binds_profile(engine, preferred_binds) ? preferred_binds : fallback_binds.id;
    }
    if (!find_input_settings_profile(engine, player.input_settings_profile_id)) {
        player.input_settings_profile_id = find_input_settings_profile(engine, preferred_input)
                                               ? preferred_input
                                               : fallback_input.id;
    }
    assign_default_devices(engine, player, primary_player);
    if (selected_user)
        persist_user_profile_choice(*selected_user, player);
}

} // namespace

GubsyLobbyState& gubsy_lobby_state(EngineState& engine) {
    return engine.lobby;
}

const GubsyLobbyState& gubsy_lobby_state(const EngineState& engine) {
    return engine.lobby;
}

void gubsy_lobby_ensure_ready(EngineState& engine) {
    ensure_default_user_profile(engine);
    ensure_default_binds_profile(engine);
    ensure_default_input_settings_profile(engine);
    if (engine.lobby.contract.net_protocol.empty())
        engine.lobby.contract.net_protocol = session_contract_default_net_protocol();
    if (engine.lobby.contract.session_phase.empty())
        engine.lobby.contract.session_phase = "lobby";
    if (engine.input_sources.empty())
        detect_input_sources(engine);
    if (engine.lobby.local_players.empty())
        engine.lobby.local_players.push_back({});

    for (std::size_t i = 0; i < engine.lobby.local_players.size(); ++i)
        fill_missing_player_choices(engine, engine.lobby.local_players[i], i == 0);

    int max_index = static_cast<int>(engine.lobby.local_players.size()) - 1;
    engine.lobby.selected_player_index =
        std::clamp(engine.lobby.selected_player_index, 0, std::max(0, max_index));
    GubsyLobbyConfigProvider& provider = engine.lobby_config_provider;
    if (provider.ensure_defaults)
        provider.ensure_defaults(provider.user_data, engine.lobby);
    sync_engine_players_from_lobby(engine);
}

int gubsy_lobby_add_local_player(EngineState& engine) {
    gubsy_lobby_ensure_ready(engine);
    if (static_cast<int>(engine.lobby.local_players.size()) >= kMaxLocalPlayers) {
        add_alert(engine, "Local player limit reached");
        return static_cast<int>(engine.lobby.local_players.size()) - 1;
    }
    engine.lobby.local_players.push_back({});
    int index = static_cast<int>(engine.lobby.local_players.size()) - 1;
    engine.lobby.selected_player_index = index;
    fill_missing_player_choices(engine, engine.lobby.local_players.back(), false);
    sync_engine_players_from_lobby(engine);
    return index;
}

void gubsy_lobby_remove_local_player(EngineState& engine, int player_index) {
    gubsy_lobby_ensure_ready(engine);
    if (engine.lobby.local_players.size() <= 1) {
        add_alert(engine, "At least one local player is required");
        return;
    }
    if (player_index < 0 || player_index >= static_cast<int>(engine.lobby.local_players.size()))
        return;
    engine.lobby.local_players.erase(engine.lobby.local_players.begin() + player_index);
    gubsy_lobby_select_player(
        engine, std::min(player_index, static_cast<int>(engine.lobby.local_players.size()) - 1));
    sync_engine_players_from_lobby(engine);
}

void gubsy_lobby_select_player(EngineState& engine, int player_index) {
    if (engine.lobby.local_players.empty()) {
        engine.lobby.selected_player_index = 0;
        return;
    }
    int max_index = static_cast<int>(engine.lobby.local_players.size()) - 1;
    engine.lobby.selected_player_index = std::clamp(player_index, 0, max_index);
}

GubsyLobbyPlayer* gubsy_lobby_player(EngineState& engine, int player_index) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.lobby.local_players.size()))
        return nullptr;
    return &engine.lobby.local_players[static_cast<std::size_t>(player_index)];
}

const GubsyLobbyPlayer* gubsy_lobby_player(const EngineState& engine, int player_index) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.lobby.local_players.size()))
        return nullptr;
    return &engine.lobby.local_players[static_cast<std::size_t>(player_index)];
}

UserProfile* gubsy_lobby_user_profile(EngineState& engine, int player_index) {
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    return player ? find_user_profile(engine, player->user_profile_id) : nullptr;
}

BindsProfile* gubsy_lobby_binds_profile(EngineState& engine, int player_index) {
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    return player ? find_binds_profile(engine, player->binds_profile_id) : nullptr;
}

InputSettingsProfile* gubsy_lobby_input_settings_profile(EngineState& engine, int player_index) {
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    return player ? find_input_settings_profile(engine, player->input_settings_profile_id)
                  : nullptr;
}

bool gubsy_lobby_set_user_profile(EngineState& engine, int player_index, int profile_id) {
    gubsy_lobby_ensure_ready(engine);
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    UserProfile* profile = find_user_profile(engine, profile_id);
    if (!player || !profile)
        return false;

    player->user_profile_id = profile->id;
    player->binds_profile_id = find_binds_profile(engine, profile->last_binds_profile_id)
                                   ? profile->last_binds_profile_id
                                   : ensure_default_binds_profile(engine).id;
    player->input_settings_profile_id =
        find_input_settings_profile(engine, profile->last_input_settings_profile_id)
            ? profile->last_input_settings_profile_id
            : ensure_default_input_settings_profile(engine).id;
    persist_user_profile_choice(*profile, *player);
    sync_engine_players_from_lobby(engine);
    return true;
}

bool gubsy_lobby_set_binds_profile(EngineState& engine, int player_index, int profile_id) {
    gubsy_lobby_ensure_ready(engine);
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    BindsProfile* binds = find_binds_profile(engine, profile_id);
    if (!player || !binds)
        return false;
    player->binds_profile_id = binds->id;
    if (UserProfile* user = find_user_profile(engine, player->user_profile_id))
        persist_user_profile_choice(*user, *player);
    sync_engine_players_from_lobby(engine);
    return true;
}

bool gubsy_lobby_set_input_settings_profile(EngineState& engine, int player_index, int profile_id) {
    gubsy_lobby_ensure_ready(engine);
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    InputSettingsProfile* input = find_input_settings_profile(engine, profile_id);
    if (!player || !input)
        return false;
    player->input_settings_profile_id = input->id;
    if (UserProfile* user = find_user_profile(engine, player->user_profile_id))
        persist_user_profile_choice(*user, *player);
    sync_engine_players_from_lobby(engine);
    return true;
}

void gubsy_lobby_toggle_device(EngineState& engine, int player_index,
                               GubsyLobbyDeviceAssignment device) {
    gubsy_lobby_ensure_ready(engine);
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    if (!player)
        return;
    auto it = std::find_if(
        player->devices.begin(), player->devices.end(),
        [device](GubsyLobbyDeviceAssignment item) { return same_device(item, device); });
    if (it != player->devices.end())
        player->devices.erase(it);
    else
        player->devices.push_back(device);
    sync_engine_players_from_lobby(engine);
}

bool gubsy_lobby_player_has_device(const EngineState& engine, int player_index,
                                   GubsyLobbyDeviceAssignment device) {
    const GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    if (!player)
        return false;
    return std::any_of(
        player->devices.begin(), player->devices.end(),
        [device](GubsyLobbyDeviceAssignment item) { return same_device(item, device); });
}

void gubsy_lobby_assign_gamepad_to_primary_player(EngineState& engine, int device_id) {
    gubsy_lobby_ensure_ready(engine);
    if (engine.lobby.local_players.empty())
        return;

    GubsyLobbyPlayer& player = engine.lobby.local_players.front();
    auto has_gamepad = [](GubsyLobbyDeviceAssignment device) {
        return device.type == InputSourceType::Gamepad;
    };
    if (std::any_of(player.devices.begin(), player.devices.end(), has_gamepad))
        return;

    player.devices.push_back(GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, device_id});
    sync_engine_players_from_lobby(engine);
}

void gubsy_lobby_remove_gamepad_device_assignments(EngineState& engine, int device_id) {
    bool changed = false;
    for (GubsyLobbyPlayer& player : engine.lobby.local_players) {
        auto old_size = player.devices.size();
        player.devices.erase(std::remove_if(player.devices.begin(), player.devices.end(),
                                            [device_id](GubsyLobbyDeviceAssignment device) {
                                                return device.type == InputSourceType::Gamepad &&
                                                       device.device_id == device_id;
                                            }),
                             player.devices.end());
        changed = changed || player.devices.size() != old_size;
    }
    if (changed)
        sync_engine_players_from_lobby(engine);
}

bool gubsy_lobby_validate_start(EngineState& engine, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    if (engine.lobby.local_players.empty()) {
        message = "At least one local player is required";
        return false;
    }
    for (int i = 0; i < static_cast<int>(engine.lobby.local_players.size()); ++i) {
        GubsyLobbyPlayer& player = engine.lobby.local_players[static_cast<std::size_t>(i)];
        if (!find_user_profile(engine, player.user_profile_id)) {
            message = "Player " + std::to_string(i + 1) + " needs a user profile";
            return false;
        }
        if (!find_binds_profile(engine, player.binds_profile_id))
            add_alert(engine, "Player " + std::to_string(i + 1) + " is missing a binds profile");
        if (!find_input_settings_profile(engine, player.input_settings_profile_id)) {
            add_alert(engine,
                      "Player " + std::to_string(i + 1) + " is missing an input settings profile");
        }
    }
    GubsyLobbyConfigProvider& provider = engine.lobby_config_provider;
    if (provider.ensure_defaults)
        provider.ensure_defaults(provider.user_data, engine.lobby);
    if (provider.validate && !provider.validate(provider.user_data, engine.lobby, message))
        return false;
    message = "Ready";
    return true;
}

std::string gubsy_lobby_player_label(const EngineState& engine, int player_index) {
    const GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    const UserProfile* profile =
        player ? find_user_profile(engine, player->user_profile_id) : nullptr;
    std::string label = "Player " + std::to_string(player_index + 1);
    if (profile)
        label += ": " + profile->name;
    return label;
}

std::string gubsy_lobby_device_label(GubsyLobbyDeviceAssignment device) {
    if (device.type == InputSourceType::Keyboard)
        return "Keyboard";
    if (device.type == InputSourceType::Mouse)
        return "Mouse";
    return "Gamepad " + std::to_string(device.device_id);
}

GubsyLobbyDeviceAssignment gubsy_lobby_device_from_input_source(const InputSource& source) {
    GubsyLobbyDeviceAssignment device;
    device.type = source.type;
    device.device_id = source.device_id.id;
    return device;
}
