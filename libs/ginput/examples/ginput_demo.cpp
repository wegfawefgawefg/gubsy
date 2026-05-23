#include "ginput/ginput.hpp"

#include <iostream>

int main() {
    enum Action {
        MenuUp = 0,
        MoveUp = 1,
        Use = 2,
    };

    ginput::Schema schema;
    schema.add_action(MenuUp, "Menu Up", "Menu");
    schema.add_action(MoveUp, "Move Up", "Gameplay");
    schema.add_action(Use, "Use", "Gameplay");
    schema.add_axis_2d(0, "Aim", "Gameplay");

    ginput::InputProfile profile;
    profile.id = 100;
    profile.name = "Keyboard Mouse";
    ginput::add_button_bind(profile, ginput::ButtonBind{26, MenuUp});
    ginput::add_button_bind(profile, ginput::ButtonBind{26, MoveUp});
    ginput::add_button_bind(profile, ginput::ButtonBind{8, Use});
    ginput::add_axis_2d_bind(profile, ginput::Axis2DBind{4000, 0});

    const ginput::ReconcileReport report = ginput::reconcile_profile(profile, schema);

    std::cout << "ginput demo api version " << ginput::version_major() << "\n";
    std::cout << "profile " << profile.id << " " << profile.name << "\n";
    std::cout << "button binds: " << profile.button_binds().size() << "\n";
    std::cout << "2d binds: " << profile.axis_2d_binds().size() << "\n";
    std::cout << "reconcile changes: " << report.changes.size() << "\n";
    return 0;
}
