#include "glayout/layout.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <gsexp/sexp.hpp>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_set>

namespace glayout {
namespace {

constexpr float kAspectWeight = 1000.0f;

struct LayoutKey {
    int id = 0;
    int width = 0;
    int height = 0;
    FormFactor form_factor = FormFactor::Desktop;
};

bool same_layout_key(const Layout& layout, const LayoutKey& key) {
    return layout.id == key.id && layout.width == key.width && layout.height == key.height &&
           layout.form_factor == key.form_factor;
}

LayoutKey layout_key(const Layout& layout) {
    return LayoutKey{layout.id, layout.width, layout.height, layout.form_factor};
}

void add_warning(std::vector<Diagnostic>& diagnostics, std::string message, int line = 1,
                 int column = 1) {
    diagnostics.push_back(
        Diagnostic{DiagnosticSeverity::Warning, std::move(message), line, column});
}

void add_error(std::vector<Diagnostic>& diagnostics, std::string message, int line = 1,
               int column = 1) {
    diagnostics.push_back(Diagnostic{DiagnosticSeverity::Error, std::move(message), line, column});
}

bool has_error(const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error)
            return true;
    }
    return false;
}

std::string layout_key_string(const Layout& layout) {
    return std::to_string(layout.id) + ":" + std::to_string(layout.width) + "x" +
           std::to_string(layout.height) + ":" + std::string(to_string(layout.form_factor));
}

bool parse_resolution(gsexp::Node layout_node, int& width, int& height) {
    gsexp::Node res_node = gsexp::FormView(layout_node).find("resolution");
    if (!res_node.valid())
        return false;

    gsexp::FormView resolution(res_node);
    std::optional<int> parsed_width = resolution.get_int("width");
    std::optional<int> parsed_height = resolution.get_int("height");
    if (!parsed_width || !parsed_height)
        return false;

    width = *parsed_width;
    height = *parsed_height;
    return true;
}

FormFactor parse_form_factor(gsexp::Node layout_node, std::vector<Diagnostic>& diagnostics) {
    gsexp::Node form_node = gsexp::FormView(layout_node).find("form_factor");
    if (!form_node.valid() || form_node.child_count() < 2)
        return FormFactor::Desktop;

    gsexp::Node value = form_node.child_at(1);
    if (value.type() != gsexp::ValueType::Atom && value.type() != gsexp::ValueType::String) {
        add_warning(diagnostics, "layout form_factor is not an atom/string; defaulting to desktop");
        return FormFactor::Desktop;
    }

    return form_factor_from_string(value.text(), &diagnostics);
}

bool parse_object(gsexp::Node object_node, Object& out) {
    gsexp::FormView object(object_node);
    std::optional<int> id = object.get_int("id");
    std::optional<std::string> label = object.get_string("label");
    std::optional<float> x = object.get_float("x");
    std::optional<float> y = object.get_float("y");
    std::optional<float> w = object.get_float("w");
    std::optional<float> h = object.get_float("h");

    if (!id || !label || !x || !y || !w || !h)
        return false;

    out = Object{*id, *label, Rect{*x, *y, *w, *h}};
    return true;
}

bool parse_layout_node(gsexp::Node layout_node, Layout& out, std::vector<Diagnostic>& diagnostics) {
    gsexp::FormView layout_form(layout_node);
    std::optional<int> id = layout_form.get_int("id");
    std::optional<std::string> label = layout_form.get_string("label");

    int width = 0;
    int height = 0;
    if (!id || !label || !parse_resolution(layout_node, width, height))
        return false;

    Layout layout;
    layout.id = *id;
    layout.label = *label;
    layout.width = width;
    layout.height = height;
    layout.form_factor = parse_form_factor(layout_node, diagnostics);

    gsexp::Node objects_node = layout_form.find("objects");
    if (objects_node.is_list()) {
        bool first = true;
        for (gsexp::Node object_node : objects_node.children()) {
            if (first) {
                first = false;
                continue;
            }
            if (!object_node.is_list() || object_node.child_count() == 0)
                continue;
            if (!object_node.child_at(0).is_atom("object"))
                continue;

            Object object;
            if (parse_object(object_node, object)) {
                layout.objects.push_back(std::move(object));
            } else {
                add_warning(diagnostics, "skipping malformed object in layout " + layout.label);
            }
        }
    }

    out = std::move(layout);
    return true;
}

float layout_score(const Layout& layout, int target_width, int target_height) {
    if (layout.width <= 0 || layout.height <= 0 || target_width <= 0 || target_height <= 0)
        return std::numeric_limits<float>::max();

    float target_aspect = static_cast<float>(target_width) / static_cast<float>(target_height);
    float layout_aspect = static_cast<float>(layout.width) / static_cast<float>(layout.height);
    float aspect_distance = std::fabs(target_aspect - layout_aspect);
    float dx = static_cast<float>(target_width - layout.width);
    float dy = static_cast<float>(target_height - layout.height);
    float resolution_distance = std::sqrt(dx * dx + dy * dy);

    return aspect_distance * kAspectWeight + resolution_distance;
}

const Layout* find_best_matching_form_factor(const std::vector<Layout>& layouts, int layout_id,
                                             int target_width, int target_height,
                                             FormFactor preferred_form_factor,
                                             bool require_form_factor) {
    const Layout* best = nullptr;
    float best_score = std::numeric_limits<float>::max();

    for (const Layout& layout : layouts) {
        if (layout.id != layout_id)
            continue;
        if (require_form_factor && layout.form_factor != preferred_form_factor)
            continue;

        float score = layout_score(layout, target_width, target_height);
        if (score < best_score) {
            best = &layout;
            best_score = score;
        }
    }

    return best;
}

} // namespace

std::string_view to_string(FormFactor form_factor) {
    switch (form_factor) {
    case FormFactor::Desktop:
        return "desktop";
    case FormFactor::Tablet:
        return "tablet";
    case FormFactor::Phone:
        return "phone";
    }
    return "desktop";
}

FormFactor form_factor_from_string(std::string_view text, std::vector<Diagnostic>* diagnostics) {
    if (text == "desktop")
        return FormFactor::Desktop;
    if (text == "tablet")
        return FormFactor::Tablet;
    if (text == "phone")
        return FormFactor::Phone;

    if (diagnostics) {
        add_warning(*diagnostics,
                    "unknown form_factor '" + std::string(text) + "'; defaulting to desktop");
    }
    return FormFactor::Desktop;
}

Rect map_rect(Rect parent, Rect child_normalized) {
    return Rect{
        parent.x + child_normalized.x * parent.w,
        parent.y + child_normalized.y * parent.h,
        child_normalized.w * parent.w,
        child_normalized.h * parent.h,
    };
}

bool intersects(Rect a, Rect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

Rect intersection(Rect a, Rect b) {
    float left = std::max(a.x, b.x);
    float top = std::max(a.y, b.y);
    float right = std::min(a.x + a.w, b.x + b.w);
    float bottom = std::min(a.y + a.h, b.y + b.h);

    if (right <= left || bottom <= top)
        return Rect{};

    return Rect{left, top, right - left, bottom - top};
}

void add_or_replace_object(Layout& layout, const Object& object) {
    for (Object& existing : layout.objects) {
        if (existing.id == object.id) {
            existing = object;
            return;
        }
    }

    layout.objects.push_back(object);
}

bool remove_object(Layout& layout, int object_id) {
    auto it = std::remove_if(layout.objects.begin(), layout.objects.end(),
                             [object_id](const Object& object) { return object.id == object_id; });
    if (it == layout.objects.end())
        return false;

    layout.objects.erase(it, layout.objects.end());
    return true;
}

bool remove_object(Layout& layout, std::string_view label) {
    auto it = std::remove_if(layout.objects.begin(), layout.objects.end(),
                             [label](const Object& object) { return object.label == label; });
    if (it == layout.objects.end())
        return false;

    layout.objects.erase(it, layout.objects.end());
    return true;
}

void add_or_replace_layout(std::vector<Layout>& layouts, const Layout& layout) {
    LayoutKey key = layout_key(layout);
    bool replaced = false;

    for (auto it = layouts.begin(); it != layouts.end();) {
        if (!same_layout_key(*it, key)) {
            ++it;
            continue;
        }

        if (!replaced) {
            *it = layout;
            replaced = true;
            ++it;
        } else {
            it = layouts.erase(it);
        }
    }

    if (!replaced)
        layouts.push_back(layout);
}

const Object* find_object(const Layout& layout, int object_id) {
    for (const Object& object : layout.objects) {
        if (object.id == object_id)
            return &object;
    }

    return nullptr;
}

const Object* find_object(const Layout& layout, std::string_view label) {
    for (const Object& object : layout.objects) {
        if (object.label == label)
            return &object;
    }

    return nullptr;
}

const Layout* find_best_layout(const std::vector<Layout>& layouts, int layout_id, int target_width,
                               int target_height, FormFactor preferred_form_factor) {
    const Layout* best = find_best_matching_form_factor(layouts, layout_id, target_width,
                                                        target_height, preferred_form_factor, true);
    if (best)
        return best;

    return find_best_matching_form_factor(layouts, layout_id, target_width, target_height,
                                          preferred_form_factor, false);
}

void LayoutStore::clear() {
    layouts.clear();
}

void LayoutStore::add_or_replace(const Layout& layout) {
    add_or_replace_layout(layouts, layout);
}

const Layout* LayoutStore::find_best(int layout_id, int target_width, int target_height,
                                     FormFactor preferred_form_factor) const {
    return find_best_layout(layouts, layout_id, target_width, target_height, preferred_form_factor);
}

Layout* LayoutStore::find_exact(int layout_id, int width, int height, FormFactor form_factor) {
    for (Layout& layout : layouts) {
        if (layout.id == layout_id && layout.width == width && layout.height == height &&
            layout.form_factor == form_factor) {
            return &layout;
        }
    }
    return nullptr;
}

const Layout* LayoutStore::find_exact(int layout_id, int width, int height,
                                      FormFactor form_factor) const {
    for (const Layout& layout : layouts) {
        if (layout.id == layout_id && layout.width == width && layout.height == height &&
            layout.form_factor == form_factor) {
            return &layout;
        }
    }
    return nullptr;
}

ParseResult LayoutStore::load_file(const std::filesystem::path& path) {
    ParseResult result = load_layout_file(path);
    if (result.ok) {
        layouts = result.layouts;
    }
    return result;
}

bool LayoutStore::save_file(const std::filesystem::path& path) const {
    return save_layout_file(path, layouts);
}

const Layout* find_best_layout(const LayoutStore& store, int layout_id, int target_width,
                               int target_height, FormFactor preferred_form_factor) {
    return store.find_best(layout_id, target_width, target_height, preferred_form_factor);
}

ParseResult parse_layouts(std::string_view text) {
    ParseResult result;
    gsexp::ParseResult parsed = gsexp::parse(text);

    for (const gsexp::Diagnostic& diagnostic : parsed.diagnostics) {
        DiagnosticSeverity severity = diagnostic.severity == gsexp::DiagnosticSeverity::Warning
                                          ? DiagnosticSeverity::Warning
                                          : DiagnosticSeverity::Error;
        result.diagnostics.push_back(Diagnostic{
            severity,
            diagnostic.message,
            diagnostic.line,
            diagnostic.column,
        });
    }

    if (!parsed.ok) {
        result.ok = false;
        return result;
    }

    gsexp::Node root;
    for (std::size_t index = 0; index < parsed.root_count(); ++index) {
        gsexp::Node value = parsed.root(index);
        if (!value.is_list() || value.child_count() == 0)
            continue;
        if (value.child_at(0).is_atom("ui_layouts")) {
            root = value;
            break;
        }
    }

    if (!root.valid()) {
        add_error(result.diagnostics, "missing ui_layouts root");
        result.ok = false;
        return result;
    }

    std::unordered_set<std::string> seen_keys;
    bool first = true;
    for (gsexp::Node entry : root.children()) {
        if (first) {
            first = false;
            continue;
        }
        if (!entry.is_list() || entry.child_count() == 0)
            continue;
        if (!entry.child_at(0).is_atom("layout"))
            continue;

        Layout layout;
        if (!parse_layout_node(entry, layout, result.diagnostics)) {
            add_warning(result.diagnostics, "skipping malformed layout");
            continue;
        }

        std::string key = layout_key_string(layout);
        if (seen_keys.contains(key)) {
            add_warning(result.diagnostics,
                        "duplicate layout variant " + key + "; latest wins on update");
        }
        seen_keys.insert(std::move(key));
        result.layouts.push_back(std::move(layout));
    }

    result.ok = !has_error(result.diagnostics);
    return result;
}

std::string write_layouts(const std::vector<Layout>& layouts) {
    std::ostringstream out;
    out << "(ui_layouts\n";

    for (const Layout& layout : layouts) {
        out << "  (layout\n";
        out << "    (id " << layout.id << ")\n";
        out << "    (label " << gsexp::quote_string(layout.label) << ")\n";
        out << "    (resolution (width " << layout.width << ") (height " << layout.height << "))\n";
        out << "    (form_factor " << to_string(layout.form_factor) << ")\n";
        out << "    (objects\n";
        for (const Object& object : layout.objects) {
            out << "      (object (id " << object.id << ") (label "
                << gsexp::quote_string(object.label) << ") ";
            out << "(x " << object.rect.x << ") (y " << object.rect.y << ") ";
            out << "(w " << object.rect.w << ") (h " << object.rect.h << "))\n";
        }
        out << "    )\n";
        out << "  )\n";
    }

    out << ")\n";
    return out.str();
}

std::string write_layouts(const LayoutStore& store) {
    return write_layouts(store.layouts);
}

ParseResult load_layout_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ParseResult result;
        add_error(result.diagnostics, "failed to open layout file: " + path.string());
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parse_layouts(buffer.str());
}

bool save_layout_file(const std::filesystem::path& path, const std::vector<Layout>& layouts) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
    }

    std::ofstream file(path);
    if (!file.is_open())
        return false;

    file << write_layouts(layouts);
    return file.good();
}

bool save_layout_file(const std::filesystem::path& path, const LayoutStore& store) {
    return save_layout_file(path, store.layouts);
}

int generate_layout_id(const std::vector<Layout>& layouts) {
    std::unordered_set<int> used;
    used.reserve(layouts.size());
    for (const Layout& layout : layouts)
        used.insert(layout.id);

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.contains(candidate))
            return candidate;
    }

    return dist(rng);
}

int generate_object_id(const Layout& layout) {
    std::unordered_set<int> used;
    used.reserve(layout.objects.size());
    for (const Object& object : layout.objects)
        used.insert(object.id);

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 99999999);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.contains(candidate))
            return candidate;
    }

    return dist(rng);
}

} // namespace glayout
