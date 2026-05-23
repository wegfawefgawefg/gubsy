#pragma once

#include <glayout/layout.hpp>
#include <string>
#include <vector>
struct EngineState;

using UIObject = glayout::Object;
using UILayout = glayout::Layout;
using UILayoutFormFactor = glayout::FormFactor;

/*
 Create a new UI layout for a specific resolution
*/
UILayout create_ui_layout(int id, const std::string& label, int width, int height);
void add_ui_object(UILayout& layout, int obj_id, const std::string& label, float x, float y,
                   float w, float h);

/*
 Save UI layout to disk
*/
bool save_ui_layout(const UILayout& layout);

/*
 Get best matching UI layout for target resolution
 Finds layout with matching id and closest resolution/aspect ratio
*/
const UILayout* get_ui_layout_for_resolution(EngineState& engine, int layout_id, int width,
                                             int height);

/*
 Get UI object from layout by id or label
*/
const UIObject* get_ui_object(const UILayout& layout, int obj_id);
const UIObject* get_ui_object(const UILayout& layout, const std::string& label);

/*
 Load all UI layouts into pool
*/
bool load_ui_layouts_pool(EngineState& engine);

/*
 Reload UI layouts from disk
*/
void reload_ui_layouts_pool(EngineState& engine);

/*
 Get reference to UI layouts pool
*/
std::vector<UILayout>& get_ui_layouts_pool(EngineState& engine);

void set_ui_layout_form_factor(UILayoutFormFactor factor);
UILayoutFormFactor current_ui_layout_form_factor();

/*
 Generate random 8-digit IDs for runtime-created layouts/objects
*/
int generate_ui_layout_id();
int generate_ui_object_id();
