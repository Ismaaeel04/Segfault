// ============================================================================
// game_art.cpp  -  The Forgotten Pointer's drawings (see game_art.h).
// The game has its own palette: it doesn't follow the app's themes.
// ============================================================================
#include "scenes/game_art.h"
#include "app/ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace ui;

namespace fp {
namespace {

Color Hex(unsigned v, unsigned char a = 255) { return {(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, a}; }
Color Fade2(Color c, float a) { c.a = (unsigned char)std::clamp(c.a * a, 0.0f, 255.0f); return c; }
void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) { DrawTriangle(a, b, c, col); DrawTriangle(a, c, b, col); }
void Glow(Vector2 c, float r, Color col, float strength) {
    for (int i = 0; i < 16; i++) DrawCircleV(c, r * (1 - i / 16.0f), Fade2(col, strength / 16));
}

// A character's local drawing frame: x grows forward (mirrored when facing left), y up is negative.
struct Frame {
    Vector2 feet;
    float s;
    int face;
    Vector2 P(float x, float y) const { return {feet.x + face * x * s, feet.y + y * s}; }
    // a rounded box given in local units, whatever the facing
    void Box(float x, float y, float w, float h, float round, Color c) const {
        float x0 = face > 0 ? feet.x + x * s : feet.x - (x + w) * s;
        DrawRectangleRounded({x0, feet.y + y * s, w * s, h * s}, round, 8, c);
    }
    void Circle(float x, float y, float r, Color c) const { DrawCircleV(P(x, y), r * s, c); }
    void Ell(float x, float y, float rx, float ry, Color c) const { Vector2 p = P(x, y); DrawEllipse((int)p.x, (int)p.y, rx * s, ry * s, c); }
    void Line(float x0, float y0, float x1, float y1, float th, Color c) const { DrawLineEx(P(x0, y0), P(x1, y1), th * s, c); }
    void Triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color c) const { Tri(P(x0, y0), P(x1, y1), P(x2, y2), c); }
};

Color Flash(Color c, float k) { return k > 0 ? Mix(c, WHITE, std::min(1.0f, k)) : c; }

} // namespace

// ------------------------------------------------- outlined, shaded drawing ---
// The hero and the bosses are drawn in their own large units (a hero is about
// 340 tall) inside a scaled rlgl matrix, with dark outlines and light from the
// top left. K() applies the current hurt flash and fade to every colour.
namespace {

const Color kInk = {18, 15, 34, 255};
float gHurt = 0, gAlpha = 1;
Color K(Color c) { return Fade2(Flash(c, gHurt), gAlpha); }

// Starts drawing in local units: (0,0) lands on o, and x is mirrored when sx < 0.
// Mirrored triangles face the other way, so culling is off while they are drawn.
void BeginLocal(Vector2 o, float sx, float sy) {
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlTranslatef(o.x, o.y, 0);
    rlScalef(sx, sy, 1);
}
void EndLocal() {
    rlPopMatrix();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    gHurt = 0;
    gAlpha = 1;
}

Vector2 V(float x, float y) { return {x, y}; }
Vector2 Add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
Vector2 Mul(Vector2 a, float k) { return {a.x * k, a.y * k}; }
void Poly(const std::vector<Vector2>& p, Color fill) {
    Vector2 c = {0, 0};
    for (auto& q : p) c = Add(c, q);
    c = Mul(c, 1.0f / p.size());
    for (size_t i = 0; i < p.size(); i++) Tri(c, p[i], p[(i + 1) % p.size()], K(fill));
}
void PolyO(const std::vector<Vector2>& p, Color fill, float ow = 5) {
    for (size_t i = 0; i < p.size(); i++) { DrawLineEx(p[i], p[(i + 1) % p.size()], ow * 2, K(kInk)); DrawCircleV(p[i], ow, K(kInk)); }
    Poly(p, fill);
}
float Rnd(Rectangle r, float rad) { float m = std::fmin(r.width, r.height) / 2; return std::clamp(rad / m, 0.0f, 1.0f); }
void RR(Rectangle r, float rad, Color fill, float ow = 5) {
    Rectangle o = {r.x - ow, r.y - ow, r.width + 2 * ow, r.height + 2 * ow};
    DrawRectangleRounded(o, Rnd(o, rad + ow), 12, K(kInk));
    DrawRectangleRounded(r, Rnd(r, rad), 12, K(fill));
}
void OC(Vector2 c, float r, Color fill, float ow = 5) { DrawCircleV(c, r + ow, K(kInk)); DrawCircleV(c, r, K(fill)); }
void OLine(Vector2 a, Vector2 b, float th, Color fill, float ow = 5) {
    DrawLineEx(a, b, th + 2 * ow, K(kInk));
    DrawCircleV(a, th / 2 + ow, K(kInk));
    DrawCircleV(b, th / 2 + ow, K(kInk));
    DrawLineEx(a, b, th, K(fill));
    DrawCircleV(a, th / 2, K(fill));
    DrawCircleV(b, th / 2, K(fill));
}
void GlowK(Vector2 c, float r, Color col, float s) { Glow(c, r, Flash(col, gHurt), s * gAlpha); }

// Text can't go through the mirrored matrix (it would read backwards): it is
// drawn afterwards at the matching screen point, in a whole font size.
void LocalText(const char* s, Vector2 local, Vector2 o, float sx, float sy, float size, Color c, FontKind f) {
    float px = std::round(size * std::fabs(sy));
    if (px < 6) return;
    Vector2 w = {o.x + local.x * sx, o.y + local.y * sy};
    Vector2 m = Measure(s, px, f);
    Text(s, w.x - m.x / 2, w.y - px / 2, px, c, f);
}

} // namespace

// ---------------------------------------------------------------- the hero ---
void DrawHero(Vector2 feet, float scale, int face, float walk, float attack, float time, bool blink) {
    if (blink) return;
    const float k = scale * 0.44f;   // the drawing is ~340 units tall; the hero is ~150 on screen
    BeginLocal(feet, face * k, k);
    auto P = [](float x, float y) { return Vector2{x, y}; };
    auto R = [](float x, float y, float w, float h) { return Rectangle{x, y, w, h}; };
    const float ow = 4.5f;
    const Color armor = Hex(0x7b6cf6), armorD = Hex(0x5244d0), armorDD = Hex(0x3a2e9e), armorL = Hex(0xb9b0ff);
    const Color steel = Hex(0xe9e7f8), steelD = Hex(0x9b97ba), gold = Hex(0xffc14d), goldD = Hex(0xc98a1c);
    const Color cape = Hex(0x3b2575), capeD = Hex(0x271654), plume = Hex(0xff5fa2), plumeD = Hex(0xc0306f), visor = Hex(0x48e5ff);
    float p = attack < 0 ? -1 : std::min(1.0f, attack);   // the swing, 0..1
    float ext = p < 0 ? 0 : std::sin(p * 1.5708f);       // how far the arm reaches out
    float lean = p < 0 ? 0 : 10 * std::sin(p * 3.1416f);
    float step = std::sin(walk) * 12, bob = std::fabs(std::sin(walk)) * 5;
    float wave = std::sin(time * 2) * 6 - step * 0.5f;
    auto U = [&](float x, float y) { return P(x, y + bob); };   // the upper body bobs as it walks

    // cape, flowing behind
    PolyO({U(-30 - lean, -236), U(16 - lean, -238), U(4, -150), P(-22, -60), P(-40, -6), P(-102 + wave, -14), P(-124 + wave, -62), U(-98, -150), U(-62, -212)}, cape, ow);
    DrawLineEx(U(-48, -200), P(-84 + wave, -30), 7, K(capeD));
    DrawLineEx(U(-66, -190), P(-108 + wave, -50), 6, K(capeD));

    // back leg
    RR(R(-34 - step, -128, 26, 60), 10, armorD, ow);
    RR(R(-33 - step, -70, 24, 50), 9, armorD, ow);
    OC(P(-21 - step, -72), 10, armor, ow);
    PolyO({P(-38 - step, -24), P(-6 - step, -24), P(2 - step, -8), P(6 - step, 0), P(-40 - step, 0)}, Hex(0x2a2150), ow);

    // back shoulder and the shield
    OC(U(-36 + lean * 0.3f, -226), 20, armorD, ow);
    PolyO({U(-104, -218), U(-14, -218), U(-10, -150), U(-58, -78), U(-108, -150)}, gold, ow);
    Poly({U(-96, -210), U(-22, -210), U(-19, -152), U(-58, -92), U(-99, -152)}, Hex(0x7a3fe0));
    Poly({U(-96, -210), U(-58, -210), U(-58, -92), U(-99, -152)}, Hex(0x8f55ff));   // lit half

    // front leg
    RR(R(4 + step, -128, 28, 60), 10, armor, ow);
    RR(R(5 + step, -70, 26, 50), 9, armor, ow);
    DrawRectangleRounded(R(8 + step, -124, 8, 50), 1, 8, K(Fade2(armorL, 0.55f)));
    OC(P(18 + step, -72), 11, Hex(0xa99fff), ow);
    DrawCircleV(P(15 + step, -75), 4, K(Fade2(WHITE, 0.6f)));
    PolyO({P(0 + step, -24), P(32 + step, -24), P(48 + step, -10), P(50 + step, 0), P(0 + step, 0)}, Hex(0x2a2150), ow);

    // tassets and body
    PolyO({U(-42, -144), U(42, -144), U(48, -108), U(-48, -108)}, armorD, ow);
    DrawLineEx(U(0, -142), U(0, -110), 4, K(kInk));
    PolyO({U(-46 + lean, -242), U(46 + lean, -242), U(52 + lean * 0.5f, -200), U(40, -142), U(-40, -142), U(-52 + lean * 0.5f, -200)}, armor, ow);
    Poly({U(-40 + lean, -236), U(-6 + lean, -236), U(-14, -160), U(-36, -170)}, Fade2(armorL, 0.5f));
    Poly({U(30, -170), U(46 + lean * 0.5f, -200), U(38, -146), U(20, -146)}, Fade2(armorDD, 0.6f));
    OC(U(6 + lean * 0.6f, -196), 19, Hex(0x231a46), ow * 0.8f);
    GlowK(U(6 + lean * 0.6f, -196), 30, visor, 0.35f);
    RR(R(-44, -150 + bob, 88, 14), 5, Hex(0x3a2a60), ow * 0.8f);
    RR(R(-9, -153 + bob, 18, 20), 4, gold, ow * 0.8f);

    // head: the plume, the helmet, the glowing visor
    float hx = 4 + lean;
    PolyO({U(hx - 2, -318), U(hx + 14, -326), U(hx - 18, -344), U(hx - 62, -328), U(hx - 92 + wave * 0.6f, -290), U(hx - 64, -300), U(hx - 30, -312)}, plume, ow);
    DrawLineEx(U(hx - 16, -330), U(hx - 70 + wave * 0.6f, -300), 4, K(plumeD));
    OC(U(hx, -280), 42, armor, ow);
    DrawCircleV(U(hx - 14, -296), 14, K(Fade2(armorL, 0.55f)));
    DrawCircleSector(U(hx, -280), 42, 20, 120, 20, K(Fade2(armorDD, 0.45f)));
    RR(R(hx - 6, -294 + bob, 50, 30), 12, Hex(0x14112a), ow * 0.8f);
    GlowK(U(hx + 20, -280), 40, visor, 0.45f);
    DrawRectangleRounded(R(hx + 2, -284 + bob, 38, 8), 1, 8, K(visor));
    DrawRectangleRounded(R(hx + 6, -283 + bob, 14, 3), 1, 8, K(WHITE));
    RR(R(hx - 36, -254 + bob, 72, 10), 5, armorD, ow * 0.8f);

    // front shoulder
    OC(U(40 + lean, -226), 24, armor, ow);
    DrawCircleSector(U(40 + lean, -226), 24, 180, 300, 12, K(Fade2(armorL, 0.55f)));

    // the arm and the sword (its tip is a pointer's arrowhead)
    Vector2 sh = U(40 + lean, -222);
    Vector2 el = Add(sh, V(20 + 18 * ext, 46 - 30 * ext));
    Vector2 hand = Add(sh, V(40 + 30 * ext, 26 - 4 * ext));
    float ang = p < 0 ? -1.05f : -2.3f + 3.0f * ext;   // idle: up and forward; a swing comes over the head
    Vector2 d = {std::cos(ang), std::sin(ang)}, n = {-d.y, d.x};
    if (p >= 0 && p < 1) {   // the swing's arc of light
        for (int i = 0; i < 10; i++) {
            float a0 = ang - 1.3f + i * 0.13f;
            Vector2 dd = {std::cos(a0), std::sin(a0)};
            DrawLineEx(Add(hand, Mul(dd, 40)), Add(hand, Mul(dd, 175)), 16, Fade2(visor, (0.04f + i * 0.03f) * (1 - p * 0.5f)));
        }
    }
    OLine(sh, el, 22, armorD, ow);
    OLine(el, hand, 20, armor, ow);
    auto At = [&](float along, float side) { return Add(Add(hand, Mul(d, along)), Mul(n, side)); };
    OLine(At(-8, 0), At(-30, 0), 9, Hex(0x3a2a60), ow * 0.8f);   // grip
    OC(At(-36, 0), 9, gold, ow * 0.8f);                           // pommel
    PolyO({At(16, -9), At(140, -7), At(140, 7), At(16, 9)}, steel, ow);
    Poly({At(16, -9), At(140, -7), At(140, 0), At(16, 0)}, WHITE);
    DrawLineEx(At(24, 0), At(132, 0), 3, K(steelD));
    GlowK(At(155, 0), 46, visor, 0.6f);
    PolyO({At(132, -18), At(176, 0), At(132, 18), At(140, 0)}, visor, ow);
    Poly({At(140, -8), At(170, 0), At(140, 0)}, WHITE);
    OLine(At(12, -28), At(12, 28), 10, gold, ow * 0.8f);   // crossguard
    DrawCircleV(At(12, 0), 6, K(goldD));
    OC(hand, 13, armorD, ow);   // gauntlet
    EndLocal();
    // the {*} on the shield and on the chest
    LocalText("{*}", {-62, -180 + bob}, feet, face * k, k, 26, WHITE, F_MONOBOLD);
    LocalText("{*}", {6 + lean * 0.6f, -196 + bob}, feet, face * k, k, 22, visor, F_MONOBOLD);
}

// -------------------------------------------------------- Off-by-One Goblin ---
void DrawGoblin(Vector2 feet, float scale, int face, float walk, float hurt, float alpha) {
    Frame f{feet, scale, face};
    auto C = [&](unsigned v) { return Fade2(Flash(Hex(v), hurt), alpha); };
    float step = std::sin(walk) * 6, bob = std::fabs(std::sin(walk)) * 3;
    // legs
    f.Box(-16 + step, -26, 12, 26, 0.5f, C(0x2f7a3a));
    f.Box(4 - step, -26, 12, 26, 0.5f, C(0x2f7a3a));
    // body with a little tunic
    f.Ell(0, -40 + bob, 24, 22, C(0x56b84a));
    f.Box(-22, -44 + bob, 44, 18, 0.5f, C(0x7a4a2a));
    // head with long ears
    f.Circle(2, -78 + bob, 24, C(0x6fd15a));
    f.Triangle(-16, -88 + bob, -44, -110 + bob, -12, -72 + bob, C(0x6fd15a));
    f.Triangle(18, -90 + bob, 44, -116 + bob, 20, -72 + bob, C(0x6fd15a));
    f.Triangle(-18, -86 + bob, -36, -102 + bob, -14, -76 + bob, C(0xf29db0));
    f.Ell(-6, -90 + bob, 8, 5, Fade2(WHITE, 0.25f * alpha));
    // eyes and a grin
    f.Circle(-2, -80 + bob, 6.5f, C(0xffffff));
    f.Circle(14, -80 + bob, 6.5f, C(0xffffff));
    f.Circle(0, -79 + bob, 3, C(0x1a1030));
    f.Circle(16, -79 + bob, 3, C(0x1a1030));
    f.Line(-2, -64 + bob, 16, -66 + bob, 3, C(0x1a1030));
    // its sign: the "<=" that runs one step too far
    f.Line(30, -20, 30, -84 + bob, 4, C(0x8a5a32));
    f.Box(14, -104 + bob, 40, 24, 0.25f, C(0xe8d3a8));
    Vector2 sp = f.P(face > 0 ? 34 : 34, -100 + bob);
    TextCenter("<=", {sp.x - 20 * scale, sp.y - 2 * scale, 40 * scale, 18 * scale}, 15 * scale, Fade2(Hex(0xb02a10), alpha), F_MONOBOLD);
}

// ---------------------------------------------------- Infinite Loop Serpent ---
// A snake in the shape of an infinity sign, chasing its own tail. Drawn in
// units where the loop is 380 wide each way; R is the size on screen.
namespace {
Vector2 Lemni(float t, float a) {
    float s = std::sin(t), k = 1 + s * s;
    return {a * std::cos(t) / k, 1.75f * a * s * std::cos(t) / k};
}
} // namespace

void DrawSerpent(Vector2 c, float R, float time, float hurt, float alpha) {
    const float k = R / 300;
    const float a = 380;
    BeginLocal(c, k, k);
    gHurt = hurt;
    gAlpha = alpha;
    GlowK({0, 0}, a * 1.3f, Hex(0x3fd1c7), 0.22f);
    // loop arrows circling it
    for (int i = 0; i < 4; i++) {
        float ph = time * 0.6f + i * 1.5708f + 0.5f;
        Vector2 p = {std::cos(ph) * a * 1.18f, std::sin(ph) * a * 0.80f};
        Color col = Fade2(Hex(0x9ff5ea), 0.7f);
        DrawRing(p, 18, 25, 40, 320, 20, K(col));
        float e = 320 * DEG2RAD;
        Vector2 tip = {p.x + std::cos(e) * 21.5f, p.y + std::sin(e) * 21.5f};
        Tri({tip.x + 11, tip.y - 1}, {tip.x - 7, tip.y - 9}, {tip.x - 1, tip.y + 11}, K(col));
    }
    const Color teal = Hex(0x2fb5a6), tealD = Hex(0x17706a), tealDD = Hex(0x0f4f4b), tealL = Hex(0x9ff5ea), belly = Hex(0xe8d38a), purple = Hex(0x7b5cff);
    // the body: the tail in the upper right, round both loops to the head
    const float t0 = -1.0f, t1 = t0 - (6.2832f - 0.5f);
    const int N = 240;
    static Vector2 pts[N + 1];
    static float rad[N + 1];
    for (int i = 0; i <= N; i++) {
        float u = (float)i / N;
        Vector2 p = Lemni(t0 + (t1 - t0) * u, a), q = Lemni(t0 + (t1 - t0) * (u + 0.002f), a);
        Vector2 d = {q.x - p.x, q.y - p.y};
        float l = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
        float sway = std::sin(u * 26 - time * 5) * 7 * u * (1 - u) * 4;   // a ripple running along it
        pts[i] = {p.x - d.y / l * sway, p.y + d.x / l * sway};
        rad[i] = 9 + 40 * std::pow(u, 0.7f);
    }
    // the body is made of many overlapping circles: on screen they are small, so fewer sides do
    auto Dot = [k](Vector2 p, float r, Color col) { DrawCircleSector(p, r, 0, 360, r * k < 6 ? 10 : 16, col); };
    // one stretch as a smooth tube: outline, shade, body, shine, then its markings
    auto Tube = [&](int from, int to, int inkFrom) {
        for (int i = std::max(from, inkFrom); i <= to; i++) Dot(pts[i], rad[i] + 6, K(kInk));
        for (int i = from; i <= to; i++) Dot(pts[i], rad[i], K(tealDD));
        for (int i = from; i <= to; i++) Dot({pts[i].x - rad[i] * 0.12f, pts[i].y - rad[i] * 0.16f}, rad[i] * 0.84f, K(tealD));
        for (int i = from; i <= to; i++) Dot({pts[i].x - rad[i] * 0.2f, pts[i].y - rad[i] * 0.26f}, rad[i] * 0.66f, K(teal));
        for (int i = from; i <= to; i++) Dot({pts[i].x - rad[i] * 0.3f, pts[i].y - rad[i] * 0.4f}, rad[i] * 0.26f, K(Fade2(tealL, 0.9f)));
        for (int i = from + 4; i < to; i += 9) {
            Vector2 p = pts[i], q = pts[i + 1];
            Vector2 d = {q.x - p.x, q.y - p.y};
            float l = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
            d = {d.x / l, d.y / l};
            Vector2 n = {-d.y, d.x};
            float r = rad[i], dr = r * 0.42f;
            Vector2 m = {p.x - r * 0.12f, p.y - r * 0.16f};
            if ((i / 9) % 2 == 0) {   // a purple diamond on its back
                Tri(Add(m, Mul(d, dr * 1.3f)), Add(m, Mul(n, dr)), Add(m, Mul(d, -dr * 1.3f)), K(kInk));
                Tri(Add(m, Mul(d, dr * 1.3f)), Add(m, Mul(n, -dr)), Add(m, Mul(d, -dr * 1.3f)), K(kInk));
                Tri(Add(m, Mul(d, dr)), Add(m, Mul(n, dr * 0.72f)), Add(m, Mul(d, -dr)), K(purple));
                Tri(Add(m, Mul(d, dr)), Add(m, Mul(n, -dr * 0.72f)), Add(m, Mul(d, -dr)), K(purple));
            } else {                  // a band between scales
                DrawLineEx(Add(p, Mul(n, r * 0.85f)), Add(p, Mul(n, -r * 0.85f)), 3, K(Fade2(tealDD, 0.8f)));
            }
        }
    };
    // two halves, so the second pass through the middle crosses over the first
    Tube(0, N / 2 + 4, 0);
    Tube(N / 2, N, N / 2 + 4);

    // ---- the head, its open jaws round its own tail ----
    Vector2 ph = pts[N], tail = pts[0];
    Vector2 d = {tail.x - ph.x, tail.y - ph.y};
    float dist = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
    d = {d.x / dist, d.y / dist};
    Vector2 n = {d.y, -d.x};
    if (n.y < 0) n = {-n.x, -n.y};   // positive side = down
    float hs = std::clamp(dist / 125, 1.0f, 1.5f);
    auto H = [&](float along, float side) { return Add(Add(ph, Mul(d, along * hs)), Mul(n, side * hs)); };
    auto J = [&](float along, float side) {   // the lower jaw, swung open
        float x = along + 30, y = side, cs = std::cos(0.30f), sn = std::sin(0.30f);
        return H(x * cs - y * sn - 30, x * sn + y * cs);
    };
    PolyO({H(30, -10), H(112, -26), H(118, 0), H(112, 26), H(30, 10)}, Hex(0x4a0d26), 5);   // the mouth
    Poly({H(40, -6), H(104, -18), H(108, 0), H(104, 18), H(40, 6)}, Hex(0x8a1d45));
    const int tipN = N / 16;   // the tail's tip, in the mouth
    for (int i = 0; i <= tipN; i++) Dot(pts[i], rad[i] + 5, K(kInk));
    for (int i = 0; i <= tipN; i++) { Dot(pts[i], rad[i], K(teal)); Dot({pts[i].x - rad[i] * 0.3f, pts[i].y - rad[i] * 0.4f}, rad[i] * 0.3f, K(tealL)); }
    for (int i = 0; i < 3; i++) { float f = (float)i; PolyO({H(-30 + f * 22, -40 + f * 4), H(-50 + f * 20, -78 + f * 10), H(-10 + f * 22, -46 + f * 4)}, purple, 5); }   // spikes
    PolyO({J(-40, 4), J(124, 8), J(120, 18), J(96, 34), J(56, 44), J(10, 48), J(-30, 38)}, tealD, 6);
    Poly({J(-30, 10), J(110, 12), J(94, 28), J(10, 40)}, belly);
    for (int i = 0; i < 5; i++) { float f = (float)i; DrawLineEx(J(4 + f * 22, 12), J(-2 + f * 22, 40), 3, K(Fade2(Hex(0xb89f55), 0.9f))); }
    PolyO({H(-40, -4), H(-30, -44), H(10, -56), H(60, -48), H(100, -30), H(124, -14), H(124, -6)}, Hex(0x36c3b3), 6);
    Poly({H(-26, -38), H(10, -50), H(58, -42), H(96, -27), H(56, -24), H(-20, -14)}, Hex(0x6fe3d4));   // lit top
    DrawLineEx(H(14, -50), H(70, -42), 6, K(tealD));                                                    // brow ridge
    for (int i = 0; i < 2; i++) {   // fangs
        float x = 80 + i * 26.0f;
        Tri(H(x - 6, -7), H(x + 6, -7), H(x, 12), K(kInk));
        Tri(H(x - 4, -7), H(x + 4, -7), H(x, 9), K(WHITE));
        Tri(J(x - 6, 10), J(x + 6, 10), J(x + 2, -8), K(kInk));
        Tri(J(x - 4, 10), J(x + 4, 10), J(x + 2, -5), K(Hex(0xf2ecff)));
    }
    DrawCircleV(H(112, -16), 3 * hs, K(kInk));   // nostril
    Vector2 e = H(50, -30);                      // the eye, burning yellow
    GlowK(e, 46 * hs, Hex(0xfff07a), 0.7f);
    OC(e, 13 * hs, Hex(0xfff07a), 4);
    DrawCircleV(Add(e, Mul(Vector2{-1, -1}, 4 * hs)), 4 * hs, K(WHITE));
    DrawLineEx(Add(e, Mul(n, -9 * hs)), Add(e, Mul(n, 9 * hs)), 4.5f * hs, K(kInk));
    EndLocal();
    // the code that keeps it going, on a dark label inside the left loop
    float fs = std::max(12.0f, std::round(30 * k)), tw = Measure("for (;;)", fs, F_MONOBOLD).x + fs;
    Vector2 lc = {c.x - a * 0.56f * k, c.y};
    DrawRectangleRounded({lc.x - tw / 2, lc.y - fs * 0.8f, tw, fs * 1.6f}, 1, 12, Fade2(Hex(0x101024), 0.7f * alpha));
    TextCenter("for (;;)", {lc.x - tw / 2, lc.y - fs * 0.8f, tw, fs * 1.6f}, fs, Fade2(Hex(0x9ff5ea), alpha), F_MONOBOLD);
}

void DrawLoopRing(Vector2 c, float r, float time) {
    Glow(c, r * 1.8f, Hex(0x3fd1c7), 0.35f);
    DrawRing(c, r * 0.62f, r, time * 400, time * 400 + 300, 24, Hex(0x2bb5a8));
    DrawCircleV(c, r * 0.22f, Hex(0x9ff5ea));
}

// ------------------------------------------------------ Out-of-Bounds Imp ---
// A little devil that hops at you, its fork's tines shaped like [ ], and over
// its head the index it reached for: always one past the end.
void DrawImp(Vector2 feet, float scale, int face, float hop, float hurt, float alpha, int index, float time) {
    const float k = scale * 0.42f;
    // squashed on the ground just before a jump, stretched in the air
    float sq = hop > 0.05f ? 1 + 0.10f * std::min(1.0f, hop) : 1 - 0.06f * (0.5f + 0.5f * std::sin(time * 9));
    BeginLocal(feet, face * k / std::sqrt(sq), k * sq);
    gHurt = hurt;
    gAlpha = alpha;
    const Color skin = Hex(0xff6b4a), skinD = Hex(0xc23b2a), skinL = Hex(0xffa083), wing = Hex(0x7a1f3a), horn = Hex(0xffe0a8);
    const Color fork = Hex(0xffc14d), eye = Hex(0xfff07a);
    float flap = std::sin(time * (hop > 0.05f ? 22 : 6)) * (hop > 0.05f ? 14 : 5);
    float tuck = hop > 0.05f ? 10.0f : 0.0f;
    // the tail, ending in an arrowhead
    DrawSplineSegmentBezierCubic({-26, -64}, {-80, -50}, {-96, -120}, {-120, -128}, 18, K(kInk));
    DrawSplineSegmentBezierCubic({-26, -64}, {-80, -50}, {-96, -120}, {-120, -128}, 9, K(skinD));
    PolyO({{-142, -140}, {-112, -144}, {-120, -118}}, skinD, 4);
    // wings behind
    PolyO({{-20, -118}, {-86, -150 - flap}, {-74, -116 - flap * 0.5f}, {-92, -96 - flap * 0.3f}, {-40, -92}}, wing, 4.5f);
    DrawLineEx({-24, -112}, {-80, -144 - flap}, 3, K(Fade2(Hex(0xc0306f), 0.8f)));
    // legs
    OLine({-14, -50}, {-18, -14 + tuck}, 16, skinD, 4.5f);
    OLine({16, -50}, {20, -14 + tuck}, 16, skin, 4.5f);
    OC({-14, -10 + tuck}, 12, skinD, 4.5f);
    OC({26, -10 + tuck}, 12, skin, 4.5f);
    // the body
    OC({0, -84}, 46, skin, 5);
    DrawCircleV({10, -74}, 28, K(skinL));
    DrawCircleV({-16, -100}, 12, K(Fade2(WHITE, 0.35f)));
    // the fork, held out in front
    OLine({62, -16}, {62, -214}, 9, Hex(0x8a5a32), 4);
    PolyO({{56, -236}, {68, -236}, {68, -196}, {56, -196}}, fork, 3.5f);   // three tines and the crossbar
    PolyO({{40, -238}, {48, -238}, {48, -196}, {40, -196}}, fork, 3.5f);
    PolyO({{76, -238}, {84, -238}, {84, -196}, {76, -196}}, fork, 3.5f);
    PolyO({{40, -202}, {84, -202}, {84, -190}, {40, -190}}, fork, 3.5f);
    OLine({24, -100}, {58, -104}, 15, skin, 4.5f);   // the arm
    OC({62, -104}, 11, skinD, 4.5f);
    // the head: ears, horns, a wide grin
    PolyO({{-34, -176}, {-78, -186}, {-40, -146}}, skin, 4.5f);
    PolyO({{40, -180}, {78, -196}, {48, -150}}, skin, 4.5f);
    PolyO({{-22, -200}, {-34, -246}, {-4, -206}}, horn, 4.5f);
    PolyO({{18, -204}, {26, -250}, {38, -200}}, horn, 4.5f);
    OC({4, -162}, 46, skin, 5);
    DrawCircleV({-12, -180}, 15, K(Fade2(WHITE, 0.3f)));
    DrawCircleSector({4, -162}, 46, 20, 110, 16, K(Fade2(skinD, 0.35f)));
    for (int i = 0; i < 2; i++) {   // eyes: yellow, slit pupils looking forward
        Vector2 e = {(float)(i == 0 ? -10 : 24), -172};
        OC(e, 11, eye, 3.5f);
        DrawEllipse((int)(e.x + 4), (int)e.y, 2.6f, 8, K(kInk));
    }
    PolyO({{-18, -146}, {38, -148}, {30, -132}, {8, -126}, {-10, -134}}, Hex(0x5a1030), 3.5f);   // grin
    Tri({-6, -146}, {0, -146}, {-3, -136}, K(WHITE));
    Tri({24, -147}, {30, -147}, {27, -137}, K(WHITE));
    EndLocal();
    // the index it reaches for, always out of bounds
    if (alpha > 0.05f) {
        std::string s = "[" + std::to_string(index) + "]";
        float px = std::round(std::max(12.0f, 15 * scale));
        Vector2 m = Measure(s, px, F_MONOBOLD);
        Vector2 at = {feet.x, feet.y - 290 * k * sq - px};
        Rectangle pill = {at.x - m.x / 2 - 8, at.y - 3, m.x + 16, px + 8};
        DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x2a0a14), 0.85f * alpha));
        DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Fade2(Hex(0xff5c7a), alpha));
        Text(s, at.x - m.x / 2, at.y + 1, px, Fade2(Hex(0xff8a9c), alpha), F_MONOBOLD);
    }
}

// --------------------------------------------------------- the Matrix Golem ---
// A giant of sandstone blocks: its body is a 3x3 array, its arms are rows of
// blocks. pose: 0 arms down .. 1 raised to slam. crumble: 0 whole .. 1 fallen apart.
namespace {
const Color kStone = Hex(0xc08a6a), kStoneD = Hex(0x8d5a48), kStoneL = Hex(0xe8b894), kStoneG = Hex(0xff9e4a);

// A stone block: outlined, lit on top, shaded below, sometimes a glowing crack.
void StoneBlock(Rectangle r, Color base, bool crack, float glow) {
    RR(r, 9, base, 5);
    DrawRectangleRounded({r.x + 6, r.y + 5, r.width - 12, 8}, 1, 6, K(Fade2(kStoneL, 0.8f)));
    DrawRectangleRounded({r.x + 6, r.y + r.height - 12, r.width - 12, 7}, 1, 6, K(Fade2(kStoneD, 0.7f)));
    if (crack) {
        Vector2 a = {r.x + r.width * 0.20f, r.y + 2}, b = {r.x + r.width * 0.28f, r.y + r.height * 0.22f}, c = {r.x + r.width * 0.15f, r.y + r.height * 0.38f};   // by the edge, clear of the number
        if (glow > 0) { DrawLineEx(a, b, 9, K(Fade2(kStoneG, 0.35f * glow))); DrawLineEx(b, c, 9, K(Fade2(kStoneG, 0.35f * glow))); }
        DrawLineEx(a, b, 3.5f, K(glow > 0 ? Mix(kInk, kStoneG, glow) : kInk));
        DrawLineEx(b, c, 3.5f, K(glow > 0 ? Mix(kInk, kStoneG, glow) : kInk));
    }
}
} // namespace

void DrawGolem(Vector2 feet, float scale, float time, float pose, float hurt, float alpha, float crumble) {
    const float k = scale * 0.68f;
    BeginLocal(feet, k, k);
    gHurt = hurt;
    gAlpha = alpha;
    const float breathe = std::sin(time * 1.6f) * 4 * (1 - crumble);
    int n = 0;   // which block: each flies its own way when it crumbles
    auto Off = [&](float bottom) {
        n++;
        float dir = std::sin(n * 12.9898f) > 0 ? 1.0f : -1.0f, sp = 40 + std::fmod(n * 37.0f, 90.0f);
        float fall = std::min(-bottom, crumble * crumble * 520);   // down to the ground, no further
        return Vector2{dir * sp * crumble, fall};
    };
    auto Moved = [&](Rectangle r) { Vector2 o = Off(r.y + r.height); return Rectangle{r.x + o.x, r.y + o.y, r.width, r.height}; };
    GlowK({0, -260}, 300, kStoneG, 0.18f * (1 - crumble));
    // legs
    for (int side = -1; side <= 1; side += 2) {
        StoneBlock(Moved({side * 54.0f - 34, -124, 68, 58}), kStoneD, false, 0);
        StoneBlock(Moved({side * 54.0f - 40, -64, 80, 64}), kStone, side > 0, 0);
    }
    // a belt plate with its declaration
    Rectangle belt = Moved({-118, -142, 236, 30});
    RR(belt, 8, Hex(0x5a3a30), 4);
    // the body: a 3x3 array of blocks; the middle one is its glowing heart
    const float bs = 74, gap = 7, gx = -(bs * 3 + gap * 2) / 2, gy = -150 - (bs * 3 + gap * 2) + breathe;
    Rectangle cells[9];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            Rectangle cell = Moved({gx + c * (bs + gap), gy + r * (bs + gap), bs, bs});
            cells[r * 3 + c] = cell;
            bool heart = r == 1 && c == 1;
            StoneBlock(cell, heart ? Mix(kStone, kStoneG, 0.25f) : ((r + c) % 2 ? kStone : Mix(kStone, kStoneL, 0.2f)), heart || (r * 3 + c) % 4 == 1,
                       heart ? 0.8f + 0.2f * std::sin(time * 3) : 0);
            if (heart) GlowK({cell.x + bs / 2, cell.y + bs / 2}, 70, kStoneG, 0.55f * (1 - crumble));
        }
    // arms: rows of blocks from the shoulders, hanging, or raised over the head
    for (int side = -1; side <= 1; side += 2) {
        Vector2 sh = {side * 150.0f, gy + 20};
        for (int i = 0; i < 3; i++) {
            float down = sh.y + 18 + i * 62, up = sh.y - 40 - i * 62;
            float y = down + (up - down) * pose;
            float x = sh.x - side * pose * (10 + i * 22);
            float s = i == 2 ? 70.0f : 58.0f;   // the fist is bigger
            StoneBlock(Moved({x - s / 2, y - s / 2, s, s}), i == 2 ? kStoneD : kStone, i == 1, 0);
        }
    }
    // the head: one heavy block, two glowing eyes under a brow
    Rectangle head = Moved({-54, gy - 96, 108, 88});
    StoneBlock(head, kStone, false, 0);
    DrawLineEx({head.x + 14, head.y + 32}, {head.x + 94, head.y + 32}, 8, K(kStoneD));
    for (int i = 0; i < 2; i++) {
        Vector2 e = {head.x + 34 + i * 40, head.y + 50};
        GlowK(e, 30, Hex(0x48e5ff), 0.8f * (1 - crumble));
        DrawRectangleRounded({e.x - 12, e.y - 6, 24, 12}, 1, 6, K(crumble > 0.3f ? Hex(0x3a3a4a) : Hex(0x48e5ff)));
    }
    EndLocal();
    // the numbers it holds, and its declaration
    const int vals[9] = {3, 1, 4, 1, 5, 9, 2, 6, 5};
    for (int i = 0; i < 9; i++)
        LocalText(std::to_string(vals[i]).c_str(), {cells[i].x + bs / 2, cells[i].y + bs / 2 + 4}, feet, k, k, 40,
                  Fade2(i == 4 ? Hex(0xfff2d8) : Hex(0x5a3428), alpha), F_MONOBOLD);
    LocalText("int m[3][3]", {belt.x + belt.width / 2, belt.y + 16}, feet, k, k, 20, Fade2(Hex(0xffd8b0), alpha), F_MONOBOLD);
}

// A block the Golem drops: a stone with a number, falling from the sky.
void DrawStoneBlock(Vector2 center, float size, int value, float alpha) {
    const float k = size / 74;
    BeginLocal(center, k, k);
    gAlpha = alpha;
    StoneBlock({-37, -37, 74, 74}, kStone, value % 3 == 0, 0);
    EndLocal();
    LocalText(std::to_string(value).c_str(), {0, 4}, center, k, k, 40, Fade2(Hex(0x5a3428), alpha), F_MONOBOLD);
}

// Where a block will land: a shadow that grows as it comes, a red ring and its index.
void DrawDropMarker(float x, float time, float progress, const char* label) {
    float w = 30 + 34 * progress;
    DrawEllipse((int)x, (int)kGround + 4, w, 9 + 4 * progress, Fade2(Hex(0x2a1410), 0.25f + 0.35f * progress));
    float pulse = 0.55f + 0.45f * std::sin(time * 18);
    DrawEllipseLines((int)x, (int)kGround + 4, w + 6, 12 + 4 * progress, Fade2(Hex(0xff3d5a), pulse));
    Vector2 m = Measure(label, 14, F_MONOBOLD);
    Rectangle pill = {x - m.x / 2 - 7, kGround + 18, m.x + 14, 20};
    DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x2a0a14), 0.8f));
    Text(label, x - m.x / 2, kGround + 20, 14, Fade2(Hex(0xff8a9c), 0.6f + 0.4f * pulse), F_MONOBOLD);
}

// The Golem's slam, running along the ground: rocks bursting up. Jump over it.
void DrawShockwave(Vector2 at, float time) {
    for (int i = 0; i < 3; i++) {
        float x = at.x + (i - 1) * 22, h = 40 + 18 * std::sin(time * 30 + i * 2.1f) + (i == 1 ? 18 : 0);
        Tri({x - 16, at.y + 2}, {x + 16, at.y + 2}, {x + 3, at.y - h - 5}, kInk);
        Tri({x - 11, at.y}, {x + 11, at.y}, {x + 2, at.y - h}, i == 1 ? kStone : kStoneD);
        Tri({x - 4, at.y - h * 0.5f}, {x + 3, at.y - h * 0.5f}, {x + 2, at.y - h}, kStoneL);
    }
    for (int i = 0; i < 5; i++) {   // dust
        float a = time * 6 + i * 1.3f;
        DrawCircleV({at.x - 30 - i * 10.0f, at.y - 8 - std::fmod(a * 7, 30.0f)}, 9.0f - i, Fade2(Hex(0xe8c8a8), 0.5f - i * 0.08f));
    }
}

// ------------------------------------------------------------------ story ---
void DrawAda(Vector2 feet, float s, float time) {
    Frame f{feet, s, 1};
    float sway = std::sin(time * 1.5f) * 2;
    // long hair behind
    f.Box(-30, -150, 60, 96, 0.6f, Hex(0x3a2a6b));
    // dress
    f.Triangle(-50, 0, 50, 0, 0, -100, Hex(0x2bb5a8));
    f.Triangle(-50, 0, 0, -100, -18 + sway, -100, Hex(0x2bb5a8));
    f.Box(-22, -108, 44, 40, 0.5f, Hex(0x35c9bb));
    f.Box(-50, -8, 100, 8, 0.6f, Hex(0x1f8f85));
    // head, face, crown with { }
    f.Circle(0, -130, 24, Hex(0xf6d2b8));
    f.Box(-26, -158, 52, 18, 0.6f, Hex(0x3a2a6b));
    f.Circle(-8, -130, 3, Hex(0x2a1d45));
    f.Circle(8, -130, 3, Hex(0x2a1d45));
    f.Line(-6, -119, 6, -119, 2.5f, Hex(0xc0566a));
    f.Box(-20, -172, 40, 14, 0.3f, Hex(0xffc14d));
    Vector2 cr = f.P(0, -170);
    TextCenter("{ }", {cr.x - 30 * s, cr.y - 2 * s, 60 * s, 14 * s}, 12 * s, Hex(0x7a4a00), F_MONOBOLD);
}

// The SEGFAULT: a giant of broken memory blocks with a crashed monitor for a
// head, a snapped pointer for a spear and a linked list dragged like a chain.
// Drawn in units around (600, 720); s is the size on screen.
void DrawSegfault(Vector2 c, float s, float time) {
    const float k = s * 0.74f;
    const Vector2 o = {c.x - 600 * k, c.y - 720 * k};   // where local (0, 0) lands
    BeginLocal(o, k, k);
    const Color body = Hex(0x5a1d46), bodyD = Hex(0x3e1231), bodyL = Hex(0x8a2f6a), core = Hex(0xff3d5a), horn = Hex(0xff8a4c), hornL = Hex(0xffc07a);
    GlowK({600, 600}, 420, core, 0.25f);
    // legs: stacked memory blocks
    for (int side = -1; side <= 1; side += 2) {
        float x = 600 + side * 92.0f - 50;
        for (int i = 0; i < 3; i++) {
            Rectangle b = {x - i * 3.0f * side, 830.0f + i * 24, 100, 34};
            RR(b, 8, i == 1 ? bodyD : body, 5);
            DrawRectangleRounded({b.x + 6, b.y + 4, b.width - 12, 6}, 1, 8, K(Fade2(bodyL, 0.7f)));
        }
        RR({x - 12, 900, 124, 30}, 10, bodyD, 5);   // the foot
    }
    // the torso: plates of memory, some cracked, some missing
    PolyO({{372, 520}, {828, 520}, {770, 838}, {430, 838}}, body, 7);
    Poly({{380, 528}, {600, 528}, {600, 830}, {438, 830}}, Fade2(bodyL, 0.18f));
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 5; col++) {
            float w = 74, h = 62, x0 = 420 + row * 8 + col * (w + 6) - row * 2, y0 = 548 + row * (h + 8);
            if (x0 + w > 790 - row * 14 || x0 < 400 + row * 14) continue;
            bool hole = (row * 5 + col) % 7 == 3, hot = (row * 3 + col) % 5 == 1;
            Rectangle pl = {x0, y0, w, h};
            if (hole) {   // a missing block: red light from inside
                DrawRectangleRounded(pl, 0.2f, 6, K(Hex(0x14060f)));
                GlowK({x0 + w / 2, y0 + h / 2}, 34, core, 0.6f);
                continue;
            }
            DrawRectangleRounded(pl, 0.2f, 6, K(hot ? Mix(body, core, 0.45f) : Mix(body, bodyL, 0.25f)));
            DrawRectangleRoundedLinesEx(pl, 0.2f, 6, 2.5f, K(Fade2(kInk, 0.7f)));
            DrawRectangleRounded({x0 + 6, y0 + 5, w - 12, 6}, 1, 6, K(Fade2(WHITE, 0.10f)));
            if ((row + col) % 3 == 0) {   // a crack
                DrawLineEx({x0 + 12, y0 + 10}, {x0 + 32, y0 + 32}, 3, K(kInk));
                DrawLineEx({x0 + 32, y0 + 32}, {x0 + 26, y0 + 54}, 3, K(kInk));
            }
        }
    // the core: the heap, glowing
    float pulse = 0.85f + 0.15f * std::sin(time * 3);
    GlowK({600, 760}, 120 * pulse, core, 0.9f);
    OC({600, 760}, 34, Hex(0xffd0d8), 5);
    DrawRing({600, 760}, 40, 46, 0, 360, 32, K(Fade2(core, 0.8f)));

    // the left arm, hanging, dragging a linked list like a chain
    OC({392, 552}, 48, bodyD, 6);
    OLine({392, 566}, {326, 690}, 90, bodyD, 6);
    OLine({326, 690}, {304, 792}, 80, body, 6);
    DrawCircleSector({392, 552}, 48, 180, 290, 16, K(bodyL));
    for (int i = 0; i < 3; i++) {   // claws
        Vector2 b = {272.0f + i * 26, 818};
        Tri({b.x - 9, b.y}, {b.x + 9, b.y}, {b.x - 2, b.y + 46}, K(kInk));
        Tri({b.x - 5, b.y + 2}, {b.x + 5, b.y + 2}, {b.x - 1, b.y + 38}, K(horn));
    }
    OC({302, 806}, 46, body, 6);
    DrawCircleV({290, 794}, 12, K(Fade2(bodyL, 0.8f)));
    Vector2 nodeAt[3];
    Vector2 from = {286, 832};
    for (int i = 0; i < 3; i++) {
        Rectangle node = {196.0f - i * 40, 846.0f + i * 46, 112, 40};
        Vector2 to = {node.x + 84, node.y};
        DrawLineEx(from, to, 9, K(kInk));
        DrawLineEx(from, to, 4, K(Hex(0xff9e4a)));
        RR(node, 7, Hex(0x2a1430), 4);
        DrawRectangle((int)node.x + 70, (int)node.y, 4, (int)node.height, K(kInk));
        DrawCircleV({node.x + 92, node.y + 20}, 7, K(Hex(0xff9e4a)));
        nodeAt[i] = {node.x + 35, node.y + 20};
        from = {node.x + 92, node.y + 20};
    }
    DrawLineEx(from, {from.x + 40, from.y}, 4, K(Hex(0xff9e4a)));   // ... and the last next is NULL

    // the right arm, raised, holding a snapped pointer like a spear
    const Color shaft = Hex(0xff9e4a), shaftL = Hex(0xffd29a);
    OLine({896, 900}, {918, 690}, 18, shaft, 5);   // the lower piece, bent off the line
    OLine({924, 640}, {976, 150}, 18, shaft, 5);   // the upper piece
    DrawLineEx({922, 630}, {972, 170}, 4, K(shaftL));
    for (int i = 0; i < 7; i++) {   // sparks where it snapped
        float ang = i * 0.9f + 0.3f + time * 2, r = 22 + (i % 3) * 9.0f;
        DrawLineEx({920, 665}, {920 + std::cos(ang) * r, 665 + std::sin(ang) * r}, 3, K(Hex(0xffe08a)));
    }
    GlowK({920, 665}, 40, Hex(0xffb050), 0.6f);
    PolyO({{976, 82}, {1010, 168}, {976, 152}, {942, 168}}, shaft, 5);   // the arrowhead
    Poly({{976, 96}, {976, 150}, {950, 162}}, shaftL);
    GlowK({976, 120}, 60, Hex(0xffb050), 0.35f);
    OC({808, 552}, 48, bodyD, 6);
    OLine({808, 566}, {898, 500}, 90, bodyD, 6);
    OLine({898, 500}, {930, 410}, 80, body, 6);
    DrawCircleSector({808, 552}, 48, 180, 290, 16, K(bodyL));
    OC({944, 400}, 46, body, 6);   // the fist round the shaft
    DrawCircleV({932, 388}, 12, K(Fade2(bodyL, 0.8f)));
    DrawLineEx({918, 384}, {968, 392}, 4, K(kInk));
    DrawLineEx({918, 404}, {968, 412}, 4, K(kInk));

    // pixels coming off its shoulders, drifting up
    for (int i = 0; i < 30; i++) {
        bool left = i % 2;
        float x = (left ? 330 : 770) + std::fmod(i * 37.0f, 110.0f), y = 520 - std::fmod(i * 53.0f + time * 30, 240.0f);
        float sz = 6 + (i % 4) * 3.0f;
        DrawRectangleRec({x, y, sz, sz}, K(Fade2(i % 3 ? bodyL : core, std::max(0.0f, 0.8f - (520 - y) / 300))));
    }
    // the head: a crashed monitor showing a blue screen, horns, cables
    for (int i = 0; i < 4; i++)
        DrawSplineSegmentBezierCubic({470.0f + i * 70, 300}, {450.0f + i * 70, 230}, {390.0f + i * 90, 260}, {360.0f + i * 140, 200}, 7, K(Hex(0x2a1d35)));
    for (int side = -1; side <= 1; side += 2) {
        Vector2 a = {600 + side * 132.0f, 262}, b = {600 + side * 168.0f, 170}, c2 = {600 + side * 260.0f, 190}, e = {600 + side * 270.0f, 90};
        DrawSplineSegmentBezierCubic(a, b, c2, e, 46, K(kInk));
        DrawSplineSegmentBezierCubic(a, b, c2, e, 34, K(horn));
        DrawSplineSegmentBezierCubic({a.x - side * 6, a.y - 4}, {b.x - side * 6, b.y - 4}, {c2.x - side * 6, c2.y - 4}, {e.x - side * 4, e.y + 2}, 10, K(hornL));
        OC(e, 17, horn, 6);
    }
    RR({540, 470, 120, 60}, 10, bodyD, 6);            // the neck
    RR({438, 250, 324, 236}, 26, Hex(0x2c2a3c), 7);   // the monitor
    DrawRectangleRounded({446, 256, 308, 40}, 0.6f, 8, K(Fade2(WHITE, 0.07f)));
    DrawRectangleRounded({468, 274, 264, 178}, 0.12f, 10, K(Hex(0x1f50c8)));
    for (int y = 0; y < 178; y += 6) DrawRectangle(468, 274 + y, 264, 2, K(Fade2(BLACK, 0.12f)));
    // a flicker of glitch across the screen now and then
    if (std::fmod(time, 3.1f) < 0.12f) {
        DrawRectangle(468, 330, 264, 14, K(Fade2(Hex(0x00e5ff), 0.45f)));
        DrawRectangle(468, 352, 264, 8, K(Fade2(Hex(0xff2050), 0.45f)));
    }
    DrawCircleV({722, 470}, 6, K(core));   // the power light
    GlowK({722, 470}, 20, core, 0.7f);
    EndLocal();
    // the words on it, drawn the right way round
    auto T = [&](const char* str, float x, float y, float size, Color col, FontKind f) { LocalText(str, {x, y}, o, k, k, size, col, f); };
    float jitter = std::fmod(time, 3.1f) < 0.12f ? 6.0f : 0.0f;
    T(":(", 560 + jitter, 346, 92, WHITE, F_HEAVY);
    T("SEGMENTATION", 556, 420, 20, Fade2(WHITE, 0.9f), F_MONOBOLD);
    T("FAULT", 522, 440, 20, Fade2(WHITE, 0.9f), F_MONOBOLD);
    T("0xC0000005", 672, 430, 20, Fade2(WHITE, 0.75f), F_MONOBOLD);
    const char* vals[3] = {"42", "7", "13"};
    for (int i = 0; i < 3; i++) T(vals[i], nodeAt[i].x, nodeAt[i].y, 28, Hex(0xffd0d8), F_MONOBOLD);
    T("NULL", from.x + 82, from.y, 26, Hex(0xff5c7a), F_MONOBOLD);
}

void DrawCastle(Rectangle r, float time) {
    Color stone = Hex(0x2b2552), stoneL = Hex(0x3b3470);
    float base = r.y + r.height;
    // towers and walls
    const float tw[5] = {0.10f, 0.28f, 0.50f, 0.72f, 0.90f}, th[5] = {0.55f, 0.75f, 0.95f, 0.75f, 0.55f};
    DrawRectangleRec({r.x + r.width * 0.08f, base - r.height * 0.45f, r.width * 0.84f, r.height * 0.45f}, stone);
    for (int i = 0; i < 5; i++) {
        float w = r.width * 0.11f, x = r.x + r.width * tw[i] - w / 2, h = r.height * th[i];
        DrawRectangleRec({x, base - h, w, h}, i == 2 ? stoneL : stone);
        Tri({x - 6, base - h}, {x + w + 6, base - h}, {x + w / 2, base - h - w * 0.9f}, Hex(0x6d5ef0));
        // windows that blink like bytes
        for (int k = 0; k < 3; k++) {
            bool on = std::fmod(time * (0.7f + i * 0.13f) + k * 0.37f + i, 1.0f) < 0.6f;
            DrawRectangleRec({x + w / 2 - 5, base - h + 20 + k * 30, 10, 14}, on ? Hex(0xffc14d) : Hex(0x1a1630));
        }
    }
    // gate
    DrawRectangleRounded({r.x + r.width / 2 - 26, base - 60, 52, 60}, 0.5f, 8, Hex(0x14112a));
    TextCenter("0x0000", {r.x, base - r.height * 0.28f, r.width, 20}, 16, Hex(0x8f88b8), F_MONOBOLD);
}

void DrawTerminalPillar(Vector2 feet, float time, bool ready, bool near) {
    Color c = ready ? Hex(0x48e5ff) : Hex(0x4b4475);
    if (ready) Glow({feet.x, feet.y - 110}, 110 + (near ? 20 : 0), c, 0.30f + 0.15f * std::sin(time * 4));
    DrawRectangleRounded({feet.x - 34, feet.y - 150, 68, 150}, 0.2f, 8, Hex(0x1d1a33));
    DrawRectangleRounded({feet.x - 46, feet.y - 196, 92, 64}, 0.2f, 8, Hex(0x262142));
    DrawRectangleRounded({feet.x - 38, feet.y - 188, 76, 48}, 0.15f, 8, Hex(0x0b0a14));
    DrawRectangleRoundedLinesEx({feet.x - 46, feet.y - 196, 92, 64}, 0.2f, 8, 3, c);
    Text(">_", feet.x - 28, feet.y - 180, 22, c, F_MONOBOLD);
    if (ready && std::fmod(time, 1.0f) < 0.5f) DrawRectangle((int)feet.x + 4, (int)feet.y - 174, 12, 4, c);
}

// ------------------------------------------------------------ Stack Plains ---
namespace {
Texture2D gPlains = {}, gCanyon = {};
bool gArtTried = false;

Texture2D LoadBackdrop(const std::string& path) {
    Image img = LoadImage(path.c_str());
    if (!img.data) return {};   // no picture: that land is drawn by hand instead
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&tex);   // drawn smaller than it is: keep it smooth
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    return tex;
}
}

void LoadArt(const std::string& resourceDir) {
    if (gArtTried) return;
    gArtTried = true;
    gPlains = LoadBackdrop(resourceDir + "/backgrounds/stack_plains.png");
    gCanyon = LoadBackdrop(resourceDir + "/backgrounds/array_canyon.png");
}

// A painted backdrop, drifting slowly (parallax). It is drawn bigger than
// the screen plus all the drift a level needs, so on a 16:9 screen its edge
// never shows; wider screens get a mirrored copy, so the edges still meet.
// Its foot sits just above the ground line; the top of its sky is cut.
static void PaintedSky(const Texture2D& tex, float cam, Rectangle scr) {
    const float h = 1000, w = h * tex.width / tex.height, top = H - h;
    float off = std::fmod(cam * 0.06f, 2 * w);
    if (off < 0) off += 2 * w;
    for (int k = -1; k < 4; k++) {
        float x = scr.x - off + k * w;
        if (x > scr.x + scr.width || x + w < scr.x) continue;
        bool flip = ((k % 2) + 2) % 2 == 1;
        Rectangle src = {0, 0, (float)tex.width * (flip ? -1 : 1), (float)tex.height};
        DrawTexturePro(tex, src, {x, top, w + 1, h}, {0, 0}, 0, WHITE);
    }
}

void DrawStackPlains(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = gPlains.id != 0;
    if (painted) PaintedSky(gPlains, cam, scr);
    else {   // by hand: a dusk sky, a sun and layers of hills
        DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0x1b1640), Hex(0x6a3f8a));
        Glow({L + scr.width * 0.72f - cam * 0.02f, 210}, 200, Hex(0xffb36b), 0.55f);
        DrawCircleV({L + scr.width * 0.72f - cam * 0.02f, 210}, 64, Hex(0xffd29a));
        struct Layer { float speed, base, amp, freq; unsigned col; };
        const Layer layers[3] = {{0.15f, 470, 80, 0.004f, 0x3a2a6b}, {0.35f, 540, 55, 0.007f, 0x2f2a5e}, {0.6f, 600, 30, 0.011f, 0x24304f}};
        for (const Layer& ly : layers)
            for (float x = L; x < R; x += 6) {
                float wx = x + cam * ly.speed;
                float hh = ly.base - ly.amp * (0.6f * std::sin(wx * ly.freq) + 0.4f * std::sin(wx * ly.freq * 2.3f + 1.7f));
                DrawRectangle((int)x, (int)hh, 7, (int)(kGround - hh) + 1, Hex(ly.col));
            }
    }
    // floating stack frames: little piles of plates drifting in the sky (pale, like the clouds, by day)
    for (int i = 0; i < 7; i++) {
        float wx = i * 640.0f + 200;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4000 * 10, 4480.0f) + L - 400;
        if (x < L - 200 || x > R + 200) continue;
        float y = 150 + (i % 3) * 70 + std::sin(time * 0.8f + i) * 8;
        int n = 2 + i % 3;
        for (int k = 0; k < n; k++) {
            Rectangle pl = {x - 46 + k * 4, y - k * 16, 92 - k * 8, 13};
            if (painted) {
                DrawRectangleRounded(pl, 0.5f, 6, Fade2(Hex(0xf3f0ff), 0.42f + 0.08f * k));
                DrawRectangleRoundedLinesEx(pl, 0.5f, 6, 1.5f, Fade2(Hex(0x9d93d8), 0.45f));
            } else {
                DrawRectangleRounded(pl, 0.5f, 6, Fade2(Hex(0x8b7bff), 0.30f + 0.08f * k));
                DrawRectangleRoundedLinesEx(pl, 0.5f, 6, 1.5f, Fade2(Hex(0xc9c4ff), 0.35f));
            }
        }
    }
    // the ground: grass on top, rows of memory tiles below (in the picture's soft greens by day)
    const Color soil = painted ? Hex(0x3b5a55) : Hex(0x1a1430), grass = painted ? Hex(0x7cbf8c) : Hex(0x3fbf6a);
    const Color grassD = painted ? Hex(0x5a9a70) : Hex(0x2a8a4c), tile0 = painted ? Hex(0x4a6c66) : Hex(0x2a2148);
    const Color tile1 = painted ? Hex(0x42605b) : Hex(0x221b3d), tuft = painted ? Hex(0x95d3a2) : Hex(0x5ad87f);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 14, grass);
    DrawRectangle((int)L, (int)kGround + 14, (int)scr.width + 1, 6, grassD);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        for (int row = 0; row < 3; row++) {
            float y = kGround + 32 + row * 40, xo = (row % 2) * 32;
            DrawRectangleRounded({x + xo + 4, y, 56, 30}, 0.2f, 4, row == 0 ? tile0 : tile1);
        }
        Tri({x + 10, kGround + 2}, {x + 16, kGround - 10}, {x + 22, kGround + 2}, tuft);
        Tri({x + 40, kGround + 2}, {x + 44, kGround - 7}, {x + 50, kGround + 2}, tuft);
    }
}

// ------------------------------------------------------------ Array Canyon ---
void DrawArrayCanyon(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = gCanyon.id != 0;
    if (painted) PaintedSky(gCanyon, cam, scr);
    else {   // by hand: a warm sky and layers of mesas
        DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0xfde9bf), Hex(0xe9b48e));
        struct Layer { float speed, base, amp, freq; unsigned col; };
        const Layer layers[3] = {{0.15f, 420, 90, 0.003f, 0xd9a58e}, {0.35f, 500, 60, 0.006f, 0xc48a76}, {0.6f, 580, 34, 0.010f, 0xa86e60}};
        for (const Layer& ly : layers)
            for (float x = L; x < R; x += 6) {
                float wx = x + cam * ly.speed;
                float s = std::sin(wx * ly.freq);
                float hh = ly.base - ly.amp * (s > 0.2f ? 1.0f : s + 0.8f);   // flat-topped mesas
                DrawRectangle((int)x, (int)hh, 7, (int)(kGround - hh) + 1, Hex(ly.col));
            }
    }
    // floating arrays: rows of boxes with their values, drifting in the warm air
    for (int i = 0; i < 6; i++) {
        float wx = i * 760.0f + 300;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4560 * 10, 4560.0f) + L - 400;
        if (x < L - 300 || x > R + 300) continue;
        float y = 140 + (i % 3) * 80 + std::sin(time * 0.7f + i * 1.7f) * 9;
        int n = 3 + i % 3;
        const float bw = 34;
        for (int c = 0; c < n; c++) {
            Rectangle b = {x - n * bw / 2 + c * bw, y, bw - 3, bw - 3};
            DrawRectangleRounded(b, 0.2f, 6, Fade2(Hex(0xfff6e4), 0.55f));
            DrawRectangleRoundedLinesEx(b, 0.2f, 6, 1.5f, Fade2(Hex(0xb07a6a), 0.55f));
            TextCenter(std::to_string((i * 7 + c * 3) % 10), b, 15, Fade2(Hex(0x9a6a5a), 0.75f), F_MONOBOLD);
            TextCenter(TextFormat("%d", c), {b.x, b.y + bw, b.width, 14}, 11, Fade2(Hex(0x9a6a5a), 0.6f), F_MONOBOLD);
        }
    }
    // the ground: sand on top, rows of sandstone blocks below, pebbles and dry grass
    const Color soil = Hex(0x7a4d48), sand = Hex(0xe2b48a), sandD = Hex(0xc0906e), brick0 = Hex(0x9c6a5a), brick1 = Hex(0x8d5e50);
    const Color tuft = Hex(0xf0d39c), pebble = Hex(0xb98670);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 14, sand);
    DrawRectangle((int)L, (int)kGround + 14, (int)scr.width + 1, 6, sandD);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        float wx = x + cam;
        int seed = (int)std::floor(wx / 64);
        for (int row = 0; row < 3; row++) {
            float y = kGround + 32 + row * 40.0f, xo = (row % 2) * 32.0f;
            DrawRectangleRounded({x + xo + 4, y, 56, 30}, 0.2f, 4, row == 0 ? brick0 : brick1);
            DrawRectangleRounded({x + xo + 8, y + 3, 48, 4}, 1, 4, Fade2(Hex(0xc8907a), 0.6f));
        }
        if (seed % 3 == 0) {   // dry grass
            Tri({x + 10, kGround + 2}, {x + 13, kGround - 13}, {x + 18, kGround + 2}, tuft);
            Tri({x + 16, kGround + 2}, {x + 22, kGround - 9}, {x + 24, kGround + 2}, tuft);
        }
        if (seed % 4 == 1) DrawEllipse((int)x + 40, (int)kGround + 2, 9, 5, pebble);
    }
}

// --------------------------------------------------------------- the icon ---
void DrawGameIcon(Rectangle r, float time, float hot) {
    const float s = r.width, rad = s * 0.28f;
    if (hot > 0.01f) Glow({r.x + s / 2, r.y + s / 2}, s * 1.1f, Hex(0x8b7bff), 0.3f * hot);
    Shadow(r, rad, 0.9f);
    GradientRoundRectV(r, rad, Hex(0x3a2f7a), Hex(0x1b1640));
    // a moon and a few stars behind the sword
    DrawCircleV({r.x + s * 0.30f, r.y + s * 0.30f}, s * 0.12f, Hex(0xffd29a));
    DrawCircleV({r.x + s * 0.34f, r.y + s * 0.27f}, s * 0.10f, Hex(0x2f2766));   // a crescent
    const float st[3][2] = {{0.62f, 0.20f}, {0.80f, 0.52f}, {0.22f, 0.62f}};
    for (auto& p : st) DrawCircleV({r.x + s * p[0], r.y + s * p[1]}, std::max(1.0f, s * 0.018f), Hex(0xd8d3f5));
    RoundRectLines(r, rad, std::max(1.0f, s * 0.025f), Fade2(Hex(0xb9b0ff), 0.5f + 0.4f * hot));
    // the sword, its tip a pointer arrow
    Vector2 a = {r.x + s * 0.24f, r.y + s * 0.80f}, b = {r.x + s * 0.72f, r.y + s * 0.32f};
    float th = std::max(2.0f, s * 0.07f);
    DrawLineEx(a, b, th, Hex(0xe3defa));
    Vector2 d = {(b.x - a.x), (b.y - a.y)};
    float l = std::sqrt(d.x * d.x + d.y * d.y);
    d = {d.x / l, d.y / l};
    Vector2 n = {-d.y, d.x};
    float hs = s * 0.12f;
    Tri({b.x + d.x * hs * 0.6f, b.y + d.y * hs * 0.6f}, {b.x - d.x * hs * 0.4f + n.x * hs * 0.6f, b.y - d.y * hs * 0.4f + n.y * hs * 0.6f},
        {b.x - d.x * hs * 0.4f - n.x * hs * 0.6f, b.y - d.y * hs * 0.4f - n.y * hs * 0.6f}, Hex(0xff9e4a));
    Vector2 g = {a.x + d.x * s * 0.1f, a.y + d.y * s * 0.1f};
    DrawLineEx({g.x - n.x * s * 0.09f, g.y - n.y * s * 0.09f}, {g.x + n.x * s * 0.09f, g.y + n.y * s * 0.09f}, th, Hex(0xffc14d));
    (void)time;
}

} // namespace fp
