#pragma once

#include "engine/vid.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

// DemoItemDef: template metadata registered by Lua mods. Mods call register_item{}
// to append more defs; gameplay consumes demo_item_defs(). See docs/demo_defs.md.
struct DemoItemDef {
    std::string id;
    std::string label;
    glm::vec2 position{0.0f, 0.0f};
    float radius{0.6f};
    int sprite_id{-1};
    std::string sprite_name;
    glm::vec3 color{0.3f, 0.5f, 0.9f}; // normalized 0..1
};

struct DemoItemInstance {
    VID vid{};
    int def_index{-1};
    glm::vec2 position{0.0f, 0.0f};
    bool active{false};
};

bool load_demo_item_defs();
void unload_demo_item_defs();

const std::vector<DemoItemDef>& demo_item_defs();
const std::vector<DemoItemInstance>& demo_item_instances();
const DemoItemDef* demo_item_def(const DemoItemInstance& inst);
void trigger_demo_item_use(const DemoItemInstance& inst);
