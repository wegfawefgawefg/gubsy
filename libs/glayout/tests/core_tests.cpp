#include "glayout/editor.hpp"
#include "glayout/layout.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool nearly_equal(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void test_rect_mapping() {
    glayout::Rect parent{10.0f, 20.0f, 200.0f, 100.0f};
    glayout::Rect child{0.25f, 0.5f, 0.5f, 0.25f};
    glayout::Rect mapped = glayout::map_rect(parent, child);

    require(nearly_equal(mapped.x, 60.0f), "map_rect x");
    require(nearly_equal(mapped.y, 70.0f), "map_rect y");
    require(nearly_equal(mapped.w, 100.0f), "map_rect w");
    require(nearly_equal(mapped.h, 25.0f), "map_rect h");

    glayout::Rect clipped = glayout::intersection(glayout::Rect{0.0f, 0.0f, 10.0f, 10.0f},
                                                  glayout::Rect{5.0f, 5.0f, 10.0f, 10.0f});
    require(glayout::intersects(glayout::Rect{0.0f, 0.0f, 10.0f, 10.0f},
                                glayout::Rect{5.0f, 5.0f, 10.0f, 10.0f}),
            "intersects overlapping rects");
    require(nearly_equal(clipped.x, 5.0f), "intersection x");
    require(nearly_equal(clipped.y, 5.0f), "intersection y");
    require(nearly_equal(clipped.w, 5.0f), "intersection w");
    require(nearly_equal(clipped.h, 5.0f), "intersection h");
}

void test_matching() {
    std::vector<glayout::Layout> layouts{
        glayout::Layout{100, "Title", 1280, 720, glayout::FormFactor::Desktop, {}},
        glayout::Layout{100, "Title", 1920, 1080, glayout::FormFactor::Desktop, {}},
        glayout::Layout{100, "Title", 1080, 1920, glayout::FormFactor::Phone, {}},
    };

    const glayout::Layout* desktop =
        glayout::find_best_layout(layouts, 100, 1920, 1080, glayout::FormFactor::Desktop);
    require(desktop != nullptr, "desktop match exists");
    require(desktop->width == 1920 && desktop->height == 1080, "desktop picks exact 1080p");

    const glayout::Layout* phone =
        glayout::find_best_layout(layouts, 100, 1080, 1920, glayout::FormFactor::Phone);
    require(phone != nullptr, "phone match exists");
    require(phone->form_factor == glayout::FormFactor::Phone, "phone form factor preferred");

    const glayout::Layout* fallback =
        glayout::find_best_layout(layouts, 100, 1920, 1080, glayout::FormFactor::Tablet);
    require(fallback != nullptr, "tablet fallback exists");
    require(fallback->width == 1920 && fallback->height == 1080, "tablet fallback uses best score");
}

void test_layout_store() {
    glayout::LayoutStore store;
    store.add_or_replace(
        glayout::Layout{100, "Title", 1280, 720, glayout::FormFactor::Desktop, {}});
    store.add_or_replace(
        glayout::Layout{100, "Title HD", 1920, 1080, glayout::FormFactor::Desktop, {}});
    store.add_or_replace(
        glayout::Layout{100, "Title Phone", 1080, 1920, glayout::FormFactor::Phone, {}});

    const glayout::Layout* best = store.find_best(100, 1920, 1080, glayout::FormFactor::Desktop);
    require(best != nullptr, "layout store best match exists");
    require(best->label == "Title HD", "layout store best match");

    glayout::Layout* exact = store.find_exact(100, 1080, 1920, glayout::FormFactor::Phone);
    require(exact != nullptr, "layout store exact match exists");
    exact->label = "Phone Edited";

    const glayout::Layout* exact_const =
        store.find_exact(100, 1080, 1920, glayout::FormFactor::Phone);
    require(exact_const != nullptr && exact_const->label == "Phone Edited",
            "layout store exact mutable");

    glayout::Layout replacement{100, "Title Replaced", 1920, 1080, glayout::FormFactor::Desktop,
                                {}};
    store.add_or_replace(replacement);
    best = glayout::find_best_layout(store, 100, 1920, 1080, glayout::FormFactor::Desktop);
    require(best != nullptr && best->label == "Title Replaced", "layout store replace");
    require(store.layouts.size() == 3, "layout store keeps unique variants");
}

void test_parse_write() {
    const std::string text = R"(
(ui_layouts
  (layout
    (id 100)
    (label "Title")
    (resolution (width 1920) (height 1080))
    (form_factor desktop)
    (objects
      (object (id 1) (label "play_button") (x 0.4) (y 0.45) (w 0.2) (h 0.08))
    )
  )
  (layout
    (id 200)
    (label "Phone")
    (resolution (width 1080) (height 1920))
    (form_factor weird)
    (objects)
  )
  (layout
    (id 100)
    (label "Title Duplicate")
    (resolution (width 1920) (height 1080))
    (form_factor desktop)
    (objects)
  )
)
)";

    glayout::ParseResult result = glayout::parse_layouts(text);
    require(result.ok, "parse_layouts ok with warnings");
    require(result.layouts.size() == 3, "parse_layouts layout count");
    require(result.layouts[0].objects.size() == 1, "parse_layouts object count");
    require(result.layouts[0].objects[0].label == "play_button", "parse_layouts object label");
    require(result.layouts[1].form_factor == glayout::FormFactor::Desktop,
            "unknown form factor defaults desktop");
    require(!result.diagnostics.empty(), "warnings are reported");

    std::string written = glayout::write_layouts(result.layouts);
    glayout::ParseResult round_trip = glayout::parse_layouts(written);
    require(round_trip.ok, "round-trip parse ok");
    require(round_trip.layouts.size() == result.layouts.size(), "round-trip layout count");
}

void test_demo_layout_file() {
    glayout::ParseResult result =
        glayout::load_layout_file(std::filesystem::path(GLAYOUT_TEST_DATA_DIR) / "layouts.lisp");
    require(result.ok, "demo layouts parse");
    require(result.layouts.size() == 9, "demo has nine layout variants");

    int title = 0;
    int settings = 0;
    int credits = 0;
    for (const glayout::Layout& layout : result.layouts) {
        if (layout.id == 100)
            ++title;
        if (layout.id == 200)
            ++settings;
        if (layout.id == 300)
            ++credits;
        require(!layout.objects.empty(), "demo layout has objects");
    }

    require(title == 3, "demo title variants");
    require(settings == 3, "demo settings variants");
    require(credits == 3, "demo credits variants");
}

void test_replace_helpers() {
    glayout::Layout layout;
    layout.id = 1;
    layout.label = "A";
    layout.width = 100;
    layout.height = 100;

    glayout::Object a{10, "first", glayout::Rect{0.0f, 0.0f, 0.1f, 0.1f}};
    glayout::Object b{10, "second", glayout::Rect{0.5f, 0.5f, 0.1f, 0.1f}};
    glayout::add_or_replace_object(layout, a);
    glayout::add_or_replace_object(layout, b);

    require(layout.objects.size() == 1, "object replace by id");
    require(layout.objects[0].label == "second", "object replace label");

    std::vector<glayout::Layout> layouts;
    glayout::add_or_replace_layout(layouts, layout);
    layout.label = "B";
    glayout::add_or_replace_layout(layouts, layout);

    require(layouts.size() == 1, "layout replace by variant key");
    require(layouts[0].label == "B", "layout replace label");

    int layout_id = glayout::generate_layout_id(layouts);
    require(layout_id != layouts[0].id, "generated layout id avoids existing id");
    int object_id = glayout::generate_object_id(layout);
    require(object_id != layout.objects[0].id, "generated object id avoids existing id");
}

glayout::Layout make_editor_layout() {
    glayout::Layout layout;
    layout.id = 50;
    layout.label = "Editor";
    layout.width = 100;
    layout.height = 100;
    layout.objects.push_back(glayout::Object{1, "one", glayout::Rect{0.1f, 0.1f, 0.2f, 0.2f}});
    layout.objects.push_back(glayout::Object{2, "two", glayout::Rect{0.6f, 0.6f, 0.2f, 0.2f}});
    return layout;
}

void test_editor_drag_and_undo() {
    glayout::Layout layout = make_editor_layout();
    glayout::EditorState editor;
    editor.snap_enabled = false;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{20.0f, 20.0f, true}, viewport);
    glayout::EditorFrameResult drag_result = glayout::editor_begin_frame(
        editor, layout, glayout::EditorInput{30.0f, 30.0f, true}, viewport);
    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{30.0f, 30.0f, false},
                                viewport);

    require(drag_result.changed, "editor drag changes layout");
    require(editor.dirty, "editor drag marks dirty");
    require(layout.objects[0].rect.x > 0.19f && layout.objects[0].rect.x < 0.21f, "editor drag x");
    require(layout.objects[0].rect.y > 0.19f && layout.objects[0].rect.y < 0.21f, "editor drag y");
    require(editor.undo_stack.size() == 2, "editor drag commits undo history");

    require(glayout::editor_undo(editor, layout), "editor undo succeeds");
    require(nearly_equal(layout.objects[0].rect.x, 0.1f), "editor undo x");
    require(nearly_equal(layout.objects[0].rect.y, 0.1f), "editor undo y");
    require(glayout::editor_redo(editor, layout), "editor redo succeeds");
    require(nearly_equal(layout.objects[0].rect.x, 0.2f), "editor redo x");
}

void test_editor_resize_delete_and_save_request() {
    glayout::Layout layout = make_editor_layout();
    glayout::EditorState editor;
    editor.snap_enabled = false;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{30.0f, 30.0f, true}, viewport);
    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{40.0f, 40.0f, true}, viewport);
    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{40.0f, 40.0f, false},
                                viewport);

    require(layout.objects[0].rect.w > 0.29f, "editor resize width");
    require(layout.objects[0].rect.h > 0.29f, "editor resize height");

    glayout::EditorFrameResult save_result = glayout::editor_begin_frame(
        editor, layout, glayout::EditorInput{0.0f, 0.0f, false, false, false, true}, viewport);
    require(save_result.save_requested, "editor save request result");
    require(editor.save_requested, "editor save request state");
    glayout::editor_mark_saved(editor);
    require(!editor.dirty && !editor.save_requested, "editor mark saved");

    glayout::editor_select_single(editor, 1);
    glayout::EditorFrameResult delete_result = glayout::editor_begin_frame(editor, layout,
                                                                           glayout::EditorInput{
                                                                               0.0f,
                                                                               0.0f,
                                                                               false,
                                                                               false,
                                                                               false,
                                                                               false,
                                                                               false,
                                                                               false,
                                                                               true,
                                                                           },
                                                                           viewport);
    require(delete_result.changed, "editor delete changes layout");
    require(layout.objects.size() == 1, "editor delete removes object");
}

void test_editor_copy_paste() {
    glayout::Layout layout = make_editor_layout();
    glayout::EditorState editor;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_select_single(editor, 0);
    glayout::editor_begin_frame(editor, layout,
                                glayout::EditorInput{
                                    0.0f,
                                    0.0f,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    true,
                                },
                                viewport);
    require(editor.clipboard.size() == 1, "editor copy stores object");

    glayout::EditorFrameResult paste_result = glayout::editor_begin_frame(editor, layout,
                                                                          glayout::EditorInput{
                                                                              0.0f,
                                                                              0.0f,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              false,
                                                                              true,
                                                                          },
                                                                          viewport);
    require(paste_result.changed, "editor paste changes layout");
    require(layout.objects.size() == 3, "editor paste adds object");
    require(layout.objects[2].id != layout.objects[0].id, "editor paste generates id");
    require(editor.selection.size() == 1 && editor.selection[0] == 2, "editor paste selects copy");
}

void test_editor_handles_oversized_rects() {
    glayout::Layout layout = make_editor_layout();
    layout.objects[0].rect = glayout::Rect{0.1f, 0.1f, 1.2f, 1.1f};

    glayout::EditorState editor;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_select_single(editor, 0);
    glayout::editor_begin_frame(editor, layout,
                                glayout::EditorInput{
                                    0.0f,
                                    0.0f,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    true,
                                },
                                viewport);
    glayout::editor_begin_frame(editor, layout,
                                glayout::EditorInput{
                                    0.0f,
                                    0.0f,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    false,
                                    true,
                                },
                                viewport);

    require(layout.objects.size() == 3, "oversized paste adds object");
    require(nearly_equal(layout.objects[2].rect.x, 0.0f), "oversized paste clamps x safely");
    require(nearly_equal(layout.objects[2].rect.y, 0.0f), "oversized paste clamps y safely");
}

void test_editor_overlay_data() {
    glayout::Layout layout = make_editor_layout();
    glayout::EditorState editor;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_select_single(editor, 0);
    std::vector<glayout::OverlayObject> objects =
        glayout::editor_collect_overlay_objects(editor, layout, viewport);
    require(objects.size() == 2, "overlay object count");
    require(objects[0].selected, "overlay selected object");
    require(objects[0].primary, "overlay primary object");
    require(nearly_equal(objects[0].rect.x, 10.0f), "overlay object x");

    std::vector<glayout::OverlayHandle> handles =
        glayout::editor_collect_overlay_handles(editor, layout, viewport);
    require(handles.size() == 8, "overlay handles for selected object");
    require(handles[0].object_index == 0, "overlay handle object index");

    glayout::editor_add_to_selection(editor, 1);
    glayout::Rect bounds;
    require(glayout::editor_selection_bounds(editor, layout, bounds), "selection bounds exists");
    require(nearly_equal(bounds.x, 0.1f), "selection bounds x");
    require(nearly_equal(bounds.y, 0.1f), "selection bounds y");
    require(nearly_equal(bounds.w, 0.7f), "selection bounds w");
    require(nearly_equal(bounds.h, 0.7f), "selection bounds h");

    handles = glayout::editor_collect_overlay_handles(editor, layout, viewport);
    require(handles.size() == 8, "overlay handles for selected group");
    require(handles[0].object_index == -1, "overlay group handle index");
    require(handles[0].group, "overlay group handle flag");
}

void test_editor_group_resize() {
    glayout::Layout layout = make_editor_layout();
    glayout::EditorState editor;
    editor.snap_enabled = false;
    glayout::Viewport viewport{0.0f, 0.0f, 100.0f, 100.0f};

    glayout::editor_select_single(editor, 0);
    glayout::editor_add_to_selection(editor, 1);

    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{80.0f, 80.0f, true}, viewport);
    glayout::EditorFrameResult drag_result = glayout::editor_begin_frame(
        editor, layout, glayout::EditorInput{90.0f, 90.0f, true}, viewport);
    glayout::editor_begin_frame(editor, layout, glayout::EditorInput{90.0f, 90.0f, false},
                                viewport);

    require(drag_result.changed, "editor group resize changes layout");
    require(layout.objects[0].rect.w > 0.22f, "editor group resize first width");
    require(layout.objects[1].rect.w > 0.22f, "editor group resize second width");
    require(layout.objects[1].rect.x > 0.66f, "editor group resize second x");
    require(editor.undo_stack.size() == 2, "editor group resize commits undo history");
}

} // namespace

int main() {
    test_rect_mapping();
    test_matching();
    test_layout_store();
    test_parse_write();
    test_demo_layout_file();
    test_replace_helpers();
    test_editor_drag_and_undo();
    test_editor_resize_delete_and_save_request();
    test_editor_copy_paste();
    test_editor_handles_oversized_rects();
    test_editor_overlay_data();
    test_editor_group_resize();

    std::cout << "glayout_core_tests passed\n";
    return 0;
}
