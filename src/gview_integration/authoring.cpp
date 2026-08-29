#include "gubsy/ui/gview_authoring.hpp"

namespace gubsy::ui {

// Hosts the standalone optional authoring suite inside Gubsy's existing ImGui frame.
void draw_view_authoring(gview::AuthoringSession& session, gview::AuthoringUiState& state,
                         gview::AuthoringHooks& hooks, const gview::Runtime& runtime) {
#if GUB_ENABLE_GVIEW_AUTHORING
    gview::draw_authoring_tools(session, state, hooks, runtime.view(), runtime.geometry(),
                                &runtime.paint());
#else
    (void)session;
    (void)state;
    (void)hooks;
    (void)runtime;
#endif
}

} // namespace gubsy::ui
