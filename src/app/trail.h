// ============================================================================
// trail.h  -  A short, fading ribbon behind the mouse in the theme's colours.
// The pointer itself is a system cursor (cursor.h), so it never lags; only
// this trail is drawn by the app. It is gone about a fifth of a second after
// the mouse stops, and it can be switched off in the theme window.
// ============================================================================
#pragma once

namespace ui {

// Call once per frame, last thing before the frame is shown.
void DrawCursorTrail(float dt, bool enabled);

} // namespace ui
