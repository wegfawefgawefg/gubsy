#include "ginput/ginput.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void require_near(float actual, float expected, const char* message) {
    if (std::fabs(actual - expected) > 0.0001f) {
        std::cerr << "FAILED: " << message << " actual=" << actual << " expected=" << expected
                  << "\n";
        std::exit(1);
    }
}

void test_encoded_controls() {
    ginput::DeviceButton button{};
    button.kind = ginput::DeviceKind::Gamepad;
    button.device_id = 2;
    button.code = 12;

    const int encoded = ginput::encode_button(button);
    ginput::DeviceButton decoded{};
    require(ginput::decode_button(encoded, decoded), "decode extended button");
    require(decoded.kind == ginput::DeviceKind::Gamepad, "button kind round trip");
    require(decoded.device_id == 2, "button device round trip");
    require(decoded.code == 12, "button code round trip");

    ginput::DeviceAxis2D stick{};
    stick.kind = ginput::DeviceKind::Gamepad;
    stick.device_id = ginput::any_device_id;
    stick.x_code = 4;
    stick.y_code = 5;

    const int stick_encoded = ginput::encode_axis_2d(stick);
    ginput::DeviceAxis2D stick_decoded{};
    require(ginput::decode_axis_2d(stick_encoded, stick_decoded), "decode extended stick");
    require(stick_decoded.device_id == ginput::any_device_id, "any-device round trip");
    require(stick_decoded.x_code == 4, "stick x round trip");
    require(stick_decoded.y_code == 5, "stick y round trip");
}

void test_schema() {
    ginput::Schema schema;
    require(schema.add_action(0, "Jump", "Gameplay"), "add action");
    require(!schema.add_action(0, "Jump Again", "Gameplay"), "reject duplicate action id");
    require(schema.add_axis_1d(0, "Throttle", "Gameplay"), "add 1d axis");
    require(schema.add_axis_2d(0, "Aim", "Gameplay"), "add 2d axis");
    require(schema.has_action(0), "has action");
    require(!schema.has_action(99), "missing action");
    require(schema.find_action(0)->label == "Jump", "find action label");
}

void test_button_state() {
    ginput::ButtonState idle = ginput::make_button_state(false, false);
    require(!idle.down, "idle down");
    require(!idle.pressed, "idle pressed");
    require(!idle.released, "idle released");

    ginput::ButtonState pressed = ginput::make_button_state(true, false);
    require(pressed.down, "pressed down");
    require(pressed.pressed, "pressed edge");
    require(!pressed.released, "pressed released");

    ginput::ButtonState held = ginput::make_button_state(true, true);
    require(held.down, "held down");
    require(!held.pressed, "held pressed");
    require(!held.released, "held released");

    ginput::ButtonState released = ginput::make_button_state(false, true);
    require(!released.down, "released down");
    require(!released.pressed, "released pressed");
    require(released.released, "released edge");
}

void test_profile_helpers() {
    ginput::InputProfile profile;
    profile.id = 7;
    profile.name = "Default";

    require(ginput::add_button_bind(profile, ginput::ButtonBind{100, 1}), "add button bind");
    require(!ginput::add_button_bind(profile, ginput::ButtonBind{100, 1}),
            "reject exact duplicate");
    require(ginput::add_button_bind(profile, ginput::ButtonBind{100, 2}),
            "allow same button to different action");
    require(profile.button_binds().size() == 2, "button bind count");
    require(ginput::actions_for_button(profile, 100).size() == 2, "button lookup count");
    require(ginput::actions_for_button(profile, 100)[0] == 1, "button lookup first action");
    require(ginput::actions_for_button(profile, 100)[1] == 2, "button lookup second action");
    require(ginput::button_binds_for_action(profile, 1).size() == 1, "action lookup count");
    require(ginput::button_binds_for_action(profile, 1)[0].device_button == 100,
            "action lookup source");
    require(ginput::remove_button_bind(profile, ginput::ButtonBind{100, 1}), "remove bind");
    require(profile.button_binds().size() == 1, "button bind remove count");
    require(ginput::actions_for_button(profile, 100).size() == 1, "button lookup after remove");
    require(ginput::button_binds_for_action(profile, 1).empty(), "action lookup after remove");

    require(ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{200, 3, -1.0f, 0.05f}),
            "add 1d bind");
    require(ginput::axes_for_1d(profile, 200).size() == 1, "1d lookup count");
    require(ginput::binds_for_axis_1d(profile, 3).size() == 1, "1d target lookup count");
    require_near(ginput::axes_for_1d(profile, 200)[0].scale, -1.0f, "1d lookup scale");

    std::vector<ginput::InputProfile> profiles;
    require(ginput::add_profile(profiles, profile), "add profile");
    require(!ginput::add_profile(profiles, profile), "reject duplicate profile id");
    require(ginput::find_profile(profiles, 7) != nullptr, "find profile by id");
    require(ginput::find_profile_by_name(profiles, "Default") != nullptr, "find profile by name");

    ginput::InputProfile replacement;
    replacement.id = 7;
    replacement.name = "Updated";
    require(ginput::replace_profile(profiles, replacement), "replace profile");
    require(ginput::find_profile(profiles, 7)->name == "Updated", "replace profile name");
}

void test_transforms() {
    require_near(ginput::apply_axis_transform(0.04f, 1.0f, 0.05f), 0.0f, "axis deadzone");
    require_near(ginput::apply_axis_transform(0.5f, -1.0f, 0.05f), -0.5f, "axis scale");
    require_near(ginput::apply_axis_transform(2.0f, 1.0f, 0.0f), 1.0f, "axis clamp");

    const ginput::Vec2 stick =
        ginput::apply_stick_transform(ginput::Vec2{0.25f, 0.5f}, 1.0f, -1.0f, 0.1f);
    require_near(stick.x, 0.25f, "stick x scale");
    require_near(stick.y, -0.5f, "stick y scale");
}

void test_runtime_helpers() {
    ginput::FrameState frame;
    frame.resize_actions(4);
    frame.resize_axes_1d(2);
    frame.resize_axes_2d(1);

    frame.begin_frame();
    frame.set_down(1, true);
    frame.set_axis_1d(0, 0.25f);
    frame.set_axis_2d(0, ginput::Vec2{0.5f, -0.25f});
    require(frame.down(1), "frame down");
    require(frame.pressed(1), "frame pressed");
    require(!frame.released(1), "frame released false");
    require_near(frame.axis_1d(0), 0.25f, "frame axis 1d");
    require_near(frame.axis_2d(0).x, 0.5f, "frame axis 2d x");
    require_near(frame.axis_2d(0).y, -0.25f, "frame axis 2d y");

    frame.begin_frame();
    require(!frame.down(1), "frame clears current");
    require(frame.released(1), "frame released");
    require_near(frame.axis_1d_delta(0), -0.25f, "frame axis 1d delta");

    frame.begin_frame(ginput::FrameReset::KeepCurrent);
    frame.set_down(2, true);
    frame.begin_frame(ginput::FrameReset::KeepCurrent);
    require(frame.down(2), "frame keep current");
    require(!frame.pressed(2), "frame kept held");

    frame.merge_axis_1d(0, 0.2f);
    frame.merge_axis_1d(0, -0.7f);
    require_near(frame.axis_1d(0), -0.7f, "frame merge 1d larger magnitude");
    frame.merge_axis_2d(0, ginput::Vec2{0.1f, 0.1f});
    frame.merge_axis_2d(0, ginput::Vec2{0.9f, 0.0f});
    require_near(frame.axis_2d(0).x, 0.9f, "frame merge 2d larger magnitude");

    ginput::RepeatState repeat;
    ginput::RepeatConfig config{0.3f, 0.1f};
    ginput::RepeatResult first = ginput::update_repeat(true, repeat, 0.0f, config);
    require(first.trigger && first.first_press && !first.repeat, "repeat first press");
    ginput::RepeatResult waiting = ginput::update_repeat(true, repeat, 0.1f, config);
    require(!waiting.trigger, "repeat waiting");
    ginput::RepeatResult repeated = ginput::update_repeat(true, repeat, 0.21f, config);
    require(repeated.trigger && repeated.repeat, "repeat trigger");
    ginput::RepeatResult released = ginput::update_repeat(false, repeat, 0.0f, config);
    require(!released.trigger, "repeat release");
    ginput::RepeatResult first_again = ginput::update_repeat(true, repeat, 0.0f, config);
    require(first_again.first_press, "repeat resets");

    ginput::MouseWheelAccumulator wheel;
    wheel.add(1.0f);
    wheel.add(-0.25f);
    require_near(wheel.value(), 0.75f, "wheel value");
    require_near(wheel.consume(), 0.75f, "wheel consume");
    require_near(wheel.value(), 0.0f, "wheel consumed reset");

    require_near(ginput::choose_larger_magnitude(0.2f, -0.5f), -0.5f, "choose 1d");
    const ginput::Vec2 chosen =
        ginput::choose_larger_magnitude(ginput::Vec2{0.2f, 0.2f}, ginput::Vec2{0.7f, 0.0f});
    require_near(chosen.x, 0.7f, "choose 2d x");
}

void test_reconcile() {
    ginput::Schema schema;
    schema.add_action(1, "Use");
    schema.add_action(2, "Jump");
    schema.add_axis_1d(3, "Throttle");
    schema.add_axis_2d(4, "Aim");

    ginput::InputProfile profile;
    profile.id = 9;
    ginput::add_button_bind(profile, ginput::ButtonBind{100, 1});
    ginput::add_button_bind(profile, ginput::ButtonBind{101, 99});
    ginput::add_button_bind(profile, ginput::ButtonBind{100, 2});
    ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{200, 3});
    ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{201, 99});
    ginput::add_axis_2d_bind(profile, ginput::Axis2DBind{300, 4});

    const ginput::ReconcileReport report = ginput::reconcile_profile(profile, schema);
    require(report.changed(), "reconcile changed");
    require(report.changes.size() == 2, "reconcile change count");
    require(profile.button_binds().size() == 2, "reconcile keeps valid distinct buttons");
    require(profile.axis_1d_binds().size() == 1, "reconcile removes invalid 1d axis");
    require(profile.axis_2d_binds().size() == 1, "reconcile keeps valid 2d axis");
    require(ginput::actions_for_button(profile, 101).empty(), "reconcile updates button lookup");
}

void test_profile_io() {
    ginput::InputProfile profile;
    profile.id = 12;
    profile.name = "Keyboard";
    ginput::add_button_bind(profile, ginput::ButtonBind{26, 0});
    ginput::add_button_bind(profile, ginput::ButtonBind{26, 1});
    ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{1000, 2, -1.0f, 0.05f});
    ginput::add_axis_2d_bind(profile, ginput::Axis2DBind{2000, 3, 1.0f, -1.0f, 0.12f});

    std::string text;
    require(ginput::save_profiles_string({profile}, text), "save profiles string");
    require(text.find("(input_profiles") != std::string::npos, "save root");
    require(text.find("(scale -1") != std::string::npos, "save 1d scale");
    require(text.find("(scale_y -1") != std::string::npos, "save 2d scale");

    ginput::LoadProfilesResult loaded = ginput::load_profiles_string(text);
    require(loaded.ok, "load saved profiles");
    require(loaded.profiles.size() == 1, "load profile count");
    require(loaded.profiles[0].name == "Keyboard", "load profile name");
    require(loaded.profiles[0].button_binds().size() == 2, "load button bind count");
    require(ginput::actions_for_button(loaded.profiles[0], 26).size() == 2,
            "load rebuilds button lookup");
    require(loaded.profiles[0].axis_1d_binds().size() == 1, "load 1d bind count");
    require(ginput::axes_for_1d(loaded.profiles[0], 1000).size() == 1, "load rebuilds 1d lookup");
    require(loaded.profiles[0].axis_2d_binds().size() == 1, "load 2d bind count");
    require(ginput::axes_for_2d(loaded.profiles[0], 2000).size() == 1, "load rebuilds 2d lookup");
    require_near(loaded.profiles[0].axis_1d_binds()[0].scale, -1.0f, "load 1d scale");
    require_near(loaded.profiles[0].axis_2d_binds()[0].scale_y, -1.0f, "load 2d scale");

    ginput::Schema schema;
    schema.add_action(0, "Menu Up");
    schema.add_action(1, "Move Up");
    schema.add_axis_1d(2, "Throttle");
    schema.add_axis_2d(3, "Aim");
    ginput::LoadProfilesResult reconciled = ginput::load_profiles_string(text, schema);
    require(reconciled.ok, "load with schema");
    require(!reconciled.reconcile_report.changed(), "load with schema no changes");
}

} // namespace

int main() {
    require(ginput::version_major() == 0, "version placeholder");
    test_encoded_controls();
    test_schema();
    test_button_state();
    test_profile_helpers();
    test_transforms();
    test_runtime_helpers();
    test_reconcile();
    test_profile_io();
    std::cout << "ginput_tests passed\n";
    return 0;
}
