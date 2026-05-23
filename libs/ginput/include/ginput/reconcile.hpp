#pragma once

#include "ginput/profile.hpp"
#include "ginput/schema.hpp"

#include <string>
#include <vector>

namespace ginput {

enum class ReconcileChangeKind {
    RemovedMissingAction,
    RemovedMissingAxis1D,
    RemovedMissingAxis2D,
    RemovedDuplicateButtonBind,
    RemovedDuplicateAxis1DBind,
    RemovedDuplicateAxis2DBind,
};

struct ReconcileChange {
    ReconcileChangeKind kind = ReconcileChangeKind::RemovedMissingAction;
    int profile_id = -1;
    int source_id = 0;
    int target_id = -1;
    std::string message;
};

struct ReconcileReport {
    std::vector<ReconcileChange> changes;

    bool changed() const;
};

ReconcileReport reconcile_profile(InputProfile& profile, const Schema& schema);
ReconcileReport reconcile_profiles(std::vector<InputProfile>& profiles, const Schema& schema);

} // namespace ginput
