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
    const int layers = 12, sides = r > 60 ? 32 : r > 20 ? 20 : 12;   // big glows need rounder edges
    for (int i = 0; i < layers; i++) DrawCircleSector(c, r * (1 - (float)i / layers), 0, 360, sides, Fade2(col, strength / layers));
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

const Color kInk = {22, 20, 38, 255};   // outlines: a deep navy, softer than black against the paintings
float gHurt = 0, gAlpha = 1;
Color gAmb = WHITE;                   // the land's light (SetLandLight)
Color K(Color c) {
    c = Flash(c, gHurt);
    c.r = (unsigned char)(c.r * gAmb.r / 255);
    c.g = (unsigned char)(c.g * gAmb.g / 255);
    c.b = (unsigned char)(c.b * gAmb.b / 255);
    return Fade2(c, gAlpha);
}

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
// The Debugger: a knight in brushed steel, with a deep blue cloak trimmed in
// gold, a red scarf, a round wooden shield with {*} painted on it, and a sword
// whose tip is a pointer's arrowhead. About 290 units tall, lit from the top
// left, and tinted by the land's light like everything else.
void DrawHero(Vector2 feet, float scale, int face, float walk, float attack, float time, bool blink) {
    if (blink) return;
    const float k = scale * 0.5f;   // ~145 pixels tall at scale 1
    BeginLocal(feet, face * k, k);
    auto P = [](float x, float y) { return Vector2{x, y}; };
    const float ow = 3.2f;
    const Color steel = Hex(0x8e98ac), steelL = Hex(0xd4dae6), steelD = Hex(0x5a647e), steelDD = Hex(0x3a4258);
    const Color cloak = Hex(0x26385a), cloakL = Hex(0x3a5482), cloakD = Hex(0x18233c), trim = Hex(0xd9a441);
    const Color scarf = Hex(0xb5443a), scarfL = Hex(0xe0695a), scarfD = Hex(0x7e2a26);
    const Color leather = Hex(0x6b4a32), leatherD = Hex(0x47301f), cloth = Hex(0x39334c);
    const Color wood = Hex(0x8a6038), woodD = Hex(0x5e3f22), gold = Hex(0xe0b04a), visor = Hex(0x48e5ff);
    float p = attack < 0 ? -1 : std::min(1.0f, attack);   // the swing, 0..1
    float ext = p < 0 ? 0 : std::sin(p * 1.5708f);       // how far the arm reaches out
    float lean = p < 0 ? 0 : 9 * std::sin(p * 3.1416f);
    float step = std::sin(walk) * 11, bob = std::fabs(std::sin(walk)) * 4, breathe = std::sin(time * 2.2f) * 1.5f;
    float wave = std::sin(time * 2.4f) * 7 - step * 0.6f;
    auto U = [&](float x, float y) { return P(x, y + bob + breathe); };   // the upper body bobs and breathes

    // the cloak, behind: from the shoulders to the knees, flowing back
    PolyO({U(-26 - lean, -222), U(14 - lean, -224), U(6, -150), P(-14, -70), P(-30, -22), P(-86 + wave, -28), P(-104 + wave * 1.2f, -72),
           U(-84 + wave * 0.6f, -140), U(-52, -200)}, cloak, ow);
    Poly({U(-24 - lean, -218), U(-6 - lean, -218), P(-20, -82), P(-58 + wave, -42), U(-62 + wave * 0.4f, -140), U(-44, -196)}, cloakL);   // a lit fold
    DrawLineEx(U(-40, -190), P(-74 + wave, -36), 5, K(cloakD));
    DrawLineEx(U(-58, -176), P(-96 + wave * 1.2f, -64), 4, K(cloakD));
    DrawLineEx(P(-30, -22), P(-86 + wave, -28), 5, K(trim));   // the gold hem

    // back leg: dark trousers, a steel greave, a leather boot
    OLine(P(-12 - step * 0.8f, -112), P(-16 - step, -56), 22, cloth, ow);
    OLine(P(-16 - step, -56), P(-18 - step, -24), 20, steelD, ow);
    PolyO({P(-32 - step, -28), P(-4 - step, -28), P(4 - step, -8), P(6 - step, 0), P(-34 - step, 0)}, leatherD, ow);

    // the shield on the back arm: round and wooden, a steel rim, {*} painted on it
    Vector2 sc = U(-30 + lean * 0.3f, -160);
    OC(sc, 46, steelD, ow);
    DrawCircleV(sc, 39, K(wood));
    DrawLineEx({sc.x - 37, sc.y - 13}, {sc.x + 37, sc.y - 13}, 2.5f, K(woodD));   // planks
    DrawLineEx({sc.x - 37, sc.y + 13}, {sc.x + 37, sc.y + 13}, 2.5f, K(woodD));
    DrawCircleSector(sc, 39, 180, 270, 16, K(Fade2(WHITE, 0.14f)));   // light, top left
    DrawCircleSector(sc, 39, 0, 90, 16, K(Fade2(BLACK, 0.2f)));       // shade, bottom right
    DrawRing(sc, 39, 46, 200, 250, 12, K(Fade2(steelL, 0.7f)));       // the rim catching the light

    // front leg
    OLine(P(14 + step * 0.8f, -112), P(16 + step, -56), 24, cloth, ow);
    OLine(P(16 + step, -56), P(18 + step, -24), 22, steel, ow);
    DrawLineEx(P(11 + step, -54), P(12 + step, -28), 4, K(steelL));
    OC(P(16 + step, -58), 9, steelL, 2.5f);   // the knee
    PolyO({P(2 + step, -28), P(30 + step, -28), P(44 + step, -10), P(46 + step, 0), P(2 + step, 0)}, leather, ow);
    DrawLineEx(P(4 + step, -20), P(30 + step, -20), 3, K(leatherD));

    // a mail skirt, the breastplate, the belt
    PolyO({U(-36, -130), U(36, -130), U(42, -92), U(-42, -92)}, Hex(0x5c6274), ow);
    for (int i = 0; i < 3; i++) DrawLineEx(U(-36 - i * 2.0f, -120 + i * 10.0f), U(36 + i * 2.0f, -120 + i * 10.0f), 2, K(Hex(0x7c8396)));
    PolyO({U(-40 + lean, -222), U(40 + lean, -222), U(46 + lean * 0.5f, -186), U(36, -130), U(-36, -130), U(-46 + lean * 0.5f, -186)}, steel, ow);
    Poly({U(-34 + lean, -216), U(0 + lean, -216), U(-4, -138), U(-30, -146), U(-40 + lean * 0.5f, -186)}, steelL);   // the lit side
    Poly({U(16, -150), U(40 + lean * 0.5f, -186), U(32, -134), U(12, -134)}, steelD);                                // the shaded side
    DrawLineEx(U(0 + lean, -218), U(-2, -134), 3, K(steelDD));                                                         // the ridge
    RR({-40, -136 + bob + breathe, 80, 12}, 4, leather, ow * 0.8f);
    RR({-8, -139 + bob + breathe, 16, 18}, 3, gold, ow * 0.8f);

    // the scarf: round the neck, its end flying back
    PolyO({U(-20 + lean, -228), U(-40 + wave * 0.3f, -222), U(-78 + wave, -212 + wave * 0.4f), U(-94 + wave * 1.3f, -196 + wave * 0.6f),
           U(-64 + wave, -200), U(-26 + lean, -214)}, scarf, ow);
    DrawLineEx(U(-30 + lean, -222), U(-76 + wave, -208), 3, K(scarfD));
    PolyO({U(-26 + lean, -234), U(24 + lean, -236), U(26 + lean, -216), U(-28 + lean, -212)}, scarf, ow);
    DrawLineEx(U(-20 + lean, -231), U(20 + lean, -233), 3, K(scarfL));

    // the head: a rounded great helm, a ridge on top, a narrow slit glowing faintly
    float hx = 4 + lean;
    Vector2 hc = U(hx, -258);
    PolyO({U(hx - 22, -284), U(hx - 2, -296), U(hx + 18, -288), U(hx + 4, -280)}, steelD, ow * 0.8f);   // the ridge
    OC(hc, 30, steel, ow);
    DrawCircleSector(hc, 30, 180, 275, 16, K(Fade2(steelL, 0.85f)));
    DrawCircleSector(hc, 30, 20, 110, 16, K(Fade2(steelDD, 0.55f)));
    PolyO({U(hx + 6, -278), U(hx + 34, -270), U(hx + 36, -242), U(hx + 22, -230), U(hx + 4, -234)}, steelD, ow * 0.8f);   // the face plate
    DrawRectangleRounded({hx + 9, -265 + bob + breathe, 26, 6}, 1, 6, K(Hex(0x0e1420)));
    GlowK(U(hx + 24, -262), 16, visor, 0.35f);
    DrawRectangleRounded({hx + 14, -264 + bob + breathe, 16, 3}, 1, 6, K(visor));
    for (int i = 0; i < 3; i++) DrawCircleV(U(hx + 28, -252 + i * 6.0f), 1.6f, K(steelDD));   // breathing holes

    // the front shoulder
    OC(U(34 + lean, -212), 18, steel, ow);
    DrawCircleSector(U(34 + lean, -212), 18, 180, 300, 12, K(Fade2(steelL, 0.7f)));

    // the arm and the sword (its tip is a pointer's arrowhead)
    Vector2 sh = U(34 + lean, -206);
    Vector2 el = Add(sh, V(18 + 16 * ext, 40 - 26 * ext));
    Vector2 hand = Add(sh, V(36 + 28 * ext, 22 - 4 * ext));
    float ang = p < 0 ? -1.05f : -2.3f + 3.0f * ext;   // idle: up and forward; a swing comes over the head
    Vector2 d = {std::cos(ang), std::sin(ang)}, n = {-d.y, d.x};
    if (p >= 0 && p < 1) {   // the swing's arc
        for (int i = 0; i < 10; i++) {
            float a0 = ang - 1.3f + i * 0.13f;
            Vector2 dd = {std::cos(a0), std::sin(a0)};
            DrawLineEx(Add(hand, Mul(dd, 36)), Add(hand, Mul(dd, 160)), 14, Fade2(Mix(WHITE, visor, 0.35f), (0.03f + i * 0.025f) * (1 - p * 0.5f)));
        }
    }
    OLine(sh, el, 18, cloth, ow);
    OLine(el, hand, 17, steelD, ow);
    auto At = [&](float along, float side) { return Add(Add(hand, Mul(d, along)), Mul(n, side)); };
    OLine(At(-8, 0), At(-28, 0), 8, leatherD, ow * 0.8f);   // grip
    OC(At(-34, 0), 8, gold, ow * 0.8f);                     // pommel
    PolyO({At(14, -7), At(128, -5), At(128, 5), At(14, 7)}, Hex(0xc8d0de), ow);
    Poly({At(14, -7), At(128, -5), At(128, 0), At(14, 0)}, Fade2(WHITE, 0.8f));
    DrawLineEx(At(22, 0), At(120, 0), 2.5f, K(steelD));
    GlowK(At(140, 0), 30, visor, 0.4f);
    PolyO({At(122, -15), At(160, 0), At(122, 15), At(130, 0)}, visor, ow);
    Poly({At(130, -7), At(154, 0), At(130, 0)}, Fade2(WHITE, 0.85f));
    OLine(At(10, -22), At(10, 22), 8, gold, ow * 0.8f);   // crossguard
    OC(hand, 11, steelD, ow);                             // gauntlet

    EndLocal();
    // the {*} painted on the shield, and engraved small on the chest
    LocalText("{*}", {-30 + lean * 0.3f, -160 + bob + breathe}, feet, face * k, k, 30, Fade2(Mix(gold, gAmb, 0.15f), 0.95f), F_MONOBOLD);
    LocalText("{*}", {0 + lean * 0.5f, -182 + bob + breathe}, feet, face * k, k, 18, Fade2(Hex(0x3a4258), 0.8f), F_MONOBOLD);
}

// -------------------------------------------------------- Off-by-One Goblin ---
// A goblin with a sign that says <=, the comparison that runs one step too far.
void DrawGoblin(Vector2 feet, float scale, int face, float walk, float hurt, float alpha) {
    const float k = scale * 0.5f;
    BeginLocal(feet, face * k, k);
    gHurt = hurt;
    gAlpha = alpha;
    const float ow = 3.2f;
    const Color skin = Hex(0x7fa54c), skinD = Hex(0x55762f), skinL = Hex(0xbad77e), tunic = Hex(0x7a5232), tunicD = Hex(0x553820);
    const Color wood = Hex(0x8a6038), board = Hex(0xd9c08a), eye = Hex(0xffe27a);
    float step = std::sin(walk) * 9, bob = std::fabs(std::sin(walk)) * 5;
    auto P = [](float x, float y) { return Vector2{x, y}; };
    auto U = [&](float x, float y) { return P(x, y + bob); };
    // the sign, held up behind it
    OLine(U(60, -30), U(60, -196), 7, wood, ow);
    RR({28, -232 + bob, 66, 40}, 5, board, ow);
    DrawLineEx(U(32, -220), U(90, -220), 2, K(Fade2(Hex(0x9a7a48), 0.7f)));
    // legs and feet
    OLine(P(-14 - step, -58), P(-16 - step, -16), 15, skinD, ow);
    OLine(P(14 + step, -58), P(16 + step, -16), 15, skin, ow);
    PolyO({P(-30 - step, -18), P(-4 - step, -18), P(2 - step, 0), P(-32 - step, 0)}, tunicD, ow);
    PolyO({P(0 + step, -18), P(26 + step, -18), P(34 + step, 0), P(0 + step, 0)}, tunicD, ow);
    // a round body in a ragged tunic
    OC(U(0, -86), 40, skin, ow);
    PolyO({U(-40, -94), U(40, -94), U(44, -56), U(30, -48), U(18, -56), U(4, -46), U(-10, -56), U(-26, -46), U(-44, -56)}, tunic, ow);
    DrawLineEx(U(-38, -84), U(38, -84), 4, K(tunicD));   // a rope belt
    // the arm holding the sign
    OLine(U(30, -90), U(54, -100), 12, skin, ow);
    OC(U(58, -102), 8, skinD, ow * 0.8f);
    // the head: long ears, a big nose, yellow eyes, a crooked grin
    PolyO({U(-26, -168), U(-74, -196), U(-34, -144)}, skin, ow);
    PolyO({U(28, -170), U(70, -206), U(36, -146)}, skin, ow);
    Poly({U(-32, -164), U(-62, -186), U(-36, -150)}, Hex(0xd88a8a));
    OC(U(2, -152), 38, skin, ow);
    DrawCircleSector(U(2, -152), 38, 180, 275, 16, K(Fade2(skinL, 0.8f)));
    DrawCircleSector(U(2, -152), 38, 20, 110, 16, K(Fade2(skinD, 0.6f)));
    PolyO({U(22, -152), U(46, -140), U(24, -132)}, skinD, ow * 0.8f);   // the nose
    for (int i = 0; i < 2; i++) {
        Vector2 e = U((float)(i == 0 ? -10 : 18), -164);
        OC(e, 8, eye, 2.5f);
        DrawCircleV({e.x + 2.5f, e.y + 1}, 3.5f, K(kInk));
    }
    PolyO({U(-14, -130), U(24, -134), U(18, -122), U(-8, -120)}, Hex(0x3a1a14), ow * 0.7f);   // the grin
    Tri(U(-4, -130), U(2, -131), U(-1, -124), K(WHITE));
    Tri(U(10, -132), U(16, -133), U(13, -126), K(WHITE));
    EndLocal();
    LocalText("<=", {61, -212 + bob}, feet, face * k, k, 30, Fade2(Hex(0x8a2a10), alpha), F_MONOBOLD);
}

// ------------------------------------------------- light and ground shadow ---
void SetLandLight(int which) {
    // the land's light on everything drawn: a gentle tint
    static const unsigned amb[6] = {0xfff4e4, 0xffe8cd, 0xdde8ff, 0xd2ecd8, 0xe6dfff, 0xf2d8ff};
    if (which < 0 || which > 5) { gAmb = WHITE; return; }
    gAmb = Hex(amb[which]);
}

void DrawShadow(Vector2 ground, float width, float lift) {
    float k = std::clamp(1 - lift, 0.25f, 1.0f);
    DrawEllipse((int)ground.x, (int)ground.y + 2, width * (0.55f + 0.45f * k), 7 + 3 * k, Fade2(Hex(0x0a0814), 0.38f * k));
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

// --------------------------------------------------------------- NULL Ghost ---
// A little ghost that points at nothing: its arrow ends at a NULL tag, and its
// badge says 0x0. solid: 1 normal .. 0 "NULL" (see-through: it can't be hit).
void DrawGhost(Vector2 at, float scale, int face, float hurt, float alpha, float solid, float time) {
    const float k = scale * 0.42f;
    const float a = alpha * (0.25f + 0.75f * solid);
    const float bob = std::sin(time * 3) * 6;
    BeginLocal({at.x, at.y + bob * k}, face * k, k);
    gHurt = hurt;
    gAlpha = a;
    const Color sheet = Hex(0xeef4ff), sheetD = Hex(0xb7c6ea), sheetL = WHITE, eye = Hex(0x1b1640), arrow = Hex(0x48e5ff);
    GlowK({0, -110}, 150, Hex(0xb9d8ff), 0.25f);
    // the body: a round head and a sheet with a wavy hem
    std::vector<Vector2> body;
    for (int i = 0; i <= 12; i++) {   // the dome
        float ang = PI + PI * i / 12.0f;
        body.push_back({std::cos(ang) * 62, -150 + std::sin(ang) * 62});
    }
    body.push_back({62, -40});
    for (int i = 1; i < 6; i++) {   // the hem, waving
        float x = 62 - i * 124.0f / 6, w = std::sin(time * 6 + i * 1.3f) * 7;
        body.push_back({x, (i % 2 ? -14.0f : -34.0f) + w});
    }
    body.push_back({-62, -40});
    // outline, then fill (a fan from the middle: the shape is round enough)
    for (size_t i = 0; i < body.size(); i++) { DrawLineEx(body[i], body[(i + 1) % body.size()], 11, K(kInk)); DrawCircleV(body[i], 5.5f, K(kInk)); }
    for (size_t i = 0; i < body.size(); i++) Tri({0, -100}, body[i], body[(i + 1) % body.size()], K(sheet));
    DrawCircleV({-22, -176}, 18, K(Fade2(sheetL, 0.8f)));                 // shine
    DrawCircleSector({0, -150}, 62, 10, 80, 16, K(Fade2(sheetD, 0.45f)));  // shade
    // eyes, a little "o" mouth
    for (int i = 0; i < 2; i++) {
        Vector2 e = {(float)(i == 0 ? 2 : 34), -150};
        DrawEllipse((int)e.x, (int)e.y, 9, 13, K(eye));
        DrawCircleV({e.x + 3, e.y - 5}, 3.5f, K(WHITE));
    }
    DrawEllipse(18, -116, 7, 9, K(eye));
    // an arm, holding a pointer that points down at nothing
    OLine({40, -96}, {74, -84}, 16, sheet, 4.5f);
    DrawLineEx({80, -86}, {108, -10}, 6, K(kInk));
    DrawLineEx({80, -86}, {108, -10}, 3, K(arrow));
    PolyO({{100, -26}, {120, -16}, {110, 4}}, arrow, 3.5f);
    EndLocal();
    if (a < 0.05f) return;
    // its tag at the arrow's tip, and the address on its badge
    LocalText("NULL", {124, 22}, {at.x, at.y + bob * k}, face * k, k, 26, Fade2(Hex(0xff8a9c), a), F_MONOBOLD);
    float px = std::round(std::max(12.0f, 15 * scale));
    Vector2 m = Measure("0x0", px, F_MONOBOLD);
    Vector2 b = {at.x, at.y + bob * k - 236 * k - px};
    Rectangle pill = {b.x - m.x / 2 - 8, b.y - 3, m.x + 16, px + 8};
    DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x101830), 0.85f * a));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Fade2(Hex(0x48e5ff), a));
    Text("0x0", b.x - m.x / 2, b.y + 1, px, Fade2(Hex(0xbff6ff), a), F_MONOBOLD);
}

// ------------------------------------------------------ Wild Pointer Wyvern ---
// An ice wyvern born from a pointer nobody set: its tail ends in an arrowhead,
// its chest says int *p; and the address it holds keeps changing (garbage).
// face: 1 looks right. tilt: degrees (a dive leans it). wing: 0..1 flap.
void DrawWyvern(Vector2 c, float scale, int face, float tilt, float wing, float hurt, float alpha, float time) {
    const float k = scale * 0.62f;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0);
    rlRotatef(tilt * face, 0, 0, 1);
    rlScalef(face * k, k, 1);
    gHurt = hurt;
    gAlpha = alpha;
    const Color ice = Hex(0x8fc8f0), iceD = Hex(0x4f86c0), iceL = Hex(0xd8f0ff), wingC = Hex(0x3f6fb0), wingM = Hex(0x7fb4e8), belly = Hex(0xe8f6ff);
    const Color eye = Hex(0xff3d5a), horn = Hex(0xf0f4ff);
    GlowK({0, 0}, 260, Hex(0x9fd8ff), 0.2f);
    float flap = std::sin(wing * 6.2832f);
    // the far wing
    PolyO({{-10, -40}, {-70, -190 - flap * 50}, {-190, -180 - flap * 40}, {-160, -110 - flap * 20}, {-70, -20}}, wingC, 5);
    // the tail, ending in a pointer's arrowhead
    Vector2 t0 = {-70, 20}, t1 = {-160, 70}, t2 = {-230, 20 + std::sin(time * 3) * 20}, t3 = {-300, 50 + std::sin(time * 3 + 1) * 24};
    DrawSplineSegmentBezierCubic(t0, t1, t2, t3, 34, K(kInk));
    DrawSplineSegmentBezierCubic(t0, t1, t2, t3, 24, K(iceD));
    DrawSplineSegmentBezierCubic({t0.x, t0.y - 4}, {t1.x, t1.y - 4}, {t2.x, t2.y - 4}, {t3.x, t3.y - 4}, 7, K(ice));
    Vector2 d = {t3.x - t2.x, t3.y - t2.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
    d = {d.x / l, d.y / l};
    Vector2 n = {-d.y, d.x};
    PolyO({Add(t3, Mul(d, 44)), Add(t3, Mul(n, 24)), Add(t3, Mul(d, 6)), Add(t3, Mul(n, -24))}, Hex(0x48e5ff), 5);
    // legs tucked under
    OLine({-30, 50}, {-40, 96}, 22, iceD, 5);
    OLine({30, 50}, {40, 96}, 22, ice, 5);
    for (int i = 0; i < 3; i++) Tri({34.0f + i * 9, 104}, {40.0f + i * 9, 104}, {38.0f + i * 9, 118}, K(horn));
    // the body
    PolyO({{-90, 10}, {-40, -50}, {50, -60}, {100, -20}, {90, 50}, {20, 80}, {-60, 60}}, ice, 6);
    Poly({{-70, 4}, {-34, -40}, {40, -48}, {20, -10}, {-40, 10}}, iceL);   // lit top
    PolyO({{-30, 30}, {60, 10}, {80, 40}, {20, 70}, {-40, 56}}, belly, 4);   // belly plate
    for (int i = 0; i < 4; i++) DrawLineEx({-24.0f + i * 26, 34.0f - i * 5}, {-28.0f + i * 26, 60.0f - i * 3}, 3, K(Fade2(Hex(0xa8c8e8), 0.9f)));
    // the neck and head
    PolyO({{60, -40}, {110, -110}, {150, -120}, {130, -60}, {96, -10}}, ice, 6);
    PolyO({{120, -150}, {200, -150}, {250, -128}, {246, -112}, {190, -98}, {130, -96}, {110, -120}}, ice, 6);   // skull and snout
    Poly({{126, -144}, {196, -144}, {236, -128}, {180, -124}, {130, -120}}, iceL);
    PolyO({{140, -150}, {120, -200}, {160, -156}}, horn, 4.5f);   // horns
    PolyO({{170, -150}, {168, -194}, {188, -152}}, horn, 4.5f);
    PolyO({{196, -104}, {246, -110}, {232, -92}, {196, -92}}, Hex(0x2a3a6a), 4);   // the jaw, a little open
    Tri({214, -104}, {220, -104}, {217, -96}, K(WHITE));
    Tri({230, -106}, {236, -106}, {233, -98}, K(WHITE));
    GlowK({178, -132}, 26, eye, 0.8f);
    OC({178, -132}, 8, eye, 3.5f);
    DrawEllipse(180, -132, 2, 6, K(kInk));
    // the near wing, over the body
    // (swept back, towards the tail, so the head stays clear)
    PolyO({{30, -40}, {0, -210 - flap * 60}, {-110, -250 - flap * 50}, {-90, -165 - flap * 30}, {-150, -130 - flap * 20}, {-30, -25}}, wingM, 5);
    DrawLineEx({20, -40}, {-110, -250 - flap * 50}, 5, K(wingC));
    DrawLineEx({10, -34}, {-150, -130 - flap * 20}, 4, K(wingC));
    rlPopMatrix();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    gHurt = 0;
    gAlpha = 1;
    // the declaration on its chest, and the garbage address it holds (it changes all the time)
    if (alpha < 0.05f) return;
    auto W2S = [&](Vector2 p) {   // local to screen: scale (mirrored), rotate, translate
        float x = p.x * face * k, y = p.y * k, r = tilt * face * DEG2RAD;
        return Vector2{c.x + x * std::cos(r) - y * std::sin(r), c.y + x * std::sin(r) + y * std::cos(r)};
    };
    float px = std::round(std::max(10.0f, 22 * k));
    Vector2 p1 = W2S({24, 36});
    Vector2 m1 = Measure("int *p;", px, F_MONOBOLD);
    Text("int *p;", p1.x - m1.x / 2, p1.y - px / 2, px, Fade2(Hex(0x2a4a80), alpha), F_MONOBOLD);
    unsigned junk = 0x7FFE0000u + ((unsigned)(time * 7) * 2654435761u >> 16 & 0xFFFF);
    std::string addr = TextFormat("p = 0x%08X ?", junk);
    float px2 = std::round(std::max(11.0f, 16 * scale));
    Vector2 m2 = Measure(addr, px2, F_MONOBOLD);
    Vector2 p2 = W2S({20, -270});
    Rectangle pill = {p2.x - m2.x / 2 - 9, p2.y - 4, m2.x + 18, px2 + 9};
    DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x101830), 0.85f * alpha));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Fade2(Hex(0xff5c7a), alpha));
    Text(addr, p2.x - m2.x / 2, p2.y, px2, Fade2(Hex(0xffb3c0), alpha), F_MONOBOLD);
}

// The Wyvern's aim: a dashed red line from its mouth to where it will shoot.
// lock: 0 still tracking .. 1 locked (solid, about to fire).
void DrawAimLine(Vector2 from, Vector2 to, float lock, float time) {
    Vector2 d = {to.x - from.x, to.y - from.y};
    float len = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
    d = {d.x / len, d.y / len};
    Color c = Fade2(Hex(0xff3d5a), 0.45f + 0.5f * lock);
    if (lock >= 1) DrawLineEx(from, to, 4, c);
    else
        for (float s = std::fmod(time * 120, 28.0f); s < len; s += 28)
            DrawLineEx(Add(from, Mul(d, s)), Add(from, Mul(d, std::min(len, s + 14))), 3, c);
    // a target ring with *p on it
    float r = 26 - 8 * lock;
    DrawRing(to, r, r + 3, 0, 360, 32, c);
    DrawLineEx({to.x - r - 8, to.y}, {to.x - r + 4, to.y}, 3, c);
    DrawLineEx({to.x + r - 4, to.y}, {to.x + r + 8, to.y}, 3, c);
    Text("*p", to.x + r + 6, to.y - r - 18, 16, c, F_MONOBOLD);
}

// The bolt it fires: an arrowhead of ice, flying along dir.
void DrawBolt(Vector2 at, Vector2 dir, float time) {
    Vector2 n = {-dir.y, dir.x};
    Glow(at, 46, Hex(0x48e5ff), 0.55f);
    for (int i = 1; i <= 5; i++) DrawCircleV(Add(at, Mul(dir, -i * 13.0f)), 8.0f - i, Fade2(Hex(0xbff6ff), 0.6f - i * 0.1f));
    Vector2 tip = Add(at, Mul(dir, 26)), l = Add(Add(at, Mul(dir, -8)), Mul(n, 14)), r = Add(Add(at, Mul(dir, -8)), Mul(n, -14));
    Tri(Add(tip, Mul(dir, 4)), Add(l, Mul(n, 3)), Add(r, Mul(n, -3)), kInk);
    Tri(tip, l, r, Hex(0x9ff5ff));
    Tri(tip, Add(at, Mul(n, 6)), at, WHITE);
    (void)time;
}

// --------------------------------------------------------------- Leak Slime ---
// A slime that leaks memory: it drips puddles of bytes and grows with every
// leak. size: 1 at first, bigger as it leaks. bytes: what it has malloc'd.
namespace {
// A slime's body: a dome with a flat, dripping base. In local units, ~130 wide.
void SlimeBody(Color fill, Color dark, Color light, float time, float wob) {
    std::vector<Vector2> body;
    for (int i = 0; i <= 14; i++) {   // the dome, left to right over the top
        float ang = PI + PI * i / 14.0f;
        float r = 66 + std::sin(time * 4 + i * 0.9f) * wob;
        body.push_back({std::cos(ang) * r, -46 + std::sin(ang) * (r - 6)});
    }
    body.push_back({64, -10});
    body.push_back({46, 0});
    body.push_back({-46, 0});
    body.push_back({-64, -10});
    for (size_t i = 0; i < body.size(); i++) { DrawLineEx(body[i], body[(i + 1) % body.size()], 11, K(kInk)); DrawCircleV(body[i], 5.5f, K(kInk)); }
    for (size_t i = 0; i < body.size(); i++) Tri({0, -40}, body[i], body[(i + 1) % body.size()], K(fill));
    DrawCircleSector({0, -46}, 60, 20, 100, 16, K(Fade2(dark, 0.55f)));   // shade, lower right
    DrawEllipse(-24, -84, 18, 11, K(Fade2(light, 0.85f)));               // the shine
    DrawCircleV({-6, -94}, 5, K(Fade2(WHITE, 0.9f)));
    // drips at the base
    for (int i = 0; i < 3; i++) {
        float x = -40.0f + i * 40, d = 6 + 6 * (0.5f + 0.5f * std::sin(time * 2.3f + i * 2.1f));
        DrawEllipse((int)x, (int)(d * 0.5f), 7, d, K(fill));
    }
}
} // namespace

void DrawSlime(Vector2 feet, float scale, int face, float hop, float size, float hurt, float alpha, int bytes, float time) {
    const float k = scale * 0.42f * size;
    float sq = 0.08f * std::sin(time * 6) - 0.12f * hop;   // wobbles; flattens before a hop
    BeginLocal(feet, face * k * (1 + sq), k * (1 - sq));
    gHurt = hurt;
    gAlpha = alpha;
    const Color slime = Hex(0x7be36b), slimeD = Hex(0x2f8a4c), slimeL = Hex(0xd8ffc8);
    GlowK({0, -50}, 120, Hex(0x7be36b), 0.25f);
    SlimeBody(slime, slimeD, slimeL, time, 2.5f);
    // bits floating inside it: the memory it leaked
    for (int i = 0; i < 5; i++) {
        float x = -36 + i * 18.0f + std::sin(time * 1.3f + i) * 5, y = -30 - std::fmod(time * 14 + i * 13, 50.0f);
        DrawRectangleRec({x, y, 7, 7}, K(Fade2(i % 2 ? Hex(0x2f8a4c) : Hex(0xffffff), 0.55f)));
    }
    // eyes (looking forward) and a little open mouth
    for (int i = 0; i < 2; i++) {
        Vector2 e = {(float)(i == 0 ? 6 : 34), -62};
        OC(e, 10, WHITE, 3.5f);
        DrawCircleV({e.x + 3, e.y + 1}, 5, K(kInk));
    }
    DrawEllipse(22, -38, 9, 6, K(Hex(0x1b4a2a)));
    EndLocal();
    if (alpha < 0.05f) return;
    // its badge: how much it has malloc'd (and never freed)
    std::string s = "malloc(" + std::to_string(bytes) + ")";
    float px = std::round(std::max(11.0f, 13 * scale));
    Vector2 m = Measure(s, px, F_MONOBOLD);
    Vector2 b = {feet.x, feet.y - (118 * k + 14) * (1 - sq) - px};
    Rectangle pill = {b.x - m.x / 2 - 7, b.y - 3, m.x + 14, px + 7};
    DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x0e2416), 0.85f * alpha));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Fade2(Hex(0x7be36b), alpha));
    Text(s, b.x - m.x / 2, b.y, px, Fade2(Hex(0xc8ffb8), alpha), F_MONOBOLD);
}

// A puddle of leaked bytes on the ground: stepping in it hurts. life: 1 new .. 0 gone.
void DrawPuddle(float x, float life, float time) {
    float a = std::min(1.0f, life * 3);
    float w = 42 * (0.6f + 0.4f * std::min(1.0f, (1 - life) * 8));
    DrawEllipse((int)x, (int)kGround + 2, w + 4, 11, Fade2(kInk, 0.5f * a));
    DrawEllipse((int)x, (int)kGround + 1, w, 8, Fade2(Hex(0x5bd14c), 0.9f * a));
    DrawEllipse((int)x - 10, (int)kGround - 1, w * 0.4f, 3, Fade2(Hex(0xd8ffc8), 0.7f * a));
    for (int i = 0; i < 3; i++) {   // bubbles popping
        float u = std::fmod(time * 1.5f + i * 0.33f + x * 0.01f, 1.0f);
        DrawCircleLines((int)(x - 20 + i * 20), (int)(kGround - 4 - u * 14), 3 + u * 2, Fade2(Hex(0xc8ffb8), a * (1 - u)));
    }
    Text("0x3F", x - 15, kGround - 26, 12, Fade2(Hex(0x9cf08a), 0.8f * a), F_MONOBOLD);
}

// ------------------------------------------------------------ the Leak Lord ---
// A king of slimes, swollen with every block that was malloc'd and never freed.
// mouth: 0 closed .. 1 wide open (spitting). melt: 0 whole .. 1 melted away.
void DrawLeakLord(Vector2 feet, float scale, float time, float mouth, float hurt, float alpha, float melt) {
    const float k = scale * 0.95f;
    float breathe = 0.03f * std::sin(time * 1.8f) * (1 - melt);
    BeginLocal(feet, k * (1 + breathe + melt * 0.6f), k * (1 - breathe - melt * 0.75f));
    gHurt = hurt;
    gAlpha = alpha;
    const Color slime = Hex(0x56c45a), slimeD = Hex(0x23703a), slimeL = Hex(0xc8ffb8), gold = Hex(0xffc14d), goldD = Hex(0xc98a1c);
    GlowK({0, -150}, 330, Hex(0x7be36b), 0.22f);
    // the body: a huge dome (the slime body, scaled up 2.4x)
    rlPushMatrix();
    rlScalef(2.4f, 2.4f, 1);
    SlimeBody(slime, slimeD, slimeL, time * 0.7f, 3.5f);
    rlPopMatrix();
    // the blocks it never freed, drifting inside it
    struct Blk { float x, y, w; const char* label; };
    const Blk blks[4] = {{-124, -54, 92, "malloc(64)"}, {-14, -40, 104, "malloc(128)"}, {60, -100, 84, "new int[8]"}, {58, -160, 80, "malloc(8)"}};
    for (int i = 0; i < 4; i++) {
        float dy = std::sin(time * 0.9f + i * 1.7f) * 8;
        Rectangle r = {blks[i].x, blks[i].y + dy, blks[i].w, 26};
        DrawRectangleRounded(r, 0.3f, 6, K(Fade2(Hex(0x1b4a2a), 0.45f)));
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2, K(Fade2(Hex(0xd8ffc8), 0.6f)));
    }
    // the crown, a little crooked
    PolyO({{-56, -232}, {-62, -282}, {-34, -256}, {-8, -296}, {16, -258}, {44, -290}, {52, -234}}, gold, 5);
    Poly({{-50, -238}, {-52, -268}, {-34, -250}, {-8, -282}, {-8, -244}}, Hex(0xffe08a));
    OC({-8, -262}, 9, Hex(0xff3d5a), 3.5f);
    DrawLineEx({-58, -238}, {52, -238}, 5, K(goldD));
    // the face: heavy-lidded eyes, a wide mouth that opens to spit
    for (int i = 0; i < 2; i++) {
        Vector2 e = {(float)(i == 0 ? -64 : 4), -176};
        OC(e, 22, WHITE, 4.5f);
        DrawCircleV({e.x - 6, e.y + 4}, 11, K(kInk));
        DrawCircleV({e.x - 9, e.y + 1}, 3.5f, K(WHITE));
        DrawRectangleRec({e.x - 26, e.y - 26, 52, 18}, K(slime));   // the lid
        DrawLineEx({e.x - 24, e.y - 8}, {e.x + 22, e.y - 10}, 5, K(kInk));
    }
    float mo = 10 + 46 * mouth;
    PolyO({{-92, -116}, {-30, -124 + mouth * 4}, {30, -116}, {20, -116 + mo}, {-40, -110 + mo * 1.15f}, {-86, -112 + mo * 0.6f}}, Hex(0x1b3a22), 5);
    for (int i = 0; i < 4; i++) Tri({-78.0f + i * 28, -118}, {-68.0f + i * 28, -118}, {-73.0f + i * 28, -104}, K(WHITE));
    EndLocal();
    if (alpha < 0.05f) return;
    float sx = k * (1 + breathe + melt * 0.6f), sy = k * (1 - breathe - melt * 0.75f);
    for (int i = 0; i < 4; i++) {
        float dy = std::sin(time * 0.9f + i * 1.7f) * 8;
        LocalText(blks[i].label, {blks[i].x + blks[i].w / 2, blks[i].y + dy + 13}, feet, sx, sy, 15, Fade2(Hex(0xe6ffe0), alpha * 0.9f), F_MONOBOLD);
    }
}

// A blob it spits: a ball of leaked memory, arcing through the air.
void DrawBlob(Vector2 at, float time) {
    Glow(at, 34, Hex(0x7be36b), 0.5f);
    DrawCircleV(at, 17, kInk);
    DrawCircleV(at, 13, Hex(0x7be36b));
    DrawCircleV({at.x - 4, at.y - 5}, 4, Hex(0xe6ffe0));
    DrawRectangleRec({at.x + 2, at.y + 1, 5, 5}, Fade2(Hex(0x1b4a2a), 0.7f));
    (void)time;
}

// --------------------------------------------------------------- Lost Node ---
// A list node that walked off on its own: a box [data|next] on two legs, with
// eyes peeking over its rim. Nodes walk in lines, each one's next pointing at
// the one behind it (the game draws those arrows with DrawLink).
namespace {
const float kNodeK = 0.45f;   // its drawing scale
const Color kNodeBox = Hex(0x2f9db0), kNodeBoxD = Hex(0x1d6c7c), kNodePaper = Hex(0xeef7f4), kLink = Hex(0xff9e4a);
}

Vector2 NodeNextDot(Vector2 feet, float scale, int face) {
    float k = scale * kNodeK;
    return {feet.x + face * 36 * k, feet.y - 84 * k};
}
Vector2 NodeInlet(Vector2 feet, float scale, int face) {
    float k = scale * kNodeK;
    return {feet.x - face * 66 * k, feet.y - 84 * k};
}

void DrawNode(Vector2 feet, float scale, int face, float walk, float hurt, float alpha, int value, bool head, bool tail, float time) {
    const float k = scale * kNodeK;
    float step = std::sin(walk) * 10, bob = std::fabs(std::sin(walk)) * 4;
    BeginLocal(feet, face * k, k);
    gHurt = hurt;
    gAlpha = alpha;
    const float ow = 3.2f;
    // legs
    OLine({-26 - step, -44}, {-28 - step, -8}, 10, kNodeBoxD, ow);
    OLine({22 + step, -44}, {24 + step, -8}, 10, kNodeBox, ow);
    PolyO({{-42 - step, -12}, {-16 - step, -12}, {-12 - step, 0}, {-44 - step, 0}}, Hex(0x2a2a3a), ow);
    PolyO({{10 + step, -12}, {36 + step, -12}, {40 + step, 0}, {8 + step, 0}}, Hex(0x2a2a3a), ow);
    // eyes peeking over the rim of the data box
    for (int i = 0; i < 2; i++) {
        Vector2 e = {-36.0f + i * 26, -126 + bob};
        OC(e, 11, WHITE, ow * 0.8f);
        DrawCircleV({e.x + 4, e.y + 2}, 5, K(kInk));
        DrawCircleV({e.x + 2, e.y}, 1.6f, K(WHITE));
    }
    // the box: data on the left, next on the right
    RR({-64, -120 + bob, 120, 76}, 10, kNodeBox, ow);
    DrawRectangleRounded({-56, -112 + bob, 66, 60}, 0.2f, 6, K(kNodePaper));
    DrawRectangleRounded({18, -112 + bob, 30, 60}, 0.2f, 6, K(Mix(kNodePaper, kNodeBox, 0.25f)));
    DrawRectangleRounded({-60, -118 + bob, 112, 8}, 1, 6, K(Fade2(WHITE, 0.25f)));   // the lit top edge
    DrawCircleV({33, -82 + bob}, 6, K(kLink));                                        // the next pointer
    if (tail) {   // nothing after it: next = NULL
        DrawLineEx({33, -82 + bob}, {33, -30}, 3, K(kLink));
        DrawLineEx({24, -30}, {42, -30}, 3, K(kLink));
        DrawLineEx({27, -24}, {39, -24}, 3, K(kLink));
        DrawLineEx({30, -18}, {36, -18}, 3, K(kLink));
    }
    EndLocal();
    if (alpha < 0.05f) return;
    LocalText(std::to_string(value).c_str(), {-23, -82 + bob}, feet, face * k, k, 34, Fade2(Hex(0x1d4a56), alpha), F_MONOBOLD);
    if (head) {   // the first of its line
        float px = std::round(std::max(11.0f, 13 * scale));
        Vector2 m = Measure("head", px, F_MONOBOLD);
        Vector2 b = {feet.x, feet.y - 152 * k - px};
        Rectangle pill = {b.x - m.x / 2 - 8, b.y - 3, m.x + 16, px + 7};
        DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x14282e), 0.85f * alpha));
        DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Fade2(kLink, alpha));
        Text("head", b.x - m.x / 2, b.y, px, Fade2(Hex(0xffd6b0), alpha), F_MONOBOLD);
    }
}

// An arrow from one node's next to the next node, sagging a little like a cable.
void DrawLink(Vector2 from, Vector2 to, float alpha, float time) {
    Vector2 mid = {(from.x + to.x) / 2, std::max(from.y, to.y) + 14 + std::sin(time * 3 + from.x * 0.01f) * 3};
    Color c = Fade2(kLink, alpha), ink = Fade2(kInk, 0.7f * alpha);
    DrawSplineSegmentBezierQuadratic(from, mid, to, 6, ink);
    DrawSplineSegmentBezierQuadratic(from, mid, to, 3, c);
    Vector2 d = {to.x - mid.x, to.y - mid.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
    d = {d.x / l, d.y / l};
    Vector2 n = {-d.y, d.x};
    Tri(to, Add(Add(to, Mul(d, -12)), Mul(n, 7)), Add(Add(to, Mul(d, -12)), Mul(n, -7)), c);
}

// ------------------------------------------------------- the Tangled List ---
// A spider made of a linked list: its body is a huge [HEAD|next] node, its legs
// are chains of little nodes, and a list hangs from its next down to NULL.
// pose: 0 standing .. 1 reared up (attacking). fall: 0 .. 1 coming undone.
void DrawTangledList(Vector2 feet, float scale, float time, float pose, float hurt, float alpha, float fall) {
    const float k = scale * 0.8f;
    BeginLocal(feet, k, k);
    gHurt = hurt;
    gAlpha = alpha;
    const float ow = 4;
    const float rise = -30 * pose + fall * 260, sway = std::sin(time * 1.4f) * 8 * (1 - fall);
    // the legs: chains of nodes from the body down to the ground, bent like a spider's
    struct Leg { float hipX, kneeX, kneeY, footX; };
    const Leg legs[4] = {{-110, -270, -420, -290}, {-60, -160, -470, -160}, {60, 160, -470, 160}, {110, 270, -420, 290}};
    for (int i = 0; i < 4; i++) {
        float walk = std::sin(time * 2.2f + i * 1.6f) * 14 * (1 - fall);
        Vector2 hip = {legs[i].hipX + sway, -280 + rise}, knee = {legs[i].kneeX + walk, legs[i].kneeY + rise * 0.6f}, foot = {legs[i].footX + walk * 0.5f, 0};
        if (fall > 0) { knee = Add(knee, {legs[i].kneeX * fall * 0.6f, -legs[i].kneeY * fall * 0.8f}); foot.x += legs[i].footX * fall * 0.5f; }
        Vector2 pts[3] = {hip, knee, foot};
        for (int s = 0; s < 2; s++) {   // a link along each segment, then nodes on it
            DrawLineEx(pts[s], pts[s + 1], 16, K(kInk));
            DrawLineEx(pts[s], pts[s + 1], 8, K(kLink));
            for (int j = 1; j <= 2; j++) {
                Vector2 c = Add(pts[s], Mul({pts[s + 1].x - pts[s].x, pts[s + 1].y - pts[s].y}, j / 3.0f));
                RR({c.x - 28, c.y - 19, 56, 38}, 7, i % 2 ? kNodeBox : kNodeBoxD, 3.5f);
                DrawRectangleRounded({c.x - 22, c.y - 13, 30, 26}, 0.2f, 6, K(Mix(kNodePaper, kNodeBox, 0.2f)));
                DrawRectangle((int)c.x + 9, (int)c.y - 19, 3, 38, K(kInk));
                DrawCircleV({c.x + 18, c.y}, 5, K(kLink));
            }
        }
        OC(knee, 18, kNodeBoxD, 3.5f);
        DrawCircleV({knee.x - 5, knee.y - 5}, 6, K(Fade2(WHITE, 0.3f)));
        PolyO({{foot.x - 22, foot.y - 14}, {foot.x + 22, foot.y - 14}, {foot.x + 14, foot.y}, {foot.x - 14, foot.y}}, Hex(0x2a2a3a), 3);
    }
    // the list hanging from its next pointer: [7] -> [3] -> NULL, swinging
    Vector2 from = {96 + sway, -300 + rise};
    for (int j = 0; j < 2; j++) {
        float swing = std::sin(time * 1.8f + j) * (16 + j * 10) * (1 - fall);
        Vector2 at = {130.0f + j * 30 + swing + sway, -190.0f + j * 80 + rise + fall * 120};
        DrawLineEx(from, at, 8, K(kInk));
        DrawLineEx(from, at, 3.5f, K(kLink));
        RR({at.x - 34, at.y, 68, 44}, 7, kNodeBox, 3);
        DrawRectangleRounded({at.x - 28, at.y + 6, 36, 32}, 0.2f, 6, K(kNodePaper));
        DrawCircleV({at.x + 22, at.y + 22}, 5, K(kLink));
        from = {at.x + 22, at.y + 22};
    }
    DrawLineEx(from, Add(from, {0, 50}), 3.5f, K(kLink));   // ... and the last next is NULL
    // the body: a huge node, HEAD in its data, angry eyes, its next glowing
    const float bx = -150 + sway, by = -380 + rise;
    GlowK({bx + 150, by + 80}, 260, Hex(0x5fd0e0), 0.18f * (1 - fall));
    RR({bx, by, 300, 150}, 18, kNodeBox, ow);
    DrawRectangleRounded({bx + 12, by + 12, 190, 126}, 0.15f, 8, K(kNodePaper));
    DrawRectangleRounded({bx + 216, by + 12, 72, 126}, 0.15f, 8, K(Mix(kNodePaper, kNodeBox, 0.3f)));
    DrawRectangleRounded({bx + 8, by + 6, 284, 12}, 1, 6, K(Fade2(WHITE, 0.25f)));
    for (int i = 0; i < 2; i++) {   // the eyes, glaring out of the data box
        Vector2 e = {bx + 62 + i * 70.0f, by + 56};
        OC(e, 22, WHITE, 3.5f);
        DrawCircleV({e.x - 6 + 4 * std::sin(time), e.y + 4}, 11, K(kInk));
        DrawLineEx({e.x - 26, e.y - 30 + i * 10}, {e.x + 24, e.y - 22 - i * 10}, 7, K(kInk));   // brows, cross
    }
    GlowK({bx + 252, by + 75}, 40, kLink, 0.6f);
    OC({bx + 252, by + 75}, 13, kLink, 3.5f);
    EndLocal();
    if (alpha < 0.05f) return;
    LocalText("HEAD", {bx + 107, by + 118}, feet, k, k, 30, Fade2(Hex(0x1d4a56), alpha), F_MONOBOLD);
    LocalText("7", {130.0f + std::sin(time * 1.8f) * 16 * (1 - fall) + sway - 10, -168.0f + rise + fall * 120}, feet, k, k, 24, Fade2(Hex(0x1d4a56), alpha), F_MONOBOLD);
    LocalText("3", {160.0f + std::sin(time * 1.8f + 1) * 26 * (1 - fall) + sway - 10, -88.0f + rise + fall * 120}, feet, k, k, 24, Fade2(Hex(0x1d4a56), alpha), F_MONOBOLD);
    LocalText("NULL", {from.x, from.y + 64}, feet, k, k, 22, Fade2(Hex(0xff8a9c), alpha), F_MONOBOLD);
}

// A node lying on the street for the Tangled List's traverse: lit when p is
// about to land on it (warn 0..1), struck when it just has.
void DrawListTile(float x, int value, float warn, float struck, float time) {
    Rectangle r = {x - 46, kGround - 30, 92, 30};
    Color box = Mix(Hex(0x2f9db0), Hex(0xff5c7a), std::max(warn * (0.6f + 0.4f * std::sin(time * 20)), struck));
    DrawRectangleRounded({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 0.3f, 6, kInk);
    DrawRectangleRounded(r, 0.3f, 6, box);
    DrawRectangleRounded({r.x + 5, r.y + 4, 52, 22}, 0.25f, 6, Hex(0xeef7f4));
    TextCenter(std::to_string(value), {r.x + 5, r.y + 4, 52, 22}, 15, Hex(0x1d4a56), F_MONOBOLD);
    DrawCircleV({r.x + 74, r.y + 15}, 5, Hex(0xff9e4a));
}

// The traversal's pointer: an arrow labelled p, pointing down at a node.
void DrawCursorP(Vector2 tip, float time) {
    float b = std::sin(time * 10) * 3;
    Vector2 t = {tip.x, tip.y + b};
    Glow(t, 36, Hex(0xffc14d), 0.4f);
    Tri({t.x, t.y}, {t.x - 16, t.y - 22}, {t.x + 16, t.y - 22}, kInk);
    Tri({t.x, t.y - 4}, {t.x - 11, t.y - 20}, {t.x + 11, t.y - 20}, Hex(0xffc14d));
    DrawLineEx({t.x, t.y - 22}, {t.x, t.y - 52}, 6, kInk);
    DrawLineEx({t.x, t.y - 22}, {t.x, t.y - 50}, 3, Hex(0xffc14d));
    Rectangle pill = {t.x + 8, t.y - 68, 28, 26};
    DrawRectangleRounded(pill, 0.5f, 8, Fade2(Hex(0x1e1c26), 0.9f));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.5f, Hex(0xffc14d));
    TextCenter("p", pill, 20, Hex(0xffe08a), F_MONOBOLD);
}

// The whip: a chain of nodes sliding along the street. Jump it.
void DrawChainWhip(Vector2 at, float time) {
    for (int i = 0; i < 4; i++) {
        float x = at.x + i * 46, y = at.y - 18 - std::fabs(std::sin(time * 12 + i)) * 10;
        if (i < 3) { DrawLineEx({x + 14, y}, {x + 46 - 16, at.y - 18}, 5, kInk); DrawLineEx({x + 14, y}, {x + 30, at.y - 18}, 2.5f, Hex(0xff9e4a)); }
        DrawRectangleRounded({x - 19, y - 15, 38, 30}, 0.3f, 6, kInk);
        DrawRectangleRounded({x - 16, y - 12, 32, 24}, 0.3f, 6, i == 0 ? Hex(0xff5c7a) : Hex(0x2f9db0));
        DrawCircleV({x + 8, y}, 3.5f, Hex(0xff9e4a));
    }
    for (int i = 0; i < 5; i++)   // sparks off the cobbles
        DrawCircleV({at.x + 160 + i * 8.0f, at.y - 4 - std::fmod(time * 90 + i * 13, 22.0f)}, 2.5f, Fade2(Hex(0xffe08a), 0.7f - i * 0.12f));
}

// ------------------------------------------------------------------ story ---
// ------------------------------------------------------------ Dangling Bat ---
// A bat still pointing at memory that was already freed: a dashed pointer
// hangs from it to a crossed-out block. flap: phase of the wing beat.
void DrawBat(Vector2 at, float scale, int face, float flap, float hurt, float alpha, float time) {
    const float k = scale * 0.45f;
    BeginLocal(at, face * k, k);
    gHurt = hurt;
    gAlpha = alpha;
    const float ow = 3.2f;
    const Color fur = Hex(0x4a2f6e), furD = Hex(0x2e1c48), furL = Hex(0x7a58a8), wing = Hex(0x3a2458), wingM = Hex(0x6a3f8a), eye = Hex(0xff4d6d);
    float w = std::sin(flap) * 40;
    // the dangling pointer: a dashed line down to a block that was freed
    for (int i = 0; i < 6; i++) DrawLineEx({4.0f + i * 6, -30.0f + i * 14}, {7.0f + i * 6, -22.0f + i * 14}, 4, K(Hex(0xffc14d)));
    RR({18, 54, 66, 36}, 6, Hex(0x5a5068), 3);
    DrawLineEx({22, 58}, {80, 86}, 4, K(Hex(0xff5c7a)));
    DrawLineEx({80, 58}, {22, 86}, 4, K(Hex(0xff5c7a)));
    // wings: a membrane between three long fingers
    for (int side = -1; side <= 1; side += 2) {
        Vector2 sh = {side * 18.0f, -60};
        Vector2 tip1 = {side * 120.0f, -110 - w}, tip2 = {side * 140.0f, -50 - w * 0.6f}, tip3 = {side * 90.0f, -10 - w * 0.2f};
        PolyO({sh, tip1, {side * 96.0f, -70 - w * 0.7f}, tip2, {side * 80.0f, -36 - w * 0.4f}, tip3, {side * 20.0f, -36}}, side < 0 ? wing : wingM, ow);
        DrawLineEx(sh, tip1, 3, K(furD));
        DrawLineEx(sh, tip2, 3, K(furD));
    }
    // the body, the ears, the face
    OC({0, -52}, 30, fur, ow);
    DrawCircleSector({0, -52}, 30, 180, 275, 12, K(Fade2(furL, 0.7f)));
    PolyO({{-22, -74}, {-30, -112}, {-6, -80}}, fur, ow);
    PolyO({{22, -74}, {30, -112}, {6, -80}}, fur, ow);
    for (int i = 0; i < 2; i++) {
        Vector2 e = {(float)(i == 0 ? -10 : 12), -58};
        GlowK(e, 14, eye, 0.6f);
        OC(e, 6, eye, 2);
    }
    Tri({-6, -40}, {-1, -40}, {-3.5f, -31}, K(WHITE));
    Tri({4, -40}, {9, -40}, {6.5f, -31}, K(WHITE));
    EndLocal();
    if (alpha > 0.05f) LocalText("freed", {51, 104}, at, face * k, k, 22, Fade2(Hex(0xffb3c0), alpha), F_MONOBOLD);
    (void)time;
}

// ---------------------------------------------------- the SEGFAULT's attacks ---
// Where an error column will fall: a red mark that tightens as it comes.
void DrawBeamMark(float x, float warn, float time) {
    float w = 50 - 26 * warn, pulse = 0.5f + 0.5f * std::sin(time * 24);
    DrawEllipse((int)x, (int)kGround + 3, w + 6, 10, Fade2(Hex(0x2a0a14), 0.45f));
    DrawEllipseLines((int)x, (int)kGround + 3, w, 8, Fade2(Hex(0xff3d5a), 0.5f + 0.5f * pulse));
    DrawLineEx({x - 12, kGround - 10}, {x + 12, kGround + 14}, 4, Fade2(Hex(0xff3d5a), 0.4f + 0.6f * warn));
    DrawLineEx({x + 12, kGround - 10}, {x - 12, kGround + 14}, 4, Fade2(Hex(0xff3d5a), 0.4f + 0.6f * warn));
    DrawRectangle((int)x - 1, 0, 2, (int)kGround, Fade2(Hex(0xff3d5a), 0.12f * warn));   // a faint line up to the sky
}

// The column itself: a beam of crash, the error code running down it. life 1 .. 0.
void DrawBeam(float x, float life, float time) {
    float w = 34 * std::min(1.0f, life * 2.5f);
    DrawRectangle((int)(x - w * 1.8f), 0, (int)(w * 3.6f), (int)kGround, Fade2(Hex(0xff3d5a), 0.18f * life));
    DrawRectangle((int)(x - w / 2), 0, (int)w, (int)kGround, Fade2(Hex(0xff6a80), 0.85f * life));
    DrawRectangle((int)(x - w / 6), 0, (int)(w / 3), (int)kGround, Fade2(WHITE, 0.9f * life));
    for (int i = 0; i < 6; i++) {
        float y = std::fmod(time * 900 + i * 130, kGround);
        Text("0xC0000005", x + w / 2 + 6, y, 12, Fade2(Hex(0xffd0d8), 0.8f * life), F_MONOBOLD);
    }
    Glow({x, kGround}, 80, Hex(0xff3d5a), 0.6f * life);
}

// The broken pointer it throws, flying flat along the ground.
void DrawSpear(Vector2 at, float dir, float time) {
    float tail = at.x - dir * 150;
    for (int i = 0; i < 6; i++) DrawLineEx({tail - dir * i * 18.0f, at.y}, {tail - dir * (i * 18.0f + 10), at.y}, 3, Fade2(Hex(0xffb050), 0.5f - i * 0.08f));
    DrawLineEx({tail, at.y}, {at.x, at.y}, 14, kInk);
    DrawLineEx({tail, at.y}, {at.x, at.y}, 8, Hex(0xff9e4a));
    DrawLineEx({tail, at.y - 2}, {at.x, at.y - 2}, 2.5f, Hex(0xffd29a));
    Vector2 tip = {at.x + dir * 34, at.y};
    Tri({tip.x + dir * 4, tip.y}, {at.x - dir * 4, at.y - 22}, {at.x - dir * 4, at.y + 22}, kInk);
    Tri(tip, {at.x, at.y - 17}, {at.x, at.y + 17}, Hex(0xff9e4a));
    Glow(tip, 30, Hex(0xffb050), 0.4f);
    (void)time;
}

// The blue screen sweeping the arena: only a dash gets through it.
void DrawBsodWave(float x, float time) {
    const Color blue = Hex(0x1f50c8);
    DrawRectangleGradientH((int)x - 60, 0, 60, (int)kGround, Fade2(blue, 0), Fade2(blue, 0.85f));
    DrawRectangle((int)x, 0, 46, (int)kGround, Fade2(blue, 0.92f));
    DrawRectangleGradientH((int)x + 46, 0, 80, (int)kGround, Fade2(blue, 0.85f), Fade2(blue, 0));
    for (int y = 0; y < (int)kGround; y += 6) DrawRectangle((int)x - 40, y, 120, 2, Fade2(BLACK, 0.15f));
    for (int i = 0; i < 4; i++) {   // the sad face and the code, scrolling down it
        float y = std::fmod(time * 220 + i * 170, kGround - 30);
        Text(i % 2 ? ":(" : "0x0D", x + 6, y, i % 2 ? 28 : 13, Fade2(WHITE, 0.85f), i % 2 ? F_HEAVY : F_MONOBOLD);
    }
}

// --------------------------------------------------------------------- Ada ---
// Princess Ada, who keeps the Kernel running: a teal gown, dark hair, and a
// little golden crown with { } on it.
void DrawAda(Vector2 feet, float s, float time) {
    const float k = s * 0.52f;
    BeginLocal(feet, k, k);
    const float ow = 3.2f;
    const Color gown = Hex(0x2bb5a8), gownD = Hex(0x1d8379), gownL = Hex(0x7fe0d4), hair = Hex(0x3a2a5a), hairL = Hex(0x5a4482), skin = Hex(0xf2cfb4), gold = Hex(0xe8b44a);
    float sway = std::sin(time * 1.5f) * 4, breathe = std::sin(time * 2.0f) * 1.2f;
    auto U = [&](float x, float y) { return Vector2{x, y + breathe}; };
    // long hair, behind
    PolyO({U(-30, -196), U(30, -196), U(36, -120), U(22, -96), U(-22, -96), U(-36, -120)}, hair, ow);
    // the gown: a wide skirt, then the bodice
    PolyO({U(-24, -128), U(24, -128), {58 + sway, -6}, {50 + sway, 0}, {-50 + sway, 0}, {-58 + sway, -6}}, gown, ow);
    Poly({U(-20, -124), U(-4, -124), {-10 + sway, -6}, {-48 + sway, -6}}, gownL);
    Poly({U(10, -124), U(22, -124), {54 + sway, -8}, {26 + sway, -8}}, gownD);
    DrawLineEx({-50 + sway, -10}, {50 + sway, -10}, 5, K(gold));   // a golden hem
    PolyO({U(-22, -168), U(22, -168), U(26, -126), U(-26, -126)}, gownL, ow);
    DrawLineEx(U(-24, -132), U(24, -132), 4, K(gold));
    // arms folded in front
    OLine(U(-20, -160), U(-8, -138), 10, skin, ow);
    OLine(U(20, -160), U(8, -138), 10, skin, ow);
    // the head: face, hair framing it, the crown
    OC(U(0, -192), 24, skin, ow);
    PolyO({U(-26, -196), U(-20, -216), U(0, -222), U(20, -216), U(26, -196), U(14, -208), U(-14, -208)}, hair, ow * 0.8f);
    DrawCircleV(U(-8, -210), 6, K(hairL));
    for (int i = 0; i < 2; i++) DrawCircleV(U(i == 0 ? -8.0f : 8.0f, -190), 2.8f, K(Hex(0x2a1d45)));
    DrawLineEx(U(-5, -180), U(5, -180), 2.5f, K(Hex(0xc0566a)));
    DrawCircleV(U(-14, -184), 4, K(Fade2(Hex(0xff9aa8), 0.5f)));
    DrawCircleV(U(14, -184), 4, K(Fade2(Hex(0xff9aa8), 0.5f)));
    PolyO({U(-18, -220), U(-18, -234), U(-9, -226), U(0, -238), U(9, -226), U(18, -234), U(18, -220)}, gold, ow * 0.8f);
    EndLocal();
    LocalText("{ }", {0, -226 + breathe}, feet, k, k, 14, Hex(0x7a4a00), F_MONOBOLD);
}

void DrawSegfault(Vector2 c, float s, float time, float hurt, float alpha) {
    const float k = s * 0.74f;
    const Vector2 o = {c.x - 600 * k, c.y - 720 * k};   // where local (0, 0) lands
    BeginLocal(o, k, k);
    gHurt = hurt;
    gAlpha = alpha;
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
    if (alpha < 0.05f) return;
    auto T = [&](const char* str, float x, float y, float size, Color col, FontKind f) { LocalText(str, {x, y}, o, k, k, size, Fade2(col, (col.a / 255.0f) * alpha), f); };
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
// The lands' paintings (by Ombremonde, CC0: see the README), loaded the first
// time each one is needed: 0 plains, 1 canyon, 2 peaks, 3 forest, 4 city, 5 the Heap.
const char* kBackdropFile[6] = {"stack_plains.jpg", "array_canyon.jpg", "pointer_peaks.jpg", "memory_forest.jpg", "list_city.jpg", "the_heap.jpg"};
Texture2D gBackdrop[6] = {};
bool gBackdropTried[6] = {};
std::string gResDir;

Texture2D LoadBackdrop(const std::string& path) {
    Image img = LoadImage(path.c_str());
    if (!img.data) return {};   // no picture: that land is drawn by hand instead
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&tex);   // drawn smaller than it is: keep it smooth
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    return tex;
}

const Texture2D& Bd(int i) {
    if (!gBackdropTried[i] && !gResDir.empty()) {
        gBackdropTried[i] = true;
        gBackdrop[i] = LoadBackdrop(gResDir + "/backgrounds/" + kBackdropFile[i]);
    }
    return gBackdrop[i];
}
}

void LoadArt(const std::string& resourceDir) { gResDir = resourceDir; }

// A land's painting filling r (cropped to fit), drifting and zooming slowly,
// darkened a little so the characters drawn over it stand out.
void DrawBackdropIn(Rectangle r, int which, float time) {
    if (which < 0 || which > 5) return;
    const Texture2D& tex = Bd(which);
    if (tex.id == 0) return;
    float zoom = 1.10f + 0.04f * std::sin(time * 0.11f);
    float aspect = r.width / r.height;
    float sw = tex.width / zoom, sh = sw / aspect;
    if (sh > tex.height / zoom) { sh = tex.height / zoom; sw = sh * aspect; }
    float sx = (tex.width - sw) * (0.5f + 0.4f * std::sin(time * 0.07f)), sy = (tex.height - sh) * 0.55f;
    DrawTexturePro(tex, {sx, sy, sw, sh}, r, {0, 0}, 0, WHITE);
    DrawRectangleRec(r, Fade2(Hex(0x0b0a14), 0.25f));
    DrawRectangleGradientV((int)r.x, (int)(r.y + r.height * 0.45f), (int)r.width, (int)(r.height * 0.55f) + 1, Fade2(Hex(0x0b0a14), 0), Fade2(Hex(0x0b0a14), 0.6f));
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
    const bool painted = Bd(0).id != 0;
    if (painted) PaintedSky(Bd(0), cam, scr);
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
            Rectangle pl = {x - 46 + k * 4, y - k * 16, 92.0f - k * 8, 13};
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
    // (by day the earthy greens and browns of the painted meadow)
    const Color soil = painted ? Hex(0x3e3222) : Hex(0x1a1430), grass = painted ? Hex(0x8fbf4e) : Hex(0x3fbf6a);
    const Color grassD = painted ? Hex(0x6a9440) : Hex(0x2a8a4c), tile0 = painted ? Hex(0x55452e) : Hex(0x2a2148);
    const Color tile1 = painted ? Hex(0x4a3c28) : Hex(0x221b3d), tuft = painted ? Hex(0xb2dc6e) : Hex(0x5ad87f);
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
    const bool painted = Bd(1).id != 0;
    if (painted) PaintedSky(Bd(1), cam, scr);
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

// ----------------------------------------------------------- Pointer Peaks ---
void DrawPointerPeaks(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = Bd(2).id != 0;
    if (painted) PaintedSky(Bd(2), cam, scr);
    else {   // by hand: a cold sky and sharp peaks
        DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0xbcdcff), Hex(0xe6f2ff));
        struct Layer { float speed, base, amp, freq; unsigned col; };
        const Layer layers[3] = {{0.15f, 430, 140, 0.004f, 0xc4d2ec}, {0.35f, 520, 90, 0.007f, 0xa8b8dc}, {0.6f, 590, 40, 0.012f, 0x8ea2cc}};
        for (const Layer& ly : layers)
            for (float x = L; x < R; x += 6) {
                float wx = x + cam * ly.speed;
                float s = std::fabs(std::fmod(wx * ly.freq, 2.0f) - 1);   // a zig-zag: sharp peaks
                float hh = ly.base - ly.amp * (1 - s);
                DrawRectangle((int)x, (int)hh, 7, (int)(kGround - hh) + 1, Hex(ly.col));
            }
    }
    // floating pointers: a box with an arrow to the value it points at
    for (int i = 0; i < 6; i++) {
        float wx = i * 760.0f + 260;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4560 * 10, 4560.0f) + L - 400;
        if (x < L - 300 || x > R + 300) continue;
        float y = 150 + (i % 3) * 80 + std::sin(time * 0.7f + i * 1.9f) * 9;
        const Color fill = Fade2(Hex(0xf6fbff), 0.6f), line = Fade2(Hex(0x7a90c0), 0.6f), ink = Fade2(Hex(0x5a6e9a), 0.85f);
        Rectangle p = {x - 70, y, 40, 30}, v = {x + 30, y, 40, 30};
        DrawRectangleRounded(p, 0.2f, 6, fill);
        DrawRectangleRoundedLinesEx(p, 0.2f, 6, 1.5f, line);
        DrawRectangleRounded(v, 0.2f, 6, fill);
        DrawRectangleRoundedLinesEx(v, 0.2f, 6, 1.5f, line);
        DrawCircleV({p.x + 20, p.y + 15}, 4, ink);
        DrawLineEx({p.x + 24, p.y + 15}, {v.x - 6, v.y + 15}, 2, ink);
        Tri({v.x - 2, v.y + 15}, {v.x - 10, v.y + 10}, {v.x - 10, v.y + 20}, ink);
        TextCenter(std::to_string((i * 13 + 7) % 50), v, 14, ink, F_MONOBOLD);
        TextCenter(i % 2 ? "p" : "q", {p.x, p.y + 32, p.width, 14}, 11, ink, F_MONOBOLD);
    }
    // snow, drifting down (two layers, the near one faster)
    for (int i = 0; i < 70; i++) {
        float layer = i % 2 ? 1.0f : 0.55f;
        float sx = std::fmod(i * 97.0f - cam * 0.3f * layer + std::sin(time * 0.8f + i) * 18 + 100000, scr.width + 40) + L - 20;
        float sy = std::fmod(i * 53.0f + time * 40 * layer, kGround + 20) - 10;
        DrawCircleV({sx, sy}, 1.5f + layer * 1.6f, Fade2(WHITE, 0.55f + 0.3f * layer));
    }
    // the ground: snow on top, blocks of ice below
    const Color soil = Hex(0x3a4a72), snow = Hex(0xf4f8ff), snowD = Hex(0xc8d6f0), ice0 = Hex(0x6f8ec4), ice1 = Hex(0x5f7cb2);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 14, snow);
    DrawRectangle((int)L, (int)kGround + 14, (int)scr.width + 1, 6, snowD);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        float wx = x + cam;
        int seed = (int)std::floor(wx / 64);
        for (int row = 0; row < 3; row++) {
            float y = kGround + 32 + row * 40.0f, xo = (row % 2) * 32.0f;
            DrawRectangleRounded({x + xo + 4, y, 56, 30}, 0.2f, 4, row == 0 ? ice0 : ice1);
            DrawRectangleRounded({x + xo + 8, y + 3, 48, 4}, 1, 4, Fade2(Hex(0xa8c4ee), 0.6f));
        }
        if (seed % 3 == 0) DrawEllipse((int)x + 20, (int)kGround + 1, 16, 6, snow);   // little drifts
        if (seed % 5 == 2) {   // an icicle hanging from the edge
            Tri({x + 44, kGround + 14}, {x + 52, kGround + 14}, {x + 48, kGround + 34}, Hex(0xdcecff));
        }
    }
}

// ----------------------------------------------------------- Memory Forest ---
void DrawMemoryForest(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = Bd(3).id != 0;
    if (painted) PaintedSky(Bd(3), cam, scr);
    else {   // by hand: a dark green wood
        DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0x0c1a14), Hex(0x1f3a2a));
        for (int i = 0; i < 14; i++) {
            float x = std::fmod(i * 173.0f - cam * 0.3f + 10000, scr.width + 200) + L - 100;
            DrawRectangle((int)x, 0, 26 + i % 3 * 10, (int)kGround, Hex(0x0f2218));
        }
    }
    // leaked blocks drifting among the trees: dashed outlines, nobody owns them now
    for (int i = 0; i < 6; i++) {
        float wx = i * 760.0f + 380;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4560 * 10, 4560.0f) + L - 400;
        if (x < L - 300 || x > R + 300) continue;
        float y = 160 + (i % 3) * 80 + std::sin(time * 0.6f + i * 2.3f) * 10;
        Rectangle b = {x - 50, y, 100, 30};
        const Color c = Fade2(Hex(0x9cf5c8), 0.55f);
        DrawRectangleRounded(b, 0.25f, 6, Fade2(Hex(0x0e2a1c), 0.45f));
        for (float dx = 0; dx < b.width; dx += 12) {   // a dashed outline
            DrawRectangle((int)(b.x + dx), (int)b.y, 6, 2, c);
            DrawRectangle((int)(b.x + dx), (int)(b.y + b.height - 2), 6, 2, c);
        }
        TextCenter(i % 2 ? "64 bytes" : "leaked", b, 13, Fade2(Hex(0xc8ffe8), 0.75f), F_MONOBOLD);
    }
    // fireflies: bits of memory glowing as they drift up
    for (int i = 0; i < 40; i++) {
        float layer = i % 2 ? 1.0f : 0.6f;
        float fx = std::fmod(i * 131.0f - cam * 0.35f * layer + std::sin(time * 0.7f + i) * 24 + 100000, scr.width + 40) + L - 20;
        float fy = kGround - 20 - std::fmod(i * 37.0f + time * 18 * layer, kGround - 60);
        float tw = 0.5f + 0.5f * std::sin(time * 3 + i * 1.7f);
        // a cheap glow (three soft rings): forty full glows cost more than the rest of the forest
        DrawCircleSector({fx, fy}, 10 * layer, 0, 360, 10, Fade2(Hex(0x6ff5d0), 0.08f * tw));
        DrawCircleSector({fx, fy}, 5.5f * layer, 0, 360, 8, Fade2(Hex(0x6ff5d0), 0.16f * tw));
        DrawCircleV({fx, fy}, 1.6f * layer + 0.4f, Fade2(Hex(0xd8fff2), 0.6f + 0.4f * tw));
    }
    // the ground: dark moss, rooty soil, glowing mushrooms
    const Color soil = Hex(0x14231c), moss = Hex(0x3f7a4a), mossD = Hex(0x2a5636), stone0 = Hex(0x23392e), stone1 = Hex(0x1d3127);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 14, moss);
    DrawRectangle((int)L, (int)kGround + 14, (int)scr.width + 1, 6, mossD);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        float wx = x + cam;
        int seed = (int)std::floor(wx / 64);
        for (int row = 0; row < 3; row++) {
            float y = kGround + 32 + row * 40.0f, xo = (row % 2) * 32.0f;
            DrawRectangleRounded({x + xo + 4, y, 56, 30}, 0.2f, 4, row == 0 ? stone0 : stone1);
        }
        if (((seed % 4) + 4) % 4 == 1) {   // a root, curling into the soil
            DrawSplineSegmentBezierCubic({x + 10, kGround + 12}, {x + 22, kGround + 40}, {x + 40, kGround + 30}, {x + 50, kGround + 64}, 5, Hex(0x2a1d14));
        }
        if (((seed % 5) + 5) % 5 == 2) {   // a glowing mushroom
            float gx = x + 30;
            Glow({gx, kGround - 10}, 26, Hex(0x6ff5d0), 0.35f + 0.15f * std::sin(time * 2 + seed));
            DrawRectangle((int)gx - 2, (int)kGround - 12, 4, 12, Hex(0xd8e8d0));
            DrawCircleSector({gx, kGround - 12}, 10, 180, 360, 12, Hex(0x6ff5d0));
        }
        if (((seed % 3) + 3) % 3 == 0) {   // tufts of grass
            Tri({x + 8, kGround + 2}, {x + 12, kGround - 12}, {x + 16, kGround + 2}, Hex(0x5c9c5a));
            Tri({x + 14, kGround + 2}, {x + 20, kGround - 8}, {x + 22, kGround + 2}, Hex(0x5c9c5a));
        }
    }
}

// --------------------------------------------------------- Linked List City ---
void DrawListCity(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = Bd(4).id != 0;
    if (painted) PaintedSky(Bd(4), cam, scr);
    else {   // by hand: an evening sky and rows of houses
        DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0x2a2f5a), Hex(0xd88a6a));
        for (int i = 0; i < 20; i++) {
            float x = std::fmod(i * 120.0f - cam * 0.3f + 10000, scr.width + 200) + L - 100, h = 140 + (i * 53) % 160;
            DrawRectangle((int)x, (int)(kGround - h), 90, (int)h, Hex(0x2a2440));
        }
    }
    // lists drifting over the roofs: three nodes and NULL
    for (int i = 0; i < 5; i++) {
        float wx = i * 900.0f + 300;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4500 * 10, 4500.0f) + L - 400;
        if (x < L - 400 || x > R + 300) continue;
        float y = 150 + (i % 3) * 85 + std::sin(time * 0.7f + i * 1.3f) * 8;
        const Color box = Fade2(Hex(0xf6fbff), 0.55f), line = Fade2(Hex(0x6a7aa8), 0.6f), ink = Fade2(Hex(0x4a5680), 0.85f);
        for (int j = 0; j < 3; j++) {
            Rectangle b = {x + j * 82.0f, y, 58, 28};
            DrawRectangleRounded(b, 0.2f, 6, box);
            DrawRectangleRoundedLinesEx(b, 0.2f, 6, 1.5f, line);
            DrawLineEx({b.x + 38, b.y}, {b.x + 38, b.y + 28}, 1.5f, line);
            TextCenter(std::to_string((i * 17 + j * 5) % 90 + 1), {b.x, b.y, 38, 28}, 13, ink, F_MONOBOLD);
            DrawCircleV({b.x + 48, b.y + 14}, 3, ink);
            float ex = j < 2 ? b.x + 82 : b.x + 92;
            DrawLineEx({b.x + 48, b.y + 14}, {ex - 4, b.y + 14}, 1.5f, ink);
            Tri({ex, b.y + 14}, {ex - 7, b.y + 10}, {ex - 7, b.y + 18}, ink);
            if (j == 2) Text("NULL", ex + 4, b.y + 7, 12, ink, F_MONOBOLD);
        }
    }
    // the street: cobbles, a kerb, lamp posts that glow
    const Color stoneTop = Hex(0x8a8070), kerb = Hex(0x6a6258), soil = Hex(0x2e2a30), cob0 = Hex(0x4e4852), cob1 = Hex(0x433e48);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 12, stoneTop);
    DrawRectangle((int)L, (int)kGround + 12, (int)scr.width + 1, 6, kerb);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        float wx = x + cam;
        int seed = (int)std::floor(wx / 64);
        for (int row = 0; row < 3; row++) {   // rounded cobbles, staggered
            float y = kGround + 28 + row * 38.0f, xo = (row % 2) * 32.0f;
            DrawRectangleRounded({x + xo + 3, y, 58, 32}, 0.55f, 6, ((seed + row) % 3) ? cob0 : cob1);
            DrawRectangleRounded({x + xo + 10, y + 4, 40, 5}, 1, 4, Fade2(Hex(0x8a8098), 0.35f));
        }
        if (((seed % 7) + 7) % 7 == 3) {   // a lamp post
            float lx = x + 32;
            Glow({lx, kGround - 132}, 70, Hex(0xffc878), 0.45f + 0.05f * std::sin(time * 3 + seed));
            DrawRectangle((int)lx - 3, (int)kGround - 126, 6, 126, Hex(0x1e1c26));
            DrawRectangle((int)lx - 9, (int)kGround - 10, 18, 10, Hex(0x1e1c26));
            DrawRectangleRounded({lx - 10, kGround - 146, 20, 22}, 0.3f, 6, Hex(0x1e1c26));
            DrawRectangleRounded({lx - 7, kGround - 143, 14, 16}, 0.3f, 6, Hex(0xffe0a0));
        }
    }
}

// ----------------------------------------------------------------- The Heap ---
void DrawTheHeap(float cam, float time) {
    Rectangle scr = ScreenRect();
    const float L = scr.x, R = scr.x + scr.width;
    const bool painted = Bd(5).id != 0;
    if (painted) PaintedSky(Bd(5), cam, scr);
    else DrawRectangleGradientV((int)L, 0, (int)scr.width + 1, (int)kGround, Hex(0x1a0a2a), Hex(0x5a2a6a));
    // freed blocks drifting up, their addresses fading
    for (int i = 0; i < 7; i++) {
        float wx = i * 700.0f + 200;
        float x = wx - cam * 0.45f;
        x = std::fmod(x - L + 4900 * 10, 4900.0f) + L - 400;
        if (x < L - 300 || x > R + 300) continue;
        float y = 120 + std::fmod(i * 97.0f + time * 12, 380.0f);
        float a = 0.55f * (0.3f + 0.7f * std::sin((y - 120) / 380 * PI));
        Rectangle b = {x - 56, y, 112, 30};
        DrawRectangleRounded(b, 0.25f, 6, Fade2(Hex(0x2a0f3a), 0.5f * a));
        DrawRectangleRoundedLinesEx(b, 0.25f, 6, 1.5f, Fade2(Hex(0xff9ad8), a));
        TextCenter(TextFormat("0x7FF%03X", (i * 0x2a7 + 0x1f0) & 0xFFF), b, 13, Fade2(Hex(0xffd0f0), a), F_MONOBOLD);
        DrawLineEx({b.x + 6, b.y + 15}, {b.x + b.width - 6, b.y + 15}, 1.5f, Fade2(Hex(0xff5c7a), a * 0.7f));   // freed: crossed out
    }
    // a glitch now and then: a thin line of colour across the sky
    if (std::fmod(time, 2.3f) < 0.08f) {
        float y = 80 + std::fmod(time * 977, 400.0f);
        DrawRectangle((int)L, (int)y, (int)scr.width + 1, 3, Fade2(Hex(0xff3dd0), 0.5f));
        DrawRectangle((int)L, (int)y + 5, (int)scr.width + 1, 2, Fade2(Hex(0x3de5ff), 0.4f));
    }
    // sparks of memory rising
    for (int i = 0; i < 30; i++) {
        float sx = std::fmod(i * 151.0f - cam * 0.35f + 100000, scr.width + 40) + L - 20;
        float sy = kGround - std::fmod(i * 47.0f + time * 30, kGround);
        DrawRectangleRec({sx, sy, 3, 3}, Fade2(i % 3 ? Hex(0xff9ad8) : Hex(0xffe08a), 0.6f));
    }
    // the floor: dark crystal blocks, each with its address
    const Color soil = Hex(0x1c0f26), top = Hex(0x8a5ab0), edge = Hex(0x5a3a7a), b0 = Hex(0x2e1a40), b1 = Hex(0x271636);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, (int)(H - kGround) + 1, soil);
    DrawRectangle((int)L, (int)kGround, (int)scr.width + 1, 10, top);
    DrawRectangle((int)L, (int)kGround + 10, (int)scr.width + 1, 6, edge);
    for (float x = L - std::fmod(cam, 64.0f) - 64; x < R; x += 64) {
        float wx = x + cam;
        int seed = (int)std::floor(wx / 64);
        for (int row = 0; row < 3; row++) {
            float y = kGround + 28 + row * 38.0f, xo = (row % 2) * 32.0f;
            DrawRectangleRounded({x + xo + 3, y, 58, 32}, 0.2f, 4, ((seed + row) % 2) ? b0 : b1);
        }
        Text(TextFormat("%02X", (seed * 37) & 0xFF), x + 22, kGround + 36, 11, Fade2(Hex(0xb08ad0), 0.35f), F_MONOBOLD);
        if (((seed % 6) + 6) % 6 == 4) {   // a crystal growing out of the floor
            Tri({x + 20, kGround + 2}, {x + 30, kGround - 30}, {x + 38, kGround + 2}, Hex(0xd08aff));
            Tri({x + 30, kGround - 30}, {x + 38, kGround + 2}, {x + 33, kGround + 2}, Hex(0x8a4ab0));
            Glow({x + 30, kGround - 12}, 26, Hex(0xd08aff), 0.35f);
        }
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
