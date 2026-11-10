#include "cursor.h"
#include "theme.h"
#include "ui.h"
#include <algorithm>
#include <cmath>

#ifdef SEGFAULT_GLFW_CURSOR
// raylib bundles GLFW; these are the only GLFW functions used.
extern "C" {
typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWcursor GLFWcursor;
typedef struct GLFWimage { int width, height; unsigned char* pixels; } GLFWimage;
GLFWwindow* glfwGetCurrentContext(void);
GLFWcursor* glfwCreateCursor(const GLFWimage* image, int xhot, int yhot);
void glfwSetCursor(GLFWwindow* window, GLFWcursor* cursor);
void glfwDestroyCursor(GLFWcursor* cursor);
}
#endif

namespace ui {

namespace {

// Shapes are described on a 32 x 32 grid and drawn 4x larger, then shrunk,
// which smooths the edges.
const int kGrid = 32, kSuper = 4;

struct Canvas {
    Image img;
    float unit;   // pixels per grid step
};

void Tri(Canvas& c, Vector2 a, Vector2 b, Vector2 d, Vector2 off, Color col) {
    auto p = [&](Vector2 v) { return Vector2{(v.x + off.x) * c.unit, (v.y + off.y) * c.unit}; };
    // raylib fills counter-clockwise triangles; draw both windings to be safe.
    ImageDrawTriangle(&c.img, p(a), p(b), p(d), col);
    ImageDrawTriangle(&c.img, p(a), p(d), p(b), col);
}
void Box(Canvas& c, float x, float y, float w, float h, Vector2 off, Color col) {
    ImageDrawRectangleRec(&c.img, {(x + off.x) * c.unit, (y + off.y) * c.unit, w * c.unit, h * c.unit}, col);
}
void Dot(Canvas& c, float x, float y, float r, Vector2 off, Color col) {
    ImageDrawCircleV(&c.img, {(x + off.x) * c.unit, (y + off.y) * c.unit}, (int)std::lround(r * c.unit), col);
}

// One cursor shape, moved by `off` and grown by `grow` (for the outline).
void Shape(Canvas& c, CursorKind kind, Vector2 off, float grow, Color col) {
    const float g = grow;
    if (kind == CURSOR_ARROW) {
        // head and tail of a classic arrow; the outline comes from drawing it
        // again at offsets all around (see Draw)
        // head: a fan from the tip, with the notch where the tail joins
        const Vector2 tip = {2, 2}, bl = {2, 24}, nl = {7.5f, 19}, nr = {11, 17.5f}, rt = {18, 17.5f};
        Tri(c, tip, bl, nl, off, col);
        Tri(c, tip, nl, nr, off, col);
        Tri(c, tip, nr, rt, off, col);
        // tail
        Tri(c, nl, nr, {15, 26.5f}, off, col);
        Tri(c, nl, {15, 26.5f}, {11.5f, 28}, off, col);
        (void)g;
    } else if (kind == CURSOR_HAND) {
        Box(c, 10 - g, 4, 5 + 2 * g, 14, off, col);                 // pointing finger
        Dot(c, 12.5f, 4, 2.5f + g, off, col);
        Dot(c, 17.5f, 13, 2.4f + g, off, col);                      // folded fingers
        Dot(c, 21.5f, 14.5f, 2.3f + g, off, col);
        Dot(c, 25, 16.5f, 2.1f + g, off, col);
        Box(c, 15, 13, 10 + g, 6, off, col);
        Box(c, 9 - g, 15, 18 + 2 * g, 9, off, col);                 // palm
        Dot(c, 13, 24, 4 + g, off, col);
        Dot(c, 22, 24, 4.8f + g, off, col);
        Box(c, 12, 22, 11, 6.5f + g, off, col);
        Tri(c, {10, 17}, {4, 15.5f}, {10, 22.5f}, off, col);        // thumb
        Dot(c, 5, 16.3f, 2 + g, off, col);
    } else {
        Box(c, 15 - g, 5, 2 + 2 * g, 22, off, col);                 // text cursor
        Box(c, 11 - g, 3 - g, 10 + 2 * g, 2.5f + 2 * g, off, col);
        Box(c, 11 - g, 26.5f - g, 10 + 2 * g, 2.5f + 2 * g, off, col);
    }
}

void Draw(Canvas& c, CursorKind kind, Color fill, Color edge) {
    const float t = 1.4f;   // outline width, in grid steps
    // soft shadow, then the outline (the shape stamped all around), then the fill
    for (int i = 0; i < 12; i++) {
        float a = i * 6.2832f / 12;
        Shape(c, kind, {0.9f + std::cos(a) * t, 1.4f + std::sin(a) * t}, t, Color{0, 0, 0, 34});
    }
    for (int i = 0; i < 16; i++) {
        float a = i * 6.2832f / 16;
        Shape(c, kind, {std::cos(a) * t, std::sin(a) * t}, t, edge);
    }
    Shape(c, kind, {0, 0}, 0, fill);
}

Vector2 HotSpot(CursorKind kind) {
    if (kind == CURSOR_HAND) return {12.5f, 2};
    if (kind == CURSOR_IBEAM) return {16, 16};
    return {2, 2};
}

#ifdef SEGFAULT_GLFW_CURSOR
GLFWcursor* gCursors[CURSOR_COUNT] = {};
int gBuiltTheme = -1, gBuiltSize = 0, gShown = -1;
#endif
int gSystemShown = -1;

} // namespace

Image CursorImage(CursorKind kind, int size) {
    const Theme& t = Themes()[CurrentTheme()];
    Color fill = kind == CURSOR_IBEAM ? t.heading : t.accent;
    Color edge = t.light ? Color{255, 255, 255, 255} : Color{255, 255, 255, 255};
    if (kind == CURSOR_IBEAM) edge = t.light ? Color{255, 255, 255, 255} : Color{12, 15, 22, 255};
    // Transparent pixels carry the edge colour, so shrinking leaves no dark fringe.
    Canvas c{GenImageColor(size * kSuper, size * kSuper, Color{edge.r, edge.g, edge.b, 0}), (float)size * kSuper / kGrid};
    Draw(c, kind, fill, edge);
    ImageResize(&c.img, size, size);
    return c.img;
}

void SetAppCursor(CursorKind kind, bool custom) {
    if (!custom) {
        UnloadCursors();
        if (gSystemShown != kind) {
            SetMouseCursor(kind == CURSOR_HAND ? MOUSE_CURSOR_POINTING_HAND : kind == CURSOR_IBEAM ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT);
            gSystemShown = kind;
        }
        return;
    }
    gSystemShown = -1;
#ifdef SEGFAULT_GLFW_CURSOR
    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) return;
    float dpi = std::max(1.0f, GetWindowScaleDPI().x);
    int size = (int)std::lround(32 * dpi);
    if (gBuiltTheme != CurrentTheme() || gBuiltSize != size) {
        UnloadCursors();
        for (int k = 0; k < CURSOR_COUNT; k++) {
            Image img = CursorImage((CursorKind)k, size);
            GLFWimage g = {img.width, img.height, (unsigned char*)img.data};
            Vector2 hot = HotSpot((CursorKind)k);
            gCursors[k] = glfwCreateCursor(&g, (int)(hot.x * size / kGrid), (int)(hot.y * size / kGrid));
            UnloadImage(img);
        }
        gBuiltTheme = CurrentTheme();
        gBuiltSize = size;
        gShown = -1;
    }
    if (gShown != kind && gCursors[kind]) {
        glfwSetCursor(win, gCursors[kind]);
        gShown = kind;
    }
#else
    if (gSystemShown == kind) return;   // raylib makes a new cursor on every call
    SetMouseCursor(kind == CURSOR_HAND ? MOUSE_CURSOR_POINTING_HAND : kind == CURSOR_IBEAM ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT);
    gSystemShown = kind;
#endif
}

void UnloadCursors() {
#ifdef SEGFAULT_GLFW_CURSOR
    if (gBuiltTheme == -1 && gShown == -1) return;
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) glfwSetCursor(win, nullptr);
    for (GLFWcursor*& c : gCursors) { if (c) glfwDestroyCursor(c); c = nullptr; }
    gBuiltTheme = -1;
    gShown = -1;
#endif
}

} // namespace ui
