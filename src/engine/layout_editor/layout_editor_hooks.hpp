#pragma once

// Notifies the layout editor which layout ID/resolution the game just rendered.
// Safe to call even if the editor is inactive; it caches the info for later.
void layout_editor_notify_active_layout(int layout_id,
                                        int resolution_width,
                                        int resolution_height);
