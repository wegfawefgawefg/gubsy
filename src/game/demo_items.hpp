#pragma once

#include "engine/vid_pool.hpp"

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
    int def_index{-1};
    glm::vec2 position{0.0f, 0.0f};
};

using DemoItemPool = VidPool<DemoItemInstance>;

bool load_demo_item_defs();
void unload_demo_item_defs();
bool demo_items_active();
void set_demo_item_mod_filter(const std::vector<std::string>& ids);

const std::vector<DemoItemDef>& demo_item_defs();
const std::vector<DemoItemPool::Entry>& demo_item_instance_slots();
const DemoItemDef* demo_item_def(const DemoItemInstance& inst);
void trigger_demo_item_use(const DemoItemInstance& inst);
