#include "ginput/reconcile.hpp"

#include <sstream>
#include <utility>

namespace ginput {
namespace {

std::string message_for(const char* reason, int source_id, int target_id) {
    std::ostringstream out;
    out << reason << ": source " << source_id << " target " << target_id;
    return out.str();
}

void add_change(ReconcileReport& report, ReconcileChangeKind kind, int profile_id, int source_id,
                int target_id, const char* reason) {
    report.changes.push_back(ReconcileChange{kind, profile_id, source_id, target_id,
                                             message_for(reason, source_id, target_id)});
}

} // namespace

bool ReconcileReport::changed() const {
    return !changes.empty();
}

ReconcileReport reconcile_profile(InputProfile& profile, const Schema& schema) {
    ReconcileReport report;

    std::vector<ButtonBind> valid_buttons;
    valid_buttons.reserve(profile.button_binds().size());
    for (const ButtonBind& bind : profile.button_binds()) {
        if (!schema.has_action(bind.action)) {
            add_change(report, ReconcileChangeKind::RemovedMissingAction, profile.id,
                       bind.device_button, bind.action, "removed missing action bind");
            continue;
        }
        bool duplicate = false;
        for (const ButtonBind& existing : valid_buttons) {
            if (same_bind(existing, bind)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            add_change(report, ReconcileChangeKind::RemovedDuplicateButtonBind, profile.id,
                       bind.device_button, bind.action, "removed duplicate button bind");
            continue;
        }
        valid_buttons.push_back(bind);
    }

    std::vector<Axis1DBind> valid_axes_1d;
    valid_axes_1d.reserve(profile.axis_1d_binds().size());
    for (const Axis1DBind& bind : profile.axis_1d_binds()) {
        if (!schema.has_axis_1d(bind.axis_1d)) {
            add_change(report, ReconcileChangeKind::RemovedMissingAxis1D, profile.id,
                       bind.device_axis, bind.axis_1d, "removed missing 1d axis bind");
            continue;
        }
        bool duplicate = false;
        for (const Axis1DBind& existing : valid_axes_1d) {
            if (same_bind(existing, bind)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            add_change(report, ReconcileChangeKind::RemovedDuplicateAxis1DBind, profile.id,
                       bind.device_axis, bind.axis_1d, "removed duplicate 1d axis bind");
            continue;
        }
        valid_axes_1d.push_back(bind);
    }

    std::vector<Axis2DBind> valid_axes_2d;
    valid_axes_2d.reserve(profile.axis_2d_binds().size());
    for (const Axis2DBind& bind : profile.axis_2d_binds()) {
        if (!schema.has_axis_2d(bind.axis_2d)) {
            add_change(report, ReconcileChangeKind::RemovedMissingAxis2D, profile.id,
                       bind.device_stick, bind.axis_2d, "removed missing 2d axis bind");
            continue;
        }
        bool duplicate = false;
        for (const Axis2DBind& existing : valid_axes_2d) {
            if (same_bind(existing, bind)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            add_change(report, ReconcileChangeKind::RemovedDuplicateAxis2DBind, profile.id,
                       bind.device_stick, bind.axis_2d, "removed duplicate 2d axis bind");
            continue;
        }
        valid_axes_2d.push_back(bind);
    }
    InputProfile reconciled;
    reconciled.id = profile.id;
    reconciled.name = profile.name;
    for (const ButtonBind& bind : valid_buttons) {
        add_button_bind(reconciled, bind);
    }
    for (const Axis1DBind& bind : valid_axes_1d) {
        add_axis_1d_bind(reconciled, bind);
    }
    for (const Axis2DBind& bind : valid_axes_2d) {
        add_axis_2d_bind(reconciled, bind);
    }
    profile = std::move(reconciled);

    return report;
}

ReconcileReport reconcile_profiles(std::vector<InputProfile>& profiles, const Schema& schema) {
    ReconcileReport out;
    for (InputProfile& profile : profiles) {
        ReconcileReport one = reconcile_profile(profile, schema);
        out.changes.insert(out.changes.end(), one.changes.begin(), one.changes.end());
    }
    return out;
}

} // namespace ginput
