#pragma once

#include "gubsy/menu/system.hpp"

#include <filesystem>
#include <functional>
#include <gview/gview.hpp>

union SDL_Event;

namespace gubsy::ui {

struct ViewModel {
    std::function<gview::Value(std::string_view)> read;
    std::function<void(std::string_view, const gview::Value&)> write;
    std::function<bool(std::string_view)> condition;
    std::function<void(std::string_view, gview::NodeIndex)> event;
    std::uint64_t revision = 0;
};

enum class AssetDomain { Game, Engine, Data, RuntimeMods };

gview::Host make_view_host(ViewModel& model);
gview::InputFrame make_view_input(const MenuInputState& input, std::string text = {});
void append_view_pointer(gview::InputFrame& input, const SDL_Event& event);
std::filesystem::path resolve_view_asset(AssetDomain domain, const std::filesystem::path& relative);

} // namespace gubsy::ui
