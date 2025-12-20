#include "engine/ui_layouts.hpp"

#include "engine/globals.hpp"
#include "engine/parser.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_set>

namespace {

constexpr const char* kUILayoutsPath = "data/ui_layouts/layouts.sxp";

const char* form_factor_to_string(UILayoutFormFactor factor) {
    switch (factor) {
        case UILayoutFormFactor::Desktop: return "desktop";
        case UILayoutFormFactor::Tablet: return "tablet";
        case UILayoutFormFactor::Phone: return "phone";
    }
    return "desktop";
}

UILayoutFormFactor form_factor_from_string(const std::string& value) {
    if (value == "tablet") return UILayoutFormFactor::Tablet;
    if (value == "phone") return UILayoutFormFactor::Phone;
    return UILayoutFormFactor::Desktop;
}

UILayoutFormFactor g_current_form_factor = UILayoutFormFactor::Desktop;

std::vector<UILayout> parse_ui_layouts_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "ui_layouts")) {
            root = &node;
            break;
        }
    }
    std::vector<UILayout> layouts;
    if (!root)
        return layouts;

    for (size_t i = 1; i < root->list.size(); ++i) {
        const sexp::SValue& entry = root->list[i];
        if (entry.type != sexp::SValue::Type::List || entry.list.empty())
            continue;
        if (!sexp::is_symbol(entry.list.front(), "layout"))
            continue;

        auto id = sexp::extract_int(entry, "id");
        auto label = sexp::extract_string(entry, "label");
        if (!id || !label)
            continue;

        UILayout layout{};
        layout.id = *id;
        layout.label = *label;

        // Parse resolution
        const sexp::SValue* res_node = sexp::find_child(entry, "resolution");
        if (res_node && res_node->type == sexp::SValue::Type::List) {
            auto width = sexp::extract_int(*res_node, "width");
            auto height = sexp::extract_int(*res_node, "height");
            if (width && height) {
                layout.resolution_width = *width;
                layout.resolution_height = *height;
            }
        }

        // Parse form factor
        if (const sexp::SValue* ff_node = sexp::find_child(entry, "form_factor")) {
            if (ff_node->type == sexp::SValue::Type::List && ff_node->list.size() >= 2) {
                const sexp::SValue& token = ff_node->list[1];
                if (token.type == sexp::SValue::Type::Symbol || token.type == sexp::SValue::Type::String) {
                    layout.form_factor = form_factor_from_string(token.text);
                }
            }
        }

        // Parse objects
        const sexp::SValue* objects_node = sexp::find_child(entry, "objects");
        if (objects_node && objects_node->type == sexp::SValue::Type::List) {
            for (size_t j = 1; j < objects_node->list.size(); ++j) {
                const sexp::SValue& obj = objects_node->list[j];
                if (obj.type != sexp::SValue::Type::List || obj.list.empty())
                    continue;
                if (!sexp::is_symbol(obj.list.front(), "object"))
                    continue;

                auto obj_id = sexp::extract_int(obj, "id");
                auto obj_label = sexp::extract_string(obj, "label");
                auto x = sexp::extract_float(obj, "x");
                auto y = sexp::extract_float(obj, "y");
                auto w = sexp::extract_float(obj, "w");
                auto h = sexp::extract_float(obj, "h");

                if (obj_id && obj_label && x && y && w && h) {
                    UIObject ui_obj{};
                    ui_obj.id = *obj_id;
                    ui_obj.label = *obj_label;
                    ui_obj.x = *x;
                    ui_obj.y = *y;
                    ui_obj.w = *w;
                    ui_obj.h = *h;
                    layout.objects.push_back(ui_obj);
                }
            }
        }

        layouts.push_back(std::move(layout));
    }
    return layouts;
}

std::vector<UILayout> read_ui_layouts_from_disk() {
    std::ifstream f(kUILayoutsPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[ui_layouts] Failed to parse %s\n", kUILayoutsPath);
        return {};
    }
    return parse_ui_layouts_tree(*parsed);
}

bool write_ui_layouts_file(const std::vector<UILayout>& layouts) {
    namespace fs = std::filesystem;
    fs::path path(kUILayoutsPath);
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(ui_layouts\n";
    for (const auto& layout : layouts) {
        out << "  (layout\n";
        out << "    (id " << layout.id << ")\n";
        out << "    (label " << sexp::quote_string(layout.label) << ")\n";
        out << "    (resolution (width " << layout.resolution_width << ") (height " << layout.resolution_height << "))\n";
        out << "    (form_factor " << form_factor_to_string(layout.form_factor) << ")\n";
        out << "    (objects\n";
        for (const auto& obj : layout.objects) {
            out << "      (object (id " << obj.id << ") (label " << sexp::quote_string(obj.label) << ") ";
            out << "(x " << obj.x << ") (y " << obj.y << ") (w " << obj.w << ") (h " << obj.h << "))\n";
        }
        out << "    )\n";
        out << "  )\n";
    }
    out << ")\n";
    return out.good();
}

} // namespace

void UILayout::add_object(int obj_id, const std::string& object_label,
                          float x, float y, float w, float h) {
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
    auto it = std::remove_if(objects.begin(), objects.end(),
                             [&object_label](const UIObject& obj) { return obj.label == object_label; });
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
        if (existing.id == layout.id &&
            existing.resolution_width == layout.resolution_width &&
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

const UILayout* get_ui_layout_for_resolution(int layout_id, int width, int height) {
    if (!es)
        return nullptr;

    // Find all layouts with matching id
    std::vector<const UILayout*> candidates;
    for (const auto& layout : es->ui_layouts_pool) {
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
        float layout_aspect = static_cast<float>(layout->resolution_width) / static_cast<float>(layout->resolution_height);
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
    if (const UILayout* match = pick_best(desired, true))
        return match;
    return pick_best(desired, false);
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

bool load_ui_layouts_pool() {
    if (!es)
        return false;
    es->ui_layouts_pool = read_ui_layouts_from_disk();
    return true;
}

void reload_ui_layouts_pool() {
    load_ui_layouts_pool();
}

std::vector<UILayout>& get_ui_layouts_pool() {
    return es->ui_layouts_pool;
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
