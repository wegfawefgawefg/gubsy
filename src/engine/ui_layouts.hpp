#pragma once

#include <string>
#include <vector>

// UI element with normalized positioning [0.0, 1.0]
struct UIObject {
    int id;
    std::string label;
    float x, y, w, h;
};

// UI layout for a specific resolution
struct UILayout {
    int id;                          // Layout identifier (shared across resolutions)
    std::string label;               // e.g., "PlayScreen", "MenuScreen"
    int resolution_width;
    int resolution_height;
    std::vector<UIObject> objects;

    // Idempotent - overwrites if object with same id exists
    void add_object(int obj_id, const std::string& label, float x, float y, float w, float h);

    // Remove object by id or label
    bool remove_object(int obj_id);
    bool remove_object(const std::string& label);
};

/*
 Create a new UI layout for a specific resolution
*/
UILayout create_ui_layout(int id, const std::string& label, int width, int height);

/*
 Save UI layout to disk
*/
bool save_ui_layout(const UILayout& layout);

/*
 Get best matching UI layout for target resolution
 Finds layout with matching id and closest resolution/aspect ratio
*/
const UILayout* get_ui_layout_for_resolution(int layout_id, int width, int height);

/*
 Get UI object from layout by id or label
*/
const UIObject* get_ui_object(const UILayout& layout, int obj_id);
const UIObject* get_ui_object(const UILayout& layout, const std::string& label);

/*
 Load all UI layouts into pool
*/
bool load_ui_layouts_pool();

/*
 Reload UI layouts from disk
*/
void reload_ui_layouts_pool();

/*
 Get reference to UI layouts pool
*/
std::vector<UILayout>& get_ui_layouts_pool();

/*
 Generate random 8-digit IDs for runtime-created layouts/objects
*/
int generate_ui_layout_id();
int generate_ui_object_id();
