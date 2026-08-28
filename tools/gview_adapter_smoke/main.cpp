#include "gubsy/ui/gview.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool value, const char* message) {
    if (value)
        return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

} // namespace

// Verifies that normal Gubsy semantic input reaches GView without remapping devices.
int main() {
    MenuInputState input;
    input.down = true;
    input.select = true;
    input.page_next = true;
    const gview::InputFrame frame = gubsy::ui::make_view_input(input, "hello");
    require(frame.navigation.size() == 3, "semantic actions are retained");
    require(frame.navigation[0] == gview::NavAction::Down, "direction is retained");
    require(frame.navigation[1] == gview::NavAction::Confirm, "confirm is retained");
    require(frame.navigation[2] == gview::NavAction::TabNext, "bumper is retained");
    require(frame.text == "hello", "text is retained");
    return 0;
}
