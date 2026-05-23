#include "ginput/profile.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ginput {
namespace {

bool same_float(float a, float b) {
    return std::fabs(a - b) <= 0.000001f;
}

template <typename T> const std::vector<T>& empty_vector() {
    static const std::vector<T> empty;
    return empty;
}

} // namespace

const std::vector<ButtonBind>& InputProfile::button_binds() const {
    return button_bind_records;
}

const std::vector<Axis1DBind>& InputProfile::axis_1d_binds() const {
    return axis_1d_bind_records;
}

const std::vector<Axis2DBind>& InputProfile::axis_2d_binds() const {
    return axis_2d_bind_records;
}

void InputProfile::rebuild_lookup() {
    lookup_index.button_actions.clear();
    lookup_index.axes_1d.clear();
    lookup_index.axes_2d.clear();
    lookup_index.action_button_binds.clear();
    lookup_index.axis_1d_binds.clear();
    lookup_index.axis_2d_binds.clear();

    for (const ButtonBind& bind : button_bind_records) {
        lookup_index.button_actions[bind.device_button].push_back(bind.action);
        lookup_index.action_button_binds[bind.action].push_back(bind);
    }
    for (const Axis1DBind& bind : axis_1d_bind_records) {
        lookup_index.axes_1d[bind.device_axis].push_back(bind);
        lookup_index.axis_1d_binds[bind.axis_1d].push_back(bind);
    }
    for (const Axis2DBind& bind : axis_2d_bind_records) {
        lookup_index.axes_2d[bind.device_stick].push_back(bind);
        lookup_index.axis_2d_binds[bind.axis_2d].push_back(bind);
    }
}

bool same_bind(const ButtonBind& a, const ButtonBind& b) {
    return a.device_button == b.device_button && a.action == b.action;
}

bool same_bind(const Axis1DBind& a, const Axis1DBind& b) {
    return a.device_axis == b.device_axis && a.axis_1d == b.axis_1d &&
           same_float(a.scale, b.scale) && same_float(a.deadzone, b.deadzone);
}

bool same_bind(const Axis2DBind& a, const Axis2DBind& b) {
    return a.device_stick == b.device_stick && a.axis_2d == b.axis_2d &&
           same_float(a.scale_x, b.scale_x) && same_float(a.scale_y, b.scale_y) &&
           same_float(a.deadzone, b.deadzone);
}

bool add_button_bind(InputProfile& profile, ButtonBind bind) {
    for (const ButtonBind& existing : profile.button_bind_records) {
        if (same_bind(existing, bind)) {
            return false;
        }
    }
    profile.button_bind_records.push_back(bind);
    profile.lookup_index.button_actions[bind.device_button].push_back(bind.action);
    profile.lookup_index.action_button_binds[bind.action].push_back(bind);
    return true;
}

bool add_axis_1d_bind(InputProfile& profile, Axis1DBind bind) {
    for (const Axis1DBind& existing : profile.axis_1d_bind_records) {
        if (same_bind(existing, bind)) {
            return false;
        }
    }
    profile.axis_1d_bind_records.push_back(bind);
    profile.lookup_index.axes_1d[bind.device_axis].push_back(bind);
    profile.lookup_index.axis_1d_binds[bind.axis_1d].push_back(bind);
    return true;
}

bool add_axis_2d_bind(InputProfile& profile, Axis2DBind bind) {
    for (const Axis2DBind& existing : profile.axis_2d_bind_records) {
        if (same_bind(existing, bind)) {
            return false;
        }
    }
    profile.axis_2d_bind_records.push_back(bind);
    profile.lookup_index.axes_2d[bind.device_stick].push_back(bind);
    profile.lookup_index.axis_2d_binds[bind.axis_2d].push_back(bind);
    return true;
}

bool remove_button_bind(InputProfile& profile, ButtonBind bind) {
    auto it = std::find_if(profile.button_bind_records.begin(), profile.button_bind_records.end(),
                           [&](const ButtonBind& existing) { return same_bind(existing, bind); });
    if (it == profile.button_bind_records.end()) {
        return false;
    }
    profile.button_bind_records.erase(it);
    profile.rebuild_lookup();
    return true;
}

bool remove_axis_1d_bind(InputProfile& profile, Axis1DBind bind) {
    auto it = std::find_if(profile.axis_1d_bind_records.begin(), profile.axis_1d_bind_records.end(),
                           [&](const Axis1DBind& existing) { return same_bind(existing, bind); });
    if (it == profile.axis_1d_bind_records.end()) {
        return false;
    }
    profile.axis_1d_bind_records.erase(it);
    profile.rebuild_lookup();
    return true;
}

bool remove_axis_2d_bind(InputProfile& profile, Axis2DBind bind) {
    auto it = std::find_if(profile.axis_2d_bind_records.begin(), profile.axis_2d_bind_records.end(),
                           [&](const Axis2DBind& existing) { return same_bind(existing, bind); });
    if (it == profile.axis_2d_bind_records.end()) {
        return false;
    }
    profile.axis_2d_bind_records.erase(it);
    profile.rebuild_lookup();
    return true;
}

InputProfile* find_profile(std::vector<InputProfile>& profiles, int id) {
    for (InputProfile& profile : profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

const InputProfile* find_profile(const std::vector<InputProfile>& profiles, int id) {
    for (const InputProfile& profile : profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

InputProfile* find_profile_by_name(std::vector<InputProfile>& profiles, const std::string& name) {
    for (InputProfile& profile : profiles) {
        if (profile.name == name) {
            return &profile;
        }
    }
    return nullptr;
}

const InputProfile* find_profile_by_name(const std::vector<InputProfile>& profiles,
                                         const std::string& name) {
    for (const InputProfile& profile : profiles) {
        if (profile.name == name) {
            return &profile;
        }
    }
    return nullptr;
}

bool add_profile(std::vector<InputProfile>& profiles, InputProfile profile) {
    if (profile.id <= 0 || profile.name.empty()) {
        return false;
    }
    if (find_profile(profiles, profile.id) != nullptr) {
        return false;
    }
    if (find_profile_by_name(profiles, profile.name) != nullptr) {
        return false;
    }
    profile.rebuild_lookup();
    profiles.push_back(std::move(profile));
    return true;
}

bool replace_profile(std::vector<InputProfile>& profiles, InputProfile profile) {
    if (profile.id <= 0 || profile.name.empty()) {
        return false;
    }
    for (const InputProfile& existing : profiles) {
        if (existing.id != profile.id && existing.name == profile.name) {
            return false;
        }
    }
    for (InputProfile& existing : profiles) {
        if (existing.id == profile.id) {
            profile.rebuild_lookup();
            existing = std::move(profile);
            return true;
        }
    }
    profile.rebuild_lookup();
    profiles.push_back(std::move(profile));
    return true;
}

const std::vector<ActionId>& actions_for_button(const InputProfile& profile,
                                                EncodedControl device_button) {
    auto it = profile.lookup_index.button_actions.find(device_button);
    if (it == profile.lookup_index.button_actions.end()) {
        return empty_vector<ActionId>();
    }
    return it->second;
}

const std::vector<Axis1DBind>& axes_for_1d(const InputProfile& profile,
                                           EncodedControl device_axis) {
    auto it = profile.lookup_index.axes_1d.find(device_axis);
    if (it == profile.lookup_index.axes_1d.end()) {
        return empty_vector<Axis1DBind>();
    }
    return it->second;
}

const std::vector<Axis2DBind>& axes_for_2d(const InputProfile& profile,
                                           EncodedControl device_stick) {
    auto it = profile.lookup_index.axes_2d.find(device_stick);
    if (it == profile.lookup_index.axes_2d.end()) {
        return empty_vector<Axis2DBind>();
    }
    return it->second;
}

const std::vector<ButtonBind>& button_binds_for_action(const InputProfile& profile,
                                                       ActionId action) {
    auto it = profile.lookup_index.action_button_binds.find(action);
    if (it == profile.lookup_index.action_button_binds.end()) {
        return empty_vector<ButtonBind>();
    }
    return it->second;
}

const std::vector<Axis1DBind>& binds_for_axis_1d(const InputProfile& profile, Axis1DId axis_1d) {
    auto it = profile.lookup_index.axis_1d_binds.find(axis_1d);
    if (it == profile.lookup_index.axis_1d_binds.end()) {
        return empty_vector<Axis1DBind>();
    }
    return it->second;
}

const std::vector<Axis2DBind>& binds_for_axis_2d(const InputProfile& profile, Axis2DId axis_2d) {
    auto it = profile.lookup_index.axis_2d_binds.find(axis_2d);
    if (it == profile.lookup_index.axis_2d_binds.end()) {
        return empty_vector<Axis2DBind>();
    }
    return it->second;
}

float apply_axis_transform(float value, float scale, float deadzone) {
    const float scaled = value * scale;
    const float magnitude = std::fabs(scaled);
    if (magnitude <= std::max(deadzone, 0.0f)) {
        return 0.0f;
    }
    return std::clamp(scaled, -1.0f, 1.0f);
}

Vec2 apply_stick_transform(Vec2 value, float scale_x, float scale_y, float deadzone) {
    Vec2 scaled{value.x * scale_x, value.y * scale_y};
    const float magnitude = std::sqrt((scaled.x * scaled.x) + (scaled.y * scaled.y));
    if (magnitude <= std::max(deadzone, 0.0f)) {
        return Vec2{};
    }
    scaled.x = std::clamp(scaled.x, -1.0f, 1.0f);
    scaled.y = std::clamp(scaled.y, -1.0f, 1.0f);
    return scaled;
}

} // namespace ginput
