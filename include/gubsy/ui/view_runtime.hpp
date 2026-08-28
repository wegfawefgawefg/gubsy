#pragma once

#include "gubsy/ui/gview.hpp"

namespace gubsy::ui {

// Owns one activated GView through Gubsy's normal semantic input, model/event,
// pointer, and resolution services.
class ViewRuntime {
  public:
    bool activate(const gview::View& source);
    void process_event(const SDL_Event& event);
    void frame(const MenuInputState& input, std::string text, const glayout::ResolveInput& resolve,
               ViewModel& model);

    gview::Runtime& runtime();
    const gview::Runtime& runtime() const;
    const std::vector<glayout::Diagnostic>& diagnostics() const;

  private:
    gview::Runtime runtime_;
    gview::InputFrame pointer_;
    std::vector<glayout::Diagnostic> diagnostics_;
};

} // namespace gubsy::ui
