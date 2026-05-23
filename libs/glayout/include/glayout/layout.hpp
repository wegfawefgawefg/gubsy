#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace glayout {

enum class FormFactor {
    Desktop = 0,
    Tablet = 1,
    Phone = 2,
};

enum class DiagnosticSeverity {
    Warning,
    Error,
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    int line = 1;
    int column = 1;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Object {
    int id = 0;
    std::string label;
    Rect rect;
};

struct Layout {
    int id = 0;
    std::string label;
    int width = 0;
    int height = 0;
    FormFactor form_factor = FormFactor::Desktop;
    std::vector<Object> objects;
};

struct ParseResult {
    bool ok = false;
    std::vector<Layout> layouts;
    std::vector<Diagnostic> diagnostics;
};

struct LayoutStore {
    std::vector<Layout> layouts;

    void clear();
    void add_or_replace(const Layout& layout);
    const Layout* find_best(int layout_id, int target_width, int target_height,
                            FormFactor preferred_form_factor) const;
    Layout* find_exact(int layout_id, int width, int height, FormFactor form_factor);
    const Layout* find_exact(int layout_id, int width, int height, FormFactor form_factor) const;
    ParseResult load_file(const std::filesystem::path& path);
    bool save_file(const std::filesystem::path& path) const;
};

std::string_view to_string(FormFactor form_factor);
FormFactor form_factor_from_string(std::string_view text,
                                   std::vector<Diagnostic>* diagnostics = nullptr);

Rect map_rect(Rect parent, Rect child_normalized);
bool intersects(Rect a, Rect b);
Rect intersection(Rect a, Rect b);

void add_or_replace_object(Layout& layout, const Object& object);
bool remove_object(Layout& layout, int object_id);
bool remove_object(Layout& layout, std::string_view label);

void add_or_replace_layout(std::vector<Layout>& layouts, const Layout& layout);

const Object* find_object(const Layout& layout, int object_id);
const Object* find_object(const Layout& layout, std::string_view label);

const Layout* find_best_layout(const std::vector<Layout>& layouts, int layout_id, int target_width,
                               int target_height, FormFactor preferred_form_factor);
const Layout* find_best_layout(const LayoutStore& store, int layout_id, int target_width,
                               int target_height, FormFactor preferred_form_factor);

ParseResult parse_layouts(std::string_view text);
std::string write_layouts(const std::vector<Layout>& layouts);
std::string write_layouts(const LayoutStore& store);

ParseResult load_layout_file(const std::filesystem::path& path);
bool save_layout_file(const std::filesystem::path& path, const std::vector<Layout>& layouts);
bool save_layout_file(const std::filesystem::path& path, const LayoutStore& store);

int generate_layout_id(const std::vector<Layout>& layouts);
int generate_object_id(const Layout& layout);

} // namespace glayout
