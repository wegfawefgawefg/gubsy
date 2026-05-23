#pragma once

#include <glayout/layout.hpp>
#include <string>
#include <vector>

struct EngineState;

using UIObject = glayout::Object;
using UILayout = glayout::Layout;
using UILayoutFormFactor = glayout::FormFactor;

UILayout create_ui_layout(int id, const std::string& label, int width, int height);
void add_ui_object(UILayout& layout, int obj_id, const std::string& label, float x, float y,
                   float w, float h);

bool save_ui_layout(const UILayout& layout);

const UILayout* get_ui_layout_for_resolution(EngineState& engine, int layout_id, int width,
                                             int height);

const UIObject* get_ui_object(const UILayout& layout, int obj_id);
const UIObject* get_ui_object(const UILayout& layout, const std::string& label);

bool load_ui_layouts_pool(EngineState& engine);
void reload_ui_layouts_pool(EngineState& engine);
std::vector<UILayout>& get_ui_layouts_pool(EngineState& engine);

void set_ui_layout_form_factor(UILayoutFormFactor factor);
UILayoutFormFactor current_ui_layout_form_factor();

int generate_ui_layout_id();
int generate_ui_object_id();
