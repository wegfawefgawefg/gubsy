#include "gubsy/ui/view_runtime.hpp"

namespace gubsy::ui {

// Compiles authored data once and retains diagnostics for host-side tooling.
bool ViewRuntime::activate(const gview::View& source) {
    gview::CompileResult compiled = gview::compile_view(source);
    diagnostics_ = std::move(compiled.diagnostics);
    if (!compiled.ok) return false;
    runtime_.reset(std::move(compiled.view));
    return true;
}

void ViewRuntime::process_event(const SDL_Event& event) { append_view_pointer(pointer_, event); }

// Merges mapped actions with accumulated native pointer/text state and invokes
// the same typed model and event callbacks games register with Gubsy.
void ViewRuntime::frame(const MenuInputState& input, std::string text,
                        const glayout::ResolveInput& resolve, ViewModel& model) {
    gview::InputFrame frame_input = make_view_input(input, std::move(text));
    frame_input.pointer = pointer_.pointer;
    frame_input.text += pointer_.text;
    pointer_ = {};
    gview::Host host = make_view_host(model);
    runtime_.frame(resolve, frame_input, host);
}

gview::Runtime& ViewRuntime::runtime() { return runtime_; }
const gview::Runtime& ViewRuntime::runtime() const { return runtime_; }
const std::vector<glayout::Diagnostic>& ViewRuntime::diagnostics() const { return diagnostics_; }

} // namespace gubsy::ui
