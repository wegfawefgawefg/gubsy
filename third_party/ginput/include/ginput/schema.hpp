#pragma once

#include "ginput/types.hpp"

#include <string>
#include <vector>

namespace ginput {

struct SchemaEntry {
    int id = -1;
    std::string label;
    std::string category;
    int order = 0;
};

class Schema {
  public:
    bool add_action(ActionId id, std::string label, std::string category = {}, int order = 0);
    bool add_axis_1d(Axis1DId id, std::string label, std::string category = {}, int order = 0);
    bool add_axis_2d(Axis2DId id, std::string label, std::string category = {}, int order = 0);

    bool has_action(ActionId id) const;
    bool has_axis_1d(Axis1DId id) const;
    bool has_axis_2d(Axis2DId id) const;

    const SchemaEntry* find_action(ActionId id) const;
    const SchemaEntry* find_axis_1d(Axis1DId id) const;
    const SchemaEntry* find_axis_2d(Axis2DId id) const;

    const std::vector<SchemaEntry>& actions() const;
    const std::vector<SchemaEntry>& axes_1d() const;
    const std::vector<SchemaEntry>& axes_2d() const;

  private:
    std::vector<SchemaEntry> action_entries;
    std::vector<SchemaEntry> axis_1d_entries;
    std::vector<SchemaEntry> axis_2d_entries;
};

} // namespace ginput
