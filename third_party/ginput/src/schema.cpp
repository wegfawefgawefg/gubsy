#include "ginput/schema.hpp"

#include <utility>

namespace ginput {
namespace {

const SchemaEntry* find_entry(const std::vector<SchemaEntry>& entries, int id) {
    for (const SchemaEntry& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

bool add_entry(std::vector<SchemaEntry>& entries, int id, std::string label, std::string category,
               int order) {
    if (find_entry(entries, id) != nullptr) {
        return false;
    }
    entries.push_back(SchemaEntry{id, std::move(label), std::move(category), order});
    return true;
}

} // namespace

bool Schema::add_action(ActionId id, std::string label, std::string category, int order) {
    return add_entry(action_entries, id, std::move(label), std::move(category), order);
}

bool Schema::add_axis_1d(Axis1DId id, std::string label, std::string category, int order) {
    return add_entry(axis_1d_entries, id, std::move(label), std::move(category), order);
}

bool Schema::add_axis_2d(Axis2DId id, std::string label, std::string category, int order) {
    return add_entry(axis_2d_entries, id, std::move(label), std::move(category), order);
}

bool Schema::has_action(ActionId id) const {
    return find_action(id) != nullptr;
}

bool Schema::has_axis_1d(Axis1DId id) const {
    return find_axis_1d(id) != nullptr;
}

bool Schema::has_axis_2d(Axis2DId id) const {
    return find_axis_2d(id) != nullptr;
}

const SchemaEntry* Schema::find_action(ActionId id) const {
    return find_entry(action_entries, id);
}

const SchemaEntry* Schema::find_axis_1d(Axis1DId id) const {
    return find_entry(axis_1d_entries, id);
}

const SchemaEntry* Schema::find_axis_2d(Axis2DId id) const {
    return find_entry(axis_2d_entries, id);
}

const std::vector<SchemaEntry>& Schema::actions() const {
    return action_entries;
}

const std::vector<SchemaEntry>& Schema::axes_1d() const {
    return axis_1d_entries;
}

const std::vector<SchemaEntry>& Schema::axes_2d() const {
    return axis_2d_entries;
}

int version_major() {
    return 0;
}

} // namespace ginput
