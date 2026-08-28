#include "gubsy/runtime.hpp"
#include "gubsy/ui/view_runtime.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unordered_map>

namespace {

void require(bool value, const char* message) {
    if (value) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

gview::View smoke_view() {
    gview::View view;
    view.id = "gubsy-hosted";
    view.layout.id = view.id;
    view.layout.root.id = "root";
    view.layout.root.container = glayout::ContainerKind::Column;
    view.layout.root.padding = {20.0f, 20.0f, 20.0f, 20.0f};
    view.layout.root.gap = 8.0f;

    glayout::GraphNode button;
    button.id = "apply";
    button.size.width = {glayout::LengthKind::Pixels, 220.0f};
    button.size.height = {glayout::LengthKind::Pixels, 52.0f};
    view.layout.root.children.push_back(button);
    gview::NodeSpec control;
    control.layout_id = "apply";
    control.content = gview::ContentKind::Text;
    control.control = gview::ControlKind::Button;
    control.text = "Apply";
    control.action = "settings:apply";
    control.focusable = true;
    view.nodes.push_back(control);
    return view;
}

} // namespace

// Exercises the actual Gubsy runtime boundary, semantic input, typed model,
// event dispatch, asset roots, and renderer-neutral GView frame lifecycle.
int main() {
    GubsyRuntime engine;
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root =
        (std::filesystem::current_path() / "build" / "gview_hosted_smoke_data").string();
    require(init_gubsy_runtime(engine, config), "normal Gubsy runtime initializes");
    gubsy_update_device_state(engine);
    gubsy_update_runtime(engine, 1.0f / 60.0f);

    gubsy::ui::ViewRuntime view;
    require(view.activate(smoke_view()), "Gubsy-hosted view activates");
    bool action_called = false;
    std::unordered_map<std::string, gview::Value> values;
    gubsy::ui::ViewModel model;
    model.read = [&](std::string_view key) { return values[std::string(key)]; };
    model.write = [&](std::string_view key, const gview::Value& value) {
        values[std::string(key)] = value;
    };
    model.condition = [](std::string_view) { return true; };
    model.event = [&](std::string_view event, gview::NodeIndex) {
        action_called = event == "settings:apply";
    };

    MenuInputState input;
    input.select = true;
    view.frame(input, "", {{0.0f, 0.0f, 1280.0f, 720.0f}, {}, nullptr, nullptr}, model);
    require(action_called, "mapped confirm dispatches a normal Gubsy game event");
    require(!view.runtime().paint().empty(), "host produces renderer-neutral paint");
    require(view.runtime().focus() != gview::invalid_node, "host retains controller focus");

    action_called = false;
    SDL_Event press{};
    press.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    press.button.button = SDL_BUTTON_LEFT;
    press.button.x = 30.0f;
    press.button.y = 30.0f;
    SDL_Event release = press;
    release.type = SDL_EVENT_MOUSE_BUTTON_UP;
    gubsy_process_sdl_event(engine, press);
    gubsy_process_sdl_event(engine, release);
    view.process_event(press);
    view.process_event(release);
    view.frame({}, "", {{0.0f, 0.0f, 1280.0f, 720.0f}, {}, nullptr, nullptr}, model);
    require(action_called, "native pointer events share the Gubsy-hosted frame path");

    MenuInputState mapping;
    mapping.down = true;
    mapping.page_next = true;
    const gview::InputFrame mapped = gubsy::ui::make_view_input(mapping, "hello");
    require(mapped.navigation.size() == 2 && mapped.text == "hello",
            "existing semantic action and text mappings are retained");
    require(gubsy::ui::resolve_view_asset(gubsy::ui::AssetDomain::Data, "views/ui.sexp") ==
                std::filesystem::path(config.data_root) / "views/ui.sexp",
            "GView uses Gubsy's configured data asset domain");

    cleanup_gubsy_runtime(engine);
    return 0;
}
