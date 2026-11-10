#include "trail.h"
#include "ui.h"
#include "raylib.h"
#include "rlgl.h"
#include <cmath>
#include <deque>
#include <vector>

namespace ui {

namespace {

struct Dot { Vector2 p; float age; };
std::deque<Dot> gDots;

const float kLife = 0.22f;      // seconds a point of the trail lasts
const float kWidth = 5.0f;      // at the head, in canvas units

Vector2 CatmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    auto f = [&](float a, float b, float c, float d) {
        return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
    };
    return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
}

// One ribbon along the points, `width` wide at the head and thin at the tail.
void Ribbon(const std::vector<Vector2>& pts, const std::vector<float>& life, float width, float alpha) {
    size_t n = pts.size();
    if (n < 2) return;
    std::vector<Vector2> left(n), right(n);
    std::vector<Color> colour(n);
    for (size_t i = 0; i < n; i++) {
        Vector2 a = pts[i > 0 ? i - 1 : i], b = pts[i + 1 < n ? i + 1 : i];
        float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
        Vector2 nrm = len > 0.0001f ? Vector2{-dy / len, dx / len} : Vector2{0, 0};
        float t = life[i];                                   // 0 = tail, 1 = head
        float w = width * (0.12f + 0.88f * std::pow(t, 1.3f)) / 2;
        left[i] = {pts[i].x + nrm.x * w, pts[i].y + nrm.y * w};
        right[i] = {pts[i].x - nrm.x * w, pts[i].y - nrm.y * w};
        colour[i] = Alpha(Mix(col::accent2, col::accent, t), alpha * std::pow(t, 1.6f));
    }
    // Plain white texture: otherwise the batch keeps the last texture used
    // (usually the font's) and the triangles come out see-through.
    rlSetTexture(rlGetTextureIdDefault());
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    auto v = [](Vector2 p, Color c) { rlColor4ub(c.r, c.g, c.b, c.a); rlTexCoord2f(0.5f, 0.5f); rlVertex2f(p.x, p.y); };
    for (size_t i = 0; i + 1 < n; i++) {
        v(left[i], colour[i]); v(right[i], colour[i]); v(left[i + 1], colour[i + 1]);
        v(right[i], colour[i]); v(right[i + 1], colour[i + 1]); v(left[i + 1], colour[i + 1]);
    }
    rlEnd();
    // raylib draws in batches: draw these now, while culling is still off,
    // or the triangles that face the other way are dropped.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlSetTexture(0);
}

} // namespace

void DrawCursorTrail(float dt, bool enabled) {
    for (Dot& d : gDots) d.age += dt;
    while (!gDots.empty() && gDots.front().age > kLife) gDots.pop_front();
    if (!enabled) { gDots.clear(); return; }

    // Outside the app area nothing new is added; what is there fades away.
    Vector2 m = Mouse();
    if (m.x < 0 || m.y < 0 || m.x > W || m.y > H) return;
    if (gDots.empty() || std::hypot(m.x - gDots.back().p.x, m.y - gDots.back().p.y) > 0.75f) gDots.push_back({m, 0});
    else gDots.back() = {m, 0};   // still: the head stays under the pointer
    if (gDots.size() < 2) return;

    // Smooth the corners between the points sampled each frame.
    std::vector<Vector2> pts;
    std::vector<float> life;
    for (size_t i = 0; i + 1 < gDots.size(); i++) {
        Vector2 p0 = gDots[i > 0 ? i - 1 : i].p, p1 = gDots[i].p, p2 = gDots[i + 1].p;
        Vector2 p3 = gDots[i + 2 < gDots.size() ? i + 2 : i + 1].p;
        for (int k = 0; k < 4; k++) {
            float t = k / 4.0f;
            pts.push_back(CatmullRom(p0, p1, p2, p3, t));
            float age = gDots[i].age + (gDots[i + 1].age - gDots[i].age) * t;
            life.push_back(1 - age / kLife);
        }
    }
    pts.push_back(gDots.back().p);
    life.push_back(1);

    Ribbon(pts, life, kWidth * 3.2f, 0.16f);   // soft glow
    Ribbon(pts, life, kWidth, 0.55f);          // the line itself
}

} // namespace ui
