// ============================================================================
// scrollbar.h  -  The geometry of a scrollbar: where the thumb is for a given
// scroll offset, and the reverse for dragging it. No raylib here, so the unit
// tests (scrollbar_tests) can include it directly.
// ============================================================================
#pragma once
#include <algorithm>

namespace ui {

constexpr float kScrollTrackPad = 4.0f;      // gap above and below the track
constexpr float kScrollMinThumb = 30.0f;

struct ScrollThumb { float top, height; };

inline float MaxScroll(float viewH, float contentH) { return std::max(0.0f, contentH - viewH); }

inline ScrollThumb ThumbFor(float viewTop, float viewH, float contentH, float scroll) {
    float trackH = std::max(0.0f, viewH - kScrollTrackPad * 2);
    float h = contentH > 0 ? trackH * viewH / contentH : trackH;
    h = std::min(std::max(h, kScrollMinThumb), trackH);   // short panels: never taller than the track
    float maxS = MaxScroll(viewH, contentH);
    float t = maxS > 0 ? std::clamp(scroll / maxS, 0.0f, 1.0f) : 0.0f;
    return {viewTop + kScrollTrackPad + (trackH - h) * t, h};
}

// The scroll offset that puts the top of the thumb at thumbTop.
inline float ScrollForThumb(float viewTop, float viewH, float contentH, float thumbTop) {
    float trackH = std::max(0.0f, viewH - kScrollTrackPad * 2);
    float free = trackH - ThumbFor(viewTop, viewH, contentH, 0).height;
    if (free <= 0) return 0;
    float t = std::clamp((thumbTop - viewTop - kScrollTrackPad) / free, 0.0f, 1.0f);
    return t * MaxScroll(viewH, contentH);
}

} // namespace ui
