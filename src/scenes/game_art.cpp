// ============================================================================
// game_art.cpp  -  The Forgotten Pointer's drawings (see game_art.h).
// The game has its own palette: it doesn't follow the app's themes.
// ============================================================================
#include "scenes/game_art.h"
#include "app/ui.h"
#include <algorithm>
#include <cmath>

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

// ---------------------------------------------------------------- the hero ---
void DrawHero(Vector2 feet, float scale, int face, float walk, float attack, float time, bool blink) {
    if (blink) return;
    Frame f{feet, scale, face};
    const Color armor = Hex(0x6d5ef0), armorD = Hex(0x4a3cc0), steel = Hex(0xe3defa), cape = Hex(0x2b2552);
    float step = std::sin(walk) * 7, bob = std::fabs(std::sin(walk)) * 2;
    // cape, behind
    f.Triangle(-14, -100 + bob, -46 - step * 0.4f, -14, 6, -100 + bob, cape);
    f.Triangle(6, -100 + bob, -46 - step * 0.4f, -14, -6, -20, cape);
    // legs
    f.Box(-20 + step, -44, 16, 44, 0.45f, armorD);
    f.Box(4 - step, -44, 16, 44, 0.45f, armorD);
    // shield on the back arm
    f.Box(-44, -94 + bob, 24, 46, 0.45f, Hex(0xb06cff));
    f.Box(-41, -91 + bob, 18, 40, 0.45f, Hex(0xc98aff));
    // body and belt
    f.Box(-26, -102 + bob, 52, 62, 0.4f, armor);
    f.Box(-26, -62 + bob, 52, 9, 0.6f, armorD);
    f.Ell(-8, -90 + bob, 10, 6, Fade2(WHITE, 0.25f));
    // head: helmet with a glowing visor
    f.Circle(0, -124 + bob, 24, armor);
    f.Box(-16, -132 + bob, 36, 12, 0.7f, Hex(0x0f0d1f));
    f.Box(-10, -129 + bob, 27, 5, 1, Hex(0x48e5ff));
    f.Ell(-8, -138 + bob, 7, 4, Fade2(WHITE, 0.35f));
    // sword: idle up and forward; an attack swings it from behind the head to the front
    float a = attack < 0 ? -1.2f : -2.2f + 3.0f * std::sin(std::min(1.0f, attack) * 1.5708f);
    Vector2 pivot = f.P(18, -84 + bob);
    Vector2 dir = {face * std::cos(a), std::sin(a)};
    float len = 72 * scale;
    Vector2 tip = {pivot.x + dir.x * len, pivot.y + dir.y * len};
    if (attack >= 0 && attack < 1) {   // the swing's trail
        for (int k = 1; k <= 6; k++) {
            float b = a - 0.18f * k;
            Vector2 d2 = {face * std::cos(b), std::sin(b)};
            DrawLineEx({pivot.x + d2.x * len * 0.4f, pivot.y + d2.y * len * 0.4f}, {pivot.x + d2.x * len, pivot.y + d2.y * len},
                       6 * scale, Fade2(Hex(0x9fe9ff), 0.45f - k * 0.07f));
        }
    }
    DrawLineEx(pivot, tip, 6 * scale, steel);
    Vector2 n = {-dir.y, dir.x};
    Vector2 guard = {pivot.x + dir.x * 14 * scale, pivot.y + dir.y * 14 * scale};
    DrawLineEx({guard.x - n.x * 12 * scale, guard.y - n.y * 12 * scale}, {guard.x + n.x * 12 * scale, guard.y + n.y * 12 * scale}, 5 * scale, Hex(0xffc14d));
    Tri(tip, {tip.x - dir.x * 12 * scale + n.x * 7 * scale, tip.y - dir.y * 12 * scale + n.y * 7 * scale},
        {tip.x - dir.x * 12 * scale - n.x * 7 * scale, tip.y - dir.y * 12 * scale - n.y * 7 * scale}, steel);   // a cursor-like tip
    DrawCircleV(pivot, 7 * scale, armorD);
    (void)time;
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
void DrawSerpent(Vector2 c, float R, float time, float hurt, float alpha) {
    const int n = 30;
    float spin = time * 1.4f;
    Glow(c, R * 1.6f, Hex(0x3fd1c7), 0.22f * alpha);
    // the body: segments around a circle, thick at the head, thin at the tail
    for (int i = n - 1; i >= 0; i--) {
        float u = (float)i / n, a = spin + u * 2 * PI * 0.94f;
        Vector2 p = {c.x + std::cos(a) * R, c.y + std::sin(a) * R};
        float r = R * (0.24f - 0.13f * u);
        Color col = (i % 4 < 2) ? Hex(0x2bb5a8) : Hex(0x6a4fd8);
        DrawCircleV(p, r + 3, Fade2(Hex(0x101024), alpha));
        DrawCircleV(p, r, Fade2(Flash(col, hurt), alpha));
        DrawCircleV({p.x - r * 0.3f, p.y - r * 0.35f}, r * 0.3f, Fade2(WHITE, 0.18f * alpha));
    }
    // the head, biting its own tail
    float a = spin - 0.05f;
    Vector2 h = {c.x + std::cos(a) * R, c.y + std::sin(a) * R};
    Vector2 fwd = {-std::sin(a), std::cos(a)}, out = {std::cos(a), std::sin(a)};
    float hr = R * 0.30f;
    Vector2 snout = {h.x - fwd.x * hr * 0.9f, h.y - fwd.y * hr * 0.9f};
    DrawCircleV(h, hr + 4, Fade2(Hex(0x101024), alpha));
    DrawCircleV(h, hr, Fade2(Flash(Hex(0x35c9bb), hurt), alpha));
    DrawCircleV(snout, hr * 0.7f, Fade2(Flash(Hex(0x35c9bb), hurt), alpha));
    for (int k = -1; k <= 1; k += 2) {
        Vector2 e = {h.x + out.x * hr * 0.45f * k + fwd.x * hr * 0.1f, h.y + out.y * hr * 0.45f * k + fwd.y * hr * 0.1f};
        DrawCircleV(e, hr * 0.24f, Fade2(Hex(0xfff4a8), alpha));
        DrawCircleV(e, hr * 0.11f, Fade2(Hex(0x1a1030), alpha));
    }
    // fangs and a flickering tongue
    DrawLineEx(snout, {snout.x - fwd.x * hr * 0.6f + out.x * hr * 0.2f * std::sin(time * 14),
                       snout.y - fwd.y * hr * 0.6f + out.y * hr * 0.2f * std::sin(time * 14)}, 4, Fade2(Hex(0xff4d6d), alpha));
    // while(1) floating in the middle
    TextCenter("while (1)", {c.x - R, c.y - 14, R * 2, 28}, std::max(12.0f, R * 0.16f), Fade2(Hex(0x9ff5ea), 0.9f * alpha), F_MONOBOLD);
}

void DrawLoopRing(Vector2 c, float r, float time) {
    Glow(c, r * 1.8f, Hex(0x3fd1c7), 0.35f);
    DrawRing(c, r * 0.62f, r, time * 400, time * 400 + 300, 24, Hex(0x2bb5a8));
    DrawCircleV(c, r * 0.22f, Hex(0x9ff5ea));
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

void DrawSegfault(Vector2 c, float s, float time) {
    Color core = Hex(0xff3d5a), skin = Hex(0x5a2147), skinD = Hex(0x431635), plate = Hex(0x6b2752), horn = Hex(0xff8a5c);
    Glow(c, 260 * s, core, 0.25f);
    for (int k = -1; k <= 1; k += 2) {
        DrawSplineSegmentBezierCubic({c.x + k * 90 * s, c.y - 250 * s}, {c.x + k * 120 * s, c.y - 330 * s}, {c.x + k * 230 * s, c.y - 300 * s},
                                     {c.x + k * 240 * s, c.y - 380 * s}, 26 * s, horn);
        DrawLineEx({c.x + k * 190 * s, c.y - 40 * s}, {c.x + k * 300 * s, c.y + 110 * s}, 70 * s, skinD);
        DrawCircleV({c.x + k * 300 * s, c.y + 110 * s}, 44 * s, skinD);
    }
    DrawEllipse((int)c.x, (int)(c.y + 40 * s), 230 * s, 220 * s, skin);
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 4; col++) {
            bool hot = (row * 5 + col * 3) % 4 == 0;
            Rectangle b = {c.x + (-150 + col * 76) * s, c.y + (10 + row * 64) * s, 68 * s, 56 * s};
            DrawRectangleRounded(b, 0.2f, 6, hot ? Mix(plate, core, 0.45f) : plate);
        }
    Rectangle face = {c.x - 160 * s, c.y - 260 * s, 320 * s, 190 * s};
    DrawRectangleRounded(face, 0.18f, 10, Hex(0x14060f));
    DrawRectangleRoundedLinesEx(face, 0.18f, 10, 5 * s, core);
    for (int k = 0; k < 2; k++) {
        float ex = c.x + (-72 + k * 144) * s, ey = c.y - 190 * s, d = 28 * s;
        DrawLineEx({ex - d, ey - d}, {ex + d, ey + d}, 13 * s, core);
        DrawLineEx({ex - d, ey + d}, {ex + d, ey - d}, 13 * s, core);
    }
    TextCenter("0xC0000005", {face.x, c.y - 140 * s, face.width, 30 * s}, 30 * s, Hex(0xffb3c0), F_MONOBOLD);
    float pulse = 0.8f + 0.2f * std::sin(time * 4);
    Glow({c.x, c.y + 230 * s}, 110 * s * pulse, core, 0.8f);
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
void DrawStackPlains(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    // sky: dusk over the machine
    DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0x1b1640), Hex(0x6a3f8a));
    Glow({L + scr.width * 0.72f - cam * 0.02f, 210}, 200, Hex(0xffb36b), 0.55f);   // a low sun
    DrawCircleV({L + scr.width * 0.72f - cam * 0.02f, 210}, 64, Hex(0xffd29a));
    // stars
    for (int i = 0; i < 60; i++) {
        float x = std::fmod(i * 197.3f - cam * 0.03f, scr.width + 40);
        if (x < 0) x += scr.width + 40;
        float y = std::fmod(i * 71.7f, 300) + 20;
        float tw = 0.4f + 0.6f * std::fabs(std::sin(time * 0.8f + i));
        DrawCircleV({L + x - 20, y}, 1.4f, Fade2(WHITE, 0.5f * tw));
    }
    // far mountains, then hills: each layer moves slower the further it is
    struct Layer { float speed, base, amp, freq; unsigned col; };
    const Layer layers[3] = {{0.15f, 470, 80, 0.004f, 0x3a2a6b}, {0.35f, 540, 55, 0.007f, 0x2f2a5e}, {0.6f, 600, 30, 0.011f, 0x24304f}};
    for (const Layer& ly : layers) {
        for (float x = L; x < R; x += 6) {
            float wx = x + cam * ly.speed;
            float h = ly.base - ly.amp * (0.6f * std::sin(wx * ly.freq) + 0.4f * std::sin(wx * ly.freq * 2.3f + 1.7f));
            DrawRectangle((int)x, (int)h, 7, (int)(kGround - h) + 1, Hex(ly.col));
        }
    }
    // floating stack frames: little piles of plates drifting in the sky
    for (int i = 0; i < 7; i++) {
        float wx = i * 640.0f + 200;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4000 * 10, 4480.0f) + L - 400;
        if (x < L - 200 || x > R + 200) continue;
        float y = 150 + (i % 3) * 70 + std::sin(time * 0.8f + i) * 8;
        int n = 2 + i % 3;
        for (int k = 0; k < n; k++) {
            Rectangle pl = {x - 46 + k * 4, y - k * 16, 92 - k * 8, 13};
            DrawRectangleRounded(pl, 0.5f, 6, Fade2(Hex(0x8b7bff), 0.30f + 0.08f * k));
            DrawRectangleRoundedLinesEx(pl, 0.5f, 6, 1.5f, Fade2(Hex(0xc9c4ff), 0.35f));
        }
    }
    // the ground: grass on top, rows of memory tiles below
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, Hex(0x1a1430));
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 14, Hex(0x3fbf6a));
    DrawRectangle((int)L, (int)kGround + 14, (int)scr.width + 1, 6, Hex(0x2a8a4c));
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        for (int row = 0; row < 3; row++) {
            float y = kGround + 32 + row * 40, xo = (row % 2) * 32;
            DrawRectangleRounded({x + xo + 4, y, 56, 30}, 0.2f, 4, Hex(row == 0 ? 0x2a2148 : 0x221b3d));
        }
        // tufts of grass
        Tri({x + 10, kGround + 2}, {x + 16, kGround - 10}, {x + 22, kGround + 2}, Hex(0x5ad87f));
        Tri({x + 40, kGround + 2}, {x + 44, kGround - 7}, {x + 50, kGround + 2}, Hex(0x5ad87f));
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
