// ============================================================================
// test_scrollbar.cpp  -  Console unit tests for the scrollbar geometry.
// Build target: scrollbar_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cmath>
#include <cstdio>
#include "../src/app/scrollbar.h"

using namespace ui;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

int main() {
    const float top = 100, viewH = 400, contentH = 2000;
    const float trackH = viewH - kScrollTrackPad * 2;

    printf("[thumb position]\n");
    {
        ScrollThumb first = ThumbFor(top, viewH, contentH, 0);
        ScrollThumb last = ThumbFor(top, viewH, contentH, contentH - viewH);
        CHECK(near(first.top, top + kScrollTrackPad));
        CHECK(near(last.top + last.height, top + viewH - kScrollTrackPad));
        CHECK(near(first.height, trackH * viewH / contentH));
        CHECK(near(ThumbFor(top, viewH, contentH, -50).top, first.top));       // out of range: stays on the track
        CHECK(near(ThumbFor(top, viewH, contentH, 99999).top, last.top));
    }

    printf("[dragging the thumb is the exact reverse]\n");
    for (float s : {0.0f, 1.0f, 250.0f, 800.0f, 1600.0f}) {
        ScrollThumb th = ThumbFor(top, viewH, contentH, s);
        CHECK(near(ScrollForThumb(top, viewH, contentH, th.top), s));
    }
    CHECK(near(ScrollForThumb(top, viewH, contentH, top - 500), 0));           // dragged past the ends
    CHECK(near(ScrollForThumb(top, viewH, contentH, top + 5000), contentH - viewH));

    printf("[very long content keeps a thumb you can grab]\n");
    {
        ScrollThumb th = ThumbFor(top, viewH, 1000000, 0);
        CHECK(near(th.height, kScrollMinThumb));
        CHECK(near(ScrollForThumb(top, viewH, 1000000, top + viewH), 1000000 - viewH));
    }

    printf("[short panels and empty content]\n");
    {
        ScrollThumb th = ThumbFor(top, 20, 500, 100);                          // track smaller than the minimum thumb
        CHECK(th.height <= 20 - kScrollTrackPad * 2 + 0.01f);
        CHECK(near(ScrollForThumb(top, 20, 500, top + 3), 0));                 // nowhere to move: no division by zero
        CHECK(near(ThumbFor(top, viewH, 100, 0).top, top + kScrollTrackPad));  // content fits
        CHECK(near(ScrollForThumb(top, viewH, 100, top + 50), 0));
        CHECK(near(ThumbFor(top, viewH, 0, 0).top, top + kScrollTrackPad));
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
