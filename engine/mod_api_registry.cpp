#include "engine/mod_api_registry.hpp"

#include <algorithm>
#include <stdexcept>

ModApiRegistry& ModApiRegistry::instance() {
    static ModApiRegistry registry;
    return registry;
}

void ModApiRegistry::register_api(ModApiDescriptor desc) {
    if (desc.name.empty())
        throw std::runtime_error("ModApiDescriptor requires a name");
    auto it = std::find_if(apis_.begin(), apis_.end(),
                           [&](const ModApiDescriptor& existing) {
                               return existing.name == desc.name;
                           });
    if (it != apis_.end())
        throw std::runtime_error("Duplicate mod API registration: " + desc.name);
    apis_.push_back(std::move(desc));
}

const ModApiDescriptor* ModApiRegistry::find(std::string_view name) const {
    auto it = std::find_if(apis_.begin(), apis_.end(),
                           [&](const ModApiDescriptor& desc) {
                               return desc.name == name;
                           });
    if (it == apis_.end())
        return nullptr;
    return &*it;
}
