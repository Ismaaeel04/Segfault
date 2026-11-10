// ============================================================================
// cursor.h  -  Mouse cursors drawn in the colours of the current theme.
// They are real system cursors (made with GLFW), not drawn inside the app, so
// they move with no delay. Builds that use a raylib installed on the system
// may not include GLFW: those fall back to the standard cursors.
// ============================================================================
#pragma once
#include "raylib.h"

namespace ui {

enum CursorKind { CURSOR_ARROW, CURSOR_HAND, CURSOR_IBEAM, CURSOR_COUNT };

// Call once per frame. Rebuilds the cursors when the theme has changed, or
// falls back to the standard OS cursors when `custom` is false.
void SetAppCursor(CursorKind kind, bool custom = true);
void UnloadCursors();
// The picture of a cursor, size x size pixels, in the current theme.
Image CursorImage(CursorKind kind, int size);

} // namespace ui
