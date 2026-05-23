#include "src/binds_profiles.hpp"

#include "src/engine_state.hpp"
#include "src/project_paths.hpp"

#include <algorithm>
#include <filesystem>
#include <random>
#include <unordered_set>

namespace {

BindsSchema g_schema;

std::filesystem::path binds_profiles_path() {
    return data_path("binds_profiles/default.lisp");
}

bool same_button_action(const ginput::ButtonBind& bind, int action_id) {
    return bind.action == action_id;
}

bool same_axis_1d_action(const ginput::Axis1DBind& bind, int action_id) {
    return bind.axis_1d == action_id;
}

bool same_axis_2d_action(const ginput::Axis2DBind& bind, int action_id) {
    return bind.axis_2d == action_id;
}

} // namespace

std::vector<BindsProfile> load_all_binds_profiles() {
    ginput::LoadProfilesResult result =
        ginput::load_profiles_file(binds_profiles_path(), g_schema, "binds_profiles");
    if (!result.ok)
        return {};
    if (result.reconcile_report.changed())
        (void)ginput::save_profiles_file(binds_profiles_path(), result.profiles, "binds_profiles");
    return std::move(result.profiles);
}

BindsProfile load_binds_profile(int profile_id) {
    auto profiles = load_all_binds_profiles();
    if (BindsProfile* profile = ginput::find_profile(profiles, profile_id))
        return *profile;

    BindsProfile empty;
    empty.id = -1;
    return empty;
}

bool save_binds_profile(const BindsProfile& profile) {
    if (profile.id <= 0 || profile.name.empty())
        return false;

    auto profiles = load_all_binds_profiles();
    if (!ginput::replace_profile(profiles, profile))
        return false;

    return ginput::save_profiles_file(binds_profiles_path(), profiles, "binds_profiles");
}

bool load_binds_profiles_pool(EngineState& engine) {
    engine.binds_profiles = load_all_binds_profiles();
    return true;
}

int generate_binds_profile_id() {
    std::unordered_set<int> used;
    auto profiles = load_all_binds_profiles();
    used.reserve(profiles.size());
    for (const auto& profile : profiles)
        used.insert(profile.id);

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.count(candidate))
            return candidate;
    }
    return dist(rng);
}

BindsProfile create_default_binds_profile() {
    BindsProfile profile;
    profile.id = generate_binds_profile_id();
    profile.name = "DefaultBinds";
    save_binds_profile(profile);
    return profile;
}

void bind_button(BindsProfile& profile, int device_button, int gubsy_action) {
    (void)ginput::add_button_bind(profile, ginput::ButtonBind{device_button, gubsy_action});
}

void bind_button(BindsProfile& profile, GubsyButton device_button, int gubsy_action) {
    bind_button(profile, static_cast<int>(device_button), gubsy_action);
}

void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog) {
    (void)ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{device_axis, gubsy_1d_analog});
}

void bind_1d_analog(BindsProfile& profile, Gubsy1DAnalog device_axis, int gubsy_1d_analog) {
    bind_1d_analog(profile, static_cast<int>(device_axis), gubsy_1d_analog);
}

void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog) {
    (void)ginput::add_axis_2d_bind(profile, ginput::Axis2DBind{device_stick, gubsy_2d_analog});
}

void bind_2d_analog(BindsProfile& profile, Gubsy2DAnalog device_stick, int gubsy_2d_analog) {
    bind_2d_analog(profile, static_cast<int>(device_stick), gubsy_2d_analog);
}

bool remove_bind_at(BindsProfile& profile, BindsActionType type, int index) {
    if (index < 0)
        return false;
    std::size_t i = static_cast<std::size_t>(index);
    if (type == BindsActionType::Button) {
        const auto& binds = profile.button_binds();
        if (i >= binds.size())
            return false;
        return ginput::remove_button_bind(profile, binds[i]);
    }
    if (type == BindsActionType::Analog1D) {
        const auto& binds = profile.axis_1d_binds();
        if (i >= binds.size())
            return false;
        return ginput::remove_axis_1d_bind(profile, binds[i]);
    }

    const auto& binds = profile.axis_2d_binds();
    if (i >= binds.size())
        return false;
    return ginput::remove_axis_2d_bind(profile, binds[i]);
}

bool replace_bind_at(BindsProfile& profile, BindsActionType type, int index, int device_code,
                     int action_id) {
    if (index >= 0)
        (void)remove_bind_at(profile, type, index);

    if (type == BindsActionType::Button)
        return ginput::add_button_bind(profile, ginput::ButtonBind{device_code, action_id});
    if (type == BindsActionType::Analog1D)
        return ginput::add_axis_1d_bind(profile, ginput::Axis1DBind{device_code, action_id});
    return ginput::add_axis_2d_bind(profile, ginput::Axis2DBind{device_code, action_id});
}

void remove_binds_for_action(BindsProfile& profile, BindsActionType type, int action_id) {
    if (type == BindsActionType::Button) {
        std::vector<ginput::ButtonBind> remove;
        for (const auto& bind : profile.button_binds()) {
            if (same_button_action(bind, action_id))
                remove.push_back(bind);
        }
        for (const auto& bind : remove)
            (void)ginput::remove_button_bind(profile, bind);
        return;
    }
    if (type == BindsActionType::Analog1D) {
        std::vector<ginput::Axis1DBind> remove;
        for (const auto& bind : profile.axis_1d_binds()) {
            if (same_axis_1d_action(bind, action_id))
                remove.push_back(bind);
        }
        for (const auto& bind : remove)
            (void)ginput::remove_axis_1d_bind(profile, bind);
        return;
    }

    std::vector<ginput::Axis2DBind> remove;
    for (const auto& bind : profile.axis_2d_binds()) {
        if (same_axis_2d_action(bind, action_id))
            remove.push_back(bind);
    }
    for (const auto& bind : remove)
        (void)ginput::remove_axis_2d_bind(profile, bind);
}

void clear_binds(BindsProfile& profile) {
    BindsProfile cleared;
    cleared.id = profile.id;
    cleared.name = profile.name;
    profile = std::move(cleared);
}

std::vector<BindsProfile>& get_binds_profiles_pool(EngineState& engine) {
    return engine.binds_profiles;
}

void register_binds_schema(EngineState& engine, const BindsSchema& schema) {
    g_schema = schema;
    auto profiles = load_all_binds_profiles();
    ginput::ReconcileReport report = ginput::reconcile_profiles(profiles, g_schema);
    if (report.changed())
        (void)ginput::save_profiles_file(binds_profiles_path(), profiles, "binds_profiles");
    engine.binds_profiles = std::move(profiles);
}

const BindsSchema& get_binds_schema() {
    return g_schema;
}
