#include "engine/layout_editor/layout_editor_history.hpp"

#include "engine/ui_layouts.hpp"

#include <vector>

namespace {

constexpr std::size_t kMaxHistoryEntries = 64;

struct LayoutSnapshot {
    int layout_id{0};
    int width{0};
    int height{0};
    std::vector<UIObject> objects;
};

std::vector<LayoutSnapshot> g_history;
int g_history_index = -1;
bool g_restoring = false;

LayoutSnapshot capture_snapshot(const UILayout& layout) {
    LayoutSnapshot snap;
    snap.layout_id = layout.id;
    snap.width = layout.resolution_width;
    snap.height = layout.resolution_height;
    snap.objects = layout.objects;
    return snap;
}

bool matches_tracked_layout(const UILayout& layout) {
    if (g_history.empty())
        return false;
    const LayoutSnapshot& snap = g_history.front();
    return snap.layout_id == layout.id &&
           snap.width == layout.resolution_width &&
           snap.height == layout.resolution_height;
}

void apply_snapshot(UILayout& layout, const LayoutSnapshot& snapshot) {
    layout.objects = snapshot.objects;
}

} // namespace

void layout_editor_history_reset(const UILayout& layout) {
    g_history.clear();
    g_history.push_back(capture_snapshot(layout));
    g_history_index = 0;
}

void layout_editor_history_commit(const UILayout& layout) {
    if (g_restoring)
        return;
    if (g_history.empty() || !matches_tracked_layout(layout)) {
        layout_editor_history_reset(layout);
        return;
    }
    // Truncate any redo branch.
    if (static_cast<std::size_t>(g_history_index + 1) < g_history.size())
        g_history.erase(g_history.begin() + g_history_index + 1, g_history.end());
    g_history.push_back(capture_snapshot(layout));
    ++g_history_index;
    if (g_history.size() > kMaxHistoryEntries) {
        g_history.erase(g_history.begin());
        --g_history_index;
    }
}

bool layout_editor_history_undo(UILayout& layout) {
    if (g_history.empty() || !matches_tracked_layout(layout))
        return false;
    if (g_history_index <= 0)
        return false;
    --g_history_index;
    g_restoring = true;
    apply_snapshot(layout, g_history[static_cast<std::size_t>(g_history_index)]);
    g_restoring = false;
    return true;
}

bool layout_editor_history_redo(UILayout& layout) {
    if (g_history.empty() || !matches_tracked_layout(layout))
        return false;
    if (static_cast<std::size_t>(g_history_index + 1) >= g_history.size())
        return false;
    ++g_history_index;
    g_restoring = true;
    apply_snapshot(layout, g_history[static_cast<std::size_t>(g_history_index)]);
    g_restoring = false;
    return true;
}

void layout_editor_history_shutdown() {
    g_history.clear();
    g_history_index = -1;
    g_restoring = false;
}
