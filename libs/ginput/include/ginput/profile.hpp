#pragma once

#include "ginput/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace ginput {

struct ButtonBind {
    EncodedControl device_button = 0;
    ActionId action = -1;
};

struct Axis1DBind {
    EncodedControl device_axis = 0;
    Axis1DId axis_1d = -1;
    float scale = 1.0f;
    float deadzone = 0.0f;
};

struct Axis2DBind {
    EncodedControl device_stick = 0;
    Axis2DId axis_2d = -1;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float deadzone = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct ProfileLookup {
    std::unordered_map<EncodedControl, std::vector<ActionId>> button_actions;
    std::unordered_map<EncodedControl, std::vector<Axis1DBind>> axes_1d;
    std::unordered_map<EncodedControl, std::vector<Axis2DBind>> axes_2d;
    std::unordered_map<ActionId, std::vector<ButtonBind>> action_button_binds;
    std::unordered_map<Axis1DId, std::vector<Axis1DBind>> axis_1d_binds;
    std::unordered_map<Axis2DId, std::vector<Axis2DBind>> axis_2d_binds;
};

class InputProfile {
  public:
    int id = -1;
    std::string name;

    const std::vector<ButtonBind>& button_binds() const;
    const std::vector<Axis1DBind>& axis_1d_binds() const;
    const std::vector<Axis2DBind>& axis_2d_binds() const;

  private:
    friend bool add_button_bind(InputProfile& profile, ButtonBind bind);
    friend bool add_axis_1d_bind(InputProfile& profile, Axis1DBind bind);
    friend bool add_axis_2d_bind(InputProfile& profile, Axis2DBind bind);
    friend bool remove_button_bind(InputProfile& profile, ButtonBind bind);
    friend bool remove_axis_1d_bind(InputProfile& profile, Axis1DBind bind);
    friend bool remove_axis_2d_bind(InputProfile& profile, Axis2DBind bind);
    friend const std::vector<ActionId>& actions_for_button(const InputProfile& profile,
                                                           EncodedControl device_button);
    friend const std::vector<Axis1DBind>& axes_for_1d(const InputProfile& profile,
                                                      EncodedControl device_axis);
    friend const std::vector<Axis2DBind>& axes_for_2d(const InputProfile& profile,
                                                      EncodedControl device_stick);
    friend const std::vector<ButtonBind>& button_binds_for_action(const InputProfile& profile,
                                                                  ActionId action);
    friend const std::vector<Axis1DBind>& binds_for_axis_1d(const InputProfile& profile,
                                                            Axis1DId axis_1d);
    friend const std::vector<Axis2DBind>& binds_for_axis_2d(const InputProfile& profile,
                                                            Axis2DId axis_2d);
    friend bool add_profile(std::vector<InputProfile>& profiles, InputProfile profile);
    friend bool replace_profile(std::vector<InputProfile>& profiles, InputProfile profile);

    void rebuild_lookup();

    std::vector<ButtonBind> button_bind_records;
    std::vector<Axis1DBind> axis_1d_bind_records;
    std::vector<Axis2DBind> axis_2d_bind_records;
    ProfileLookup lookup_index;
};

bool same_bind(const ButtonBind& a, const ButtonBind& b);
bool same_bind(const Axis1DBind& a, const Axis1DBind& b);
bool same_bind(const Axis2DBind& a, const Axis2DBind& b);

bool add_button_bind(InputProfile& profile, ButtonBind bind);
bool add_axis_1d_bind(InputProfile& profile, Axis1DBind bind);
bool add_axis_2d_bind(InputProfile& profile, Axis2DBind bind);

bool remove_button_bind(InputProfile& profile, ButtonBind bind);
bool remove_axis_1d_bind(InputProfile& profile, Axis1DBind bind);
bool remove_axis_2d_bind(InputProfile& profile, Axis2DBind bind);

InputProfile* find_profile(std::vector<InputProfile>& profiles, int id);
const InputProfile* find_profile(const std::vector<InputProfile>& profiles, int id);
InputProfile* find_profile_by_name(std::vector<InputProfile>& profiles, const std::string& name);
const InputProfile* find_profile_by_name(const std::vector<InputProfile>& profiles,
                                         const std::string& name);

bool add_profile(std::vector<InputProfile>& profiles, InputProfile profile);
bool replace_profile(std::vector<InputProfile>& profiles, InputProfile profile);

const std::vector<ActionId>& actions_for_button(const InputProfile& profile,
                                                EncodedControl device_button);
const std::vector<Axis1DBind>& axes_for_1d(const InputProfile& profile, EncodedControl device_axis);
const std::vector<Axis2DBind>& axes_for_2d(const InputProfile& profile,
                                           EncodedControl device_stick);
const std::vector<ButtonBind>& button_binds_for_action(const InputProfile& profile,
                                                       ActionId action);
const std::vector<Axis1DBind>& binds_for_axis_1d(const InputProfile& profile, Axis1DId axis_1d);
const std::vector<Axis2DBind>& binds_for_axis_2d(const InputProfile& profile, Axis2DId axis_2d);

float apply_axis_transform(float value, float scale, float deadzone);
Vec2 apply_stick_transform(Vec2 value, float scale_x, float scale_y, float deadzone);

} // namespace ginput
