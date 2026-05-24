#include "gubsy/runtime.hpp"
#include "src/gubsy_runtime_internal.hpp"
#include "src/lobby_state.hpp"

#include <SDL.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {

constexpr int kSmokeAction = 7001;
constexpr int kSmokeProfileId = 9001;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

BindsProfile make_smoke_profile() {
    BindsProfile profile;
    profile.id = kSmokeProfileId;
    profile.name = "Smoke";
    bind_button(profile, GubsyButton::KB_A, kSmokeAction);
    bind_button(profile, GubsyButton::GP_A, kSmokeAction);
    return profile;
}

void assign_smoke_player(EngineState& engine, GubsyLobbyDeviceAssignment device) {
    gubsy_lobby_ensure_ready(engine);
    GubsyLobbyPlayer* player = gubsy_lobby_player(engine, 0);
    require(player != nullptr, "missing default lobby player");
    player->binds_profile_id = kSmokeProfileId;
    player->devices.clear();
    player->devices.push_back(device);
}

} // namespace

int main() {
    try {
        GubsyRuntime runtime;
        GubsyAppConfig config;
        config.enable_mods = false;
        config.project_root = std::filesystem::current_path().string();
        config.data_root =
            (std::filesystem::current_path() / "build" / "lobby_input_smoke_data").string();
        require(init_gubsy_runtime(runtime, config), "failed to init runtime");
        require(gubsy_replace_binds_profile(runtime, make_smoke_profile()),
                "failed to install smoke binds profile");

        EngineState& engine = gubsy_runtime_engine(runtime);
        assign_smoke_player(engine, GubsyLobbyDeviceAssignment{InputSourceType::Keyboard, 0});
        engine.device_state.keyboard[SDL_SCANCODE_A] = 1;
        require(gubsy_lobby_player_action_down(runtime, 0, kSmokeAction),
                "keyboard-assigned player did not see keyboard action");

        assign_smoke_player(engine, GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, 42});
        require(!gubsy_lobby_player_action_down(runtime, 0, kSmokeAction),
                "gamepad-assigned player saw keyboard action");

        DeviceState::ControllerState controller;
        controller.device_id = 42;
        controller.buttons[SDL_CONTROLLER_BUTTON_A] = 1;
        engine.device_state.controllers.push_back(controller);
        require(gubsy_lobby_player_action_down(runtime, 0, kSmokeAction),
                "gamepad-assigned player did not see matching gamepad action");

        engine.device_state.controllers.front().device_id = 43;
        require(!gubsy_lobby_player_action_down(runtime, 0, kSmokeAction),
                "gamepad-assigned player saw different gamepad device");

        cleanup_gubsy_runtime(runtime);
        std::puts("[lobby_input_smoke] ok");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lobby_input_smoke] %s\n", e.what());
        return 1;
    }
}
