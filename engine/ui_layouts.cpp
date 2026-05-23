#include "engine/ui_layouts.hpp"

#include "engine/engine_state.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/project_paths.hpp"

#include <algorithm>
#include <filesystem>
#include <glayout/layout.hpp>
#include <random>
#include <unordered_set>

namespace {

UILayoutFormFactor g_current_form_factor = UILayoutFormFactor::Desktop;

glayout::FormFactor to_glayout_form_factor(UILayoutFormFactor factor) {
    switch (factor) {
    case UILayoutFormFactor::Desktop:
        return glayout::FormFactor::Desktop;
    case UILayoutFormFactor::Tablet:
        return glayout::FormFactor::Tablet;
    case UILayoutFormFactor::Phone:
        return glayout::FormFactor::Phone;
    }
    return glayout::FormFactor::Desktop;
}

UILayoutFormFactor from_glayout_form_factor(glayout::FormFactor factor) {
    switch (factor) {
    case glayout::FormFactor::Desktop:
        return UILayoutFormFactor::Desktop;
    case glayout::FormFactor::Tablet:
        return UILayoutFormFactor::Tablet;
    case glayout::FormFactor::Phone:
        return UILayoutFormFactor::Phone;
    }
    return UILayoutFormFactor::Desktop;
}

glayout::Layout to_glayout_layout(const UILayout& layout) {
    glayout::Layout out;
    out.id = layout.id;
    out.label = layout.label;
    out.width = layout.resolution_width;
    out.height = layout.resolution_height;
    out.form_factor = to_glayout_form_factor(layout.form_factor);
    out.objects.reserve(layout.objects.size());
    for (const UIObject& object : layout.objects) {
        out.objects.push_back(glayout::Object{
            object.id,
            object.label,
            glayout::Rect{object.x, object.y, object.w, object.h},
        });
    }
    return out;
}

UILayout from_glayout_layout(const glayout::Layout& layout) {
    UILayout out;
    out.id = layout.id;
    out.label = layout.label;
    out.resolution_width = layout.width;
    out.resolution_height = layout.height;
    out.form_factor = from_glayout_form_factor(layout.form_factor);
    out.objects.reserve(layout.objects.size());
    for (const glayout::Object& object : layout.objects) {
        out.objects.push_back(UIObject{
            object.id,
            object.label,
            object.rect.x,
            object.rect.y,
            object.rect.w,
            object.rect.h,
        });
    }
    return out;
}

std::vector<glayout::Layout> to_glayout_layouts(const std::vector<UILayout>& layouts) {
    std::vector<glayout::Layout> out;
    out.reserve(layouts.size());
    for (const UILayout& layout : layouts)
        out.push_back(to_glayout_layout(layout));
    return out;
}

std::vector<UILayout> from_glayout_layouts(const std::vector<glayout::Layout>& layouts) {
    std::vector<UILayout> out;
    out.reserve(layouts.size());
    for (const glayout::Layout& layout : layouts)
        out.push_back(from_glayout_layout(layout));
    return out;
}

std::filesystem::path ui_layouts_path() {
    return data_path("ui_layouts/layouts.lisp");
}

std::vector<UILayout> read_ui_layouts_from_disk() {
    glayout::ParseResult result = glayout::load_layout_file(ui_layouts_path());
    if (!result.ok)
        return {};
    return from_glayout_layouts(result.layouts);
}

bool write_ui_layouts_file(const std::vector<UILayout>& layouts) {
    return glayout::save_layout_file(ui_layouts_path(), to_glayout_layouts(layouts));
}

} // namespace

void UILayout::add_object(int obj_id, const std::string& object_label, float x, float y, float w,
                          float h) {
    // Idempotent - check if object with same id exists
    for (auto& obj : objects) {
        if (obj.id == obj_id) {
            // Update existing object
            obj.label = object_label;
            obj.x = x;
            obj.y = y;
            obj.w = w;
            obj.h = h;
            return;
        }
    }
    // Add new object
    objects.push_back({obj_id, object_label, x, y, w, h});
}

bool UILayout::remove_object(int obj_id) {
    auto it = std::remove_if(objects.begin(), objects.end(),
                             [obj_id](const UIObject& obj) { return obj.id == obj_id; });
    if (it != objects.end()) {
        objects.erase(it, objects.end());
        return true;
    }
    return false;
}

bool UILayout::remove_object(const std::string& object_label) {
    auto it = std::remove_if(objects.begin(), objects.end(), [&object_label](const UIObject& obj) {
        return obj.label == object_label;
    });
    if (it != objects.end()) {
        objects.erase(it, objects.end());
        return true;
    }
    return false;
}

UILayout create_ui_layout(int id, const std::string& label, int width, int height) {
    UILayout layout{};
    layout.id = id;
    layout.label = label;
    layout.resolution_width = width;
    layout.resolution_height = height;
    return layout;
}

bool save_ui_layout(const UILayout& layout) {
    if (layout.id <= 0)
        return false;
    if (layout.label.empty())
        return false;

    auto layouts = read_ui_layouts_from_disk();

    // Check if layout with same id and resolution exists
    bool updated = false;
    for (auto& existing : layouts) {
        if (existing.id == layout.id && existing.resolution_width == layout.resolution_width &&
            existing.resolution_height == layout.resolution_height &&
            existing.form_factor == layout.form_factor) {
            existing = layout;
            updated = true;
            break;
        }
    }

    if (!updated) {
        layouts.push_back(layout);
    }

    return write_ui_layouts_file(layouts);
}

const UILayout* get_ui_layout_for_resolution(EngineState& engine, int layout_id, int width,
                                             int height) {
    // Find all layouts with matching id
    std::vector<const UILayout*> candidates;
    for (const auto& layout : engine.ui_layouts_pool) {
        if (layout.id == layout_id) {
            candidates.push_back(&layout);
        }
    }

    if (candidates.empty())
        return nullptr;

    // If only one candidate, return it
    if (candidates.size() == 1)
        return candidates[0];

    auto score_layout = [&](const UILayout* layout) {
        float target_aspect = static_cast<float>(width) / static_cast<float>(height);
        float layout_aspect = static_cast<float>(layout->resolution_width) /
                              static_cast<float>(layout->resolution_height);
        float aspect_distance = std::abs(target_aspect - layout_aspect);
        float res_dx = static_cast<float>(width - layout->resolution_width);
        float res_dy = static_cast<float>(height - layout->resolution_height);
        float res_distance = std::sqrt(res_dx * res_dx + res_dy * res_dy);
        return aspect_distance * 1000.0f + res_distance;
    };

    auto pick_best = [&](UILayoutFormFactor factor, bool require_match) -> const UILayout* {
        const UILayout* best = nullptr;
        float best_score = std::numeric_limits<float>::max();
        for (const auto* layout : candidates) {
            if (require_match && layout->form_factor != factor)
                continue;
            float score = score_layout(layout);
            if (score < best_score) {
                best = layout;
                best_score = score;
            }
        }
        return best;
    };

    UILayoutFormFactor desired = g_current_form_factor;
    const UILayout* chosen = pick_best(desired, true);
    if (!chosen)
        chosen = pick_best(desired, false);
    if (chosen) {
        layout_editor_notify_active_layout(engine, layout_id, chosen->resolution_width,
                                           chosen->resolution_height);
    }
    return chosen;
}

const UIObject* get_ui_object(const UILayout& layout, int obj_id) {
    for (const auto& obj : layout.objects) {
        if (obj.id == obj_id)
            return &obj;
    }
    return nullptr;
}

const UIObject* get_ui_object(const UILayout& layout, const std::string& label) {
    for (const auto& obj : layout.objects) {
        if (obj.label == label)
            return &obj;
    }
    return nullptr;
}

bool load_ui_layouts_pool(EngineState& engine) {
    engine.ui_layouts_pool = read_ui_layouts_from_disk();
    return true;
}

void reload_ui_layouts_pool(EngineState& engine) {
    load_ui_layouts_pool(engine);
}

std::vector<UILayout>& get_ui_layouts_pool(EngineState& engine) {
    return engine.ui_layouts_pool;
}

void set_ui_layout_form_factor(UILayoutFormFactor factor) {
    g_current_form_factor = factor;
}

UILayoutFormFactor current_ui_layout_form_factor() {
    return g_current_form_factor;
}

int generate_ui_layout_id() {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    std::unordered_set<int> used;

    auto layouts = read_ui_layouts_from_disk();
    used.reserve(layouts.size());
    for (const auto& layout : layouts) {
        used.insert(layout.id);
    }

    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.count(candidate))
            return candidate;
    }
    return dist(rng);
}

int generate_ui_object_id() {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 99999999);
    return dist(rng);
}
