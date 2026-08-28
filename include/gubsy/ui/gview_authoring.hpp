#pragma once

#include <gview/imgui_editor.hpp>
#include <gview/runtime.hpp>

namespace gubsy::ui {

void draw_view_authoring(gview::AuthoringSession& session, gview::AuthoringUiState& state,
                         gview::AuthoringHooks& hooks, const gview::Runtime& runtime);

} // namespace gubsy::ui
