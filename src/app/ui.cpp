#include "ui.h"
#include "scrollbar.h"
#include "paste.h"
#include "theme.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace ui {

Color Mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                  (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}
Color Alpha(Color c, float a) {
    c.a = (unsigned char)std::clamp(c.a * a, 0.0f, 255.0f);
    return c;
}

// ================================================================ frame ====
static float gScale = 1.0f;
static bool gInput = true;
static bool gHand = false;
static bool gIBeam = false;
static bool gClickConsumed = false;

void SetScale(float s) { gScale = s; }
float Scale() { return gScale; }
void BeginFrame() { gHand = false; gIBeam = false; gClickConsumed = false; gInput = true; }
bool WantsHandCursor() { return gHand; }
void WantHand() { gHand = true; }
bool WantsIBeam() { return gIBeam; }
void WantIBeam() { gIBeam = true; }
void SetInputEnabled(bool on) { gInput = on; }
bool InputEnabled() { return gInput; }

Vector2 Mouse() {
    // main.cpp sets raylib's mouse offset/scale so GetMousePosition() is
    // already in virtual canvas coordinates.
    return GetMousePosition();
}
bool MouseIn(Rectangle r) { return CheckCollisionPointRec(Mouse(), r); }
bool Clicked(Rectangle r) {
    if (!gInput || gClickConsumed) return false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MouseIn(r)) { gClickConsumed = true; return true; }
    return false;
}
bool RightClicked(Rectangle r) {
    return gInput && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && MouseIn(r);
}

// ================================================================ fonts ====
static const char* kFontFiles[F_COUNT] = {
    "Inter-Regular.ttf", "Inter-Medium.ttf", "Inter-SemiBold.ttf", "Inter-Bold.ttf", "Inter-ExtraBold.ttf",
    "JetBrainsMono-Regular.ttf", "JetBrainsMono-Bold.ttf" };
static unsigned char* gFontData[F_COUNT] = {};
static int gFontSize[F_COUNT] = {};
static std::map<int, Font> gFontCache;

bool InitFonts(const std::string& dir) {
    bool ok = true;
    for (int i = 0; i < F_COUNT; i++) {
        std::string path = dir + "/fonts/" + kFontFiles[i];
        if (FileExists(path.c_str())) gFontData[i] = LoadFileData(path.c_str(), &gFontSize[i]);
        if (!gFontData[i]) ok = false;
    }
    return ok;
}

static int gWarmNext = 0;

void ClearFontCache() {
    for (auto& kv : gFontCache) UnloadFont(kv.second);
    gFontCache.clear();
    gWarmNext = 0;   // the new scale needs them again
}

bool WarmFonts() {
    // every kind at the sizes the screens use (11 to 26 canvas units), one per call
    const int lo = 11, hi = 26, perKind = hi - lo + 1, total = F_COUNT * perKind;
    if (gWarmNext >= total) return false;
    int kind = gWarmNext / perKind, size = lo + gWarmNext % perKind;
    gWarmNext++;
    GetFont((FontKind)kind, (float)size);
    return true;
}

void UnloadFonts() {
    ClearFontCache();
    for (int i = 0; i < F_COUNT; i++) if (gFontData[i]) { UnloadFileData(gFontData[i]); gFontData[i] = nullptr; }
}

// The characters every font is made with: ASCII, the Latin-1 letters
// (á é í ó ú ñ ü ¿ ¡ ...) and a few symbols answers and lessons use.
static const std::vector<int>& FontCodepoints() {
    static std::vector<int> cps;
    if (cps.empty()) {
        for (int c = 32; c <= 126; c++) cps.push_back(c);
        for (int c = 160; c <= 255; c++) cps.push_back(c);
        const int extra[] = {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2190, 0x2192,
                             0x2191, 0x2193, 0x2194, 0x2260, 0x2264, 0x2265, 0x2248, 0x221A, 0x221E, 0x2211,
                             0x00D7, 0x2713, 0x2717, 0x20AC, 0x0152, 0x0153};
        for (int c : extra) if (std::find(cps.begin(), cps.end(), c) == cps.end()) cps.push_back(c);
    }
    return cps;
}

Font GetFont(FontKind kind, float size) {
    int px = (int)std::lround(size * gScale);
    if (px < 6) px = 6;
    int key = kind * 10000 + px;
    auto it = gFontCache.find(key);
    if (it != gFontCache.end()) return it->second;
    Font f;
    if (gFontData[kind]) {
        const std::vector<int>& cps = FontCodepoints();
        f = LoadFontFromMemory(".ttf", gFontData[kind], gFontSize[kind], px, const_cast<int*>(cps.data()), (int)cps.size());
        SetTextureFilter(f.texture, TEXTURE_FILTER_POINT);
        // small sizes round the space advance down to almost nothing: enforce a minimum
        for (int g = 0; g < f.glyphCount; g++)
            if (f.glyphs[g].value == ' ' && f.glyphs[g].advanceX < (int)std::lround(px * 0.26f))
                f.glyphs[g].advanceX = (int)std::lround(px * 0.26f);
    } else {
        f = GetFontDefault();
    }
    gFontCache[key] = f;
    return f;
}

static float Snap(float v) { return std::round(v * gScale) / gScale; }

// Text is drawn at exactly the font's pixel size (baseSize / scale in canvas
// units), so every glyph lands on whole pixels: no blurry, resampled text.
void Text(const std::string& s, float x, float y, float size, Color c, FontKind k) {
    if (s.empty()) return;
    Font f = GetFont(k, size);
    DrawTextEx(f, s.c_str(), {Snap(x), Snap(y)}, f.baseSize / gScale, 0, c);
}

Vector2 Measure(const std::string& s, float size, FontKind k) {
    if (s.empty()) return {0, size};
    Font f = GetFont(k, size);
    return MeasureTextEx(f, s.c_str(), f.baseSize / gScale, 0);
}

void TextCenter(const std::string& s, Rectangle r, float size, Color c, FontKind k) {
    Vector2 m = Measure(s, size, k);
    Text(s, r.x + (r.width - m.x) / 2, r.y + (r.height - size) / 2 - 1, size, c, k);
}

void TextRight(const std::string& s, float rightX, float y, float size, Color c, FontKind k) {
    Text(s, rightX - Measure(s, size, k).x, y, size, c, k);
}

std::vector<std::string> Wrap(const std::string& s, float size, FontKind k, float maxWidth) {
    std::vector<std::string> out;
    for (const std::string& para : SplitLines(s)) {
        std::string line, word;
        size_t i = 0;
        auto flushWord = [&]() {
            if (word.empty()) return;
            std::string trial = line.empty() ? word : line + " " + word;
            if (!line.empty() && Measure(trial, size, k).x > maxWidth) { out.push_back(line); line = word; }
            else line = trial;
            word.clear();
        };
        for (; i < para.size(); i++) {
            if (para[i] == ' ') flushWord();
            else word += para[i];
        }
        flushWord();
        out.push_back(line);
    }
    return out;
}

std::string Ellipsize(const std::string& s, float size, FontKind k, float maxWidth) {
    if (Measure(s, size, k).x <= maxWidth) return s;
    std::string t = s;
    while (!t.empty() && Measure(t + "...", size, k).x > maxWidth) t.pop_back();
    return t + "...";
}

// ------------------------------------------------------------ rich text --
struct Piece { std::string s; int style; };   // 0 normal, 1 bold, 2 code
using Token = std::vector<Piece>;

static FontKind StyleFont(int style) { return style == 1 ? F_BOLD : style == 2 ? F_MONO : F_UI; }
static float StyleSize(int style, float size) { return style == 2 ? size * 0.9f : size; }

static float TokenWidth(const Token& t, float size) {
    float w = 0;
    for (auto& p : t) w += Measure(p.s, StyleSize(p.style, size), StyleFont(p.style)).x + (p.style == 2 ? 4 : 0);
    return w;
}

static std::vector<Token> Tokenize(const std::string& line) {
    std::vector<Token> tokens;
    Token cur;
    std::string piece;
    bool code = false, bold = false;
    auto flushPiece = [&]() {
        if (!piece.empty()) cur.push_back({piece, code ? 2 : bold ? 1 : 0});
        piece.clear();
    };
    auto flushToken = [&]() { flushPiece(); if (!cur.empty()) tokens.push_back(cur); cur.clear(); };
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '`') { flushPiece(); code = !code; continue; }
        if (!code && c == '*' && i + 1 < line.size() && line[i + 1] == '*') { flushPiece(); bold = !bold; i++; continue; }
        if (c == ' ' && !code) { flushToken(); continue; }
        piece += c;
    }
    flushToken();
    return tokens;
}

float RichText(Rectangle r, const std::string& text, float size, Color c, bool draw) {
    float y = r.y;
    float lineH = size * 1.5f;
    float spaceW = Measure(" ", size, F_UI).x;
    for (const std::string& raw : SplitLines(text)) {
        if (raw.empty()) { y += size * 0.55f; continue; }
        std::string line = raw;
        float indent = 0;
        if (line.rfind("- ", 0) == 0) {
            if (draw) DrawCircleV({r.x + 6, y + size * 0.62f}, 2.6f, col::accent2);
            indent = 18;
            line = line.substr(2);
        }
        float startX = r.x + indent;
        float x = startX;
        for (const Token& t : Tokenize(line)) {
            float w = TokenWidth(t, size);
            if (x > startX && x + w > r.x + r.width) { x = startX; y += lineH; }
            if (draw) {
                float px = x;
                for (const Piece& p : t) {
                    float ps = StyleSize(p.style, size);
                    float pw = Measure(p.s, ps, StyleFont(p.style)).x;
                    if (p.style == 2) {
                        RoundRect({px, y + 1, pw + 4, size * 1.25f}, 4, Alpha(col::accent, 0.16f));
                        Text(p.s, px + 2, y + (size - ps) * 0.5f + 1, ps, Mix(col::accent, col::heading, 0.45f), F_MONO);
                        px += pw + 4;
                    } else {
                        Text(p.s, px, y, ps, p.style == 1 ? col::heading : c, StyleFont(p.style));
                        px += pw;
                    }
                }
            }
            x += w + spaceW;
        }
        y += lineH;
    }
    return y - r.y;
}

// =============================================================== shapes ====
void RoundRect(Rectangle r, float radius, Color c) {
    float m = std::min(r.width, r.height);
    if (m <= 0) return;
    DrawRectangleRounded(r, std::min(1.0f, radius * 2 / m), 16, c);
}
void RoundRectLines(Rectangle r, float radius, float thick, Color c) {
    float m = std::min(r.width, r.height);
    if (m <= 0) return;
    // A whole number of screen pixels, or a 1-unit line comes out 1 or 2 pixels
    // wide depending on where it lands, which looks broken.
    thick = std::max(1.0f, std::round(thick * gScale)) / gScale;
    DrawRectangleRoundedLinesEx(r, std::min(1.0f, radius * 2 / m), 16, thick, c);
}
void Shadow(Rectangle r, float radius, float strength) {
    for (int i = 4; i >= 1; i--) {
        float g = i * 2.0f;
        RoundRect({r.x - g, r.y - g + 3, r.width + g * 2, r.height + g * 2}, radius + g, Alpha(col::shadow, 0.045f * strength));
    }
}

void Panel(Rectangle r, Color fill, float radius) {
    Shadow(r, radius);
    if (fill.a == 255) {
        // A lighter line along the top edge, as if lit from above.
        RoundRect(r, radius, Mix(fill, WHITE, 0.09f));
        RoundRect({r.x, r.y + 1, r.width, r.height - 1}, radius, fill);
    } else {
        RoundRect(r, radius, fill);
    }
    RoundRectLines(r, radius, 1, col::border);
}

// Rounded rectangle filled with colours that change across it. raylib has no
// rounded gradient, so the rounded ends are drawn one pixel row at a time and
// the straight middle in one piece. Every row edge is computed the same way,
// so neighbouring rows meet exactly: no gaps and no doubled lines.
static void GradientShape(Rectangle r, float radius, Color tl, Color tr, Color bl, Color br) {
    if (r.width <= 0 || r.height <= 0) return;
    radius = std::min(radius, std::min(r.width, r.height) / 2);
    auto at = [&](Color a, Color b, float t) { return Mix(a, b, std::clamp(t, 0.0f, 1.0f)); };
    auto band = [&](float y0, float y1) {
        float mid = (y0 + y1) / 2;
        float d = mid < radius ? radius - mid : mid > r.height - radius ? mid - (r.height - radius) : 0;
        float inset = radius - std::sqrt(std::max(0.0f, radius * radius - d * d));
        float t0 = y0 / r.height, t1 = y1 / r.height;
        DrawRectangleGradientEx({r.x + inset, r.y + y0, r.width - inset * 2, y1 - y0},
                                at(tl, bl, t0), at(tl, bl, t1), at(tr, br, t1), at(tr, br, t0));
    };
    const int rows = std::max(1, (int)std::ceil(radius * gScale));
    auto edge = [&](int k) { return radius * k / rows; };
    for (int k = 0; k < rows; k++) band(edge(k), edge(k + 1));                                   // top
    if (r.height > radius * 2) band(radius, r.height - radius);                                   // middle
    for (int k = rows; k > 0; k--) band(r.height - edge(k), r.height - edge(k - 1));            // bottom
}

void GradientRoundRect(Rectangle r, float radius, Color left, Color right) {
    GradientShape(r, radius, left, right, left, right);
}

void GradientRoundRectV(Rectangle r, float radius, Color top, Color bottom) {
    GradientShape(r, radius, top, top, bottom, bottom);
}

void PanelTint(Rectangle r, float radius, Color c, float strength) {
    GradientRoundRectV({r.x, r.y, r.width, r.height * 0.65f}, radius, Alpha(c, strength), Alpha(c, 0));
}

void DrawLogo(Rectangle r, float time, float hot) {
    const float s = r.width, rad = s * 0.28f;
    const Vector2 c = {r.x + s / 2, r.y + s / 2};
    if (hot > 0.01f) SoftGlow(c, s * 1.15f, col::accent, 0.28f * hot);
    Shadow(r, rad, 0.9f);
    Color mid = Mix(col::accent, col::accent2, 0.4f);
    GradientRoundRect(r, rad, col::accent, Mix(col::accent, col::accent2, 0.75f));
    GradientRoundRectV({r.x, r.y, s, s * 0.55f}, rad, Alpha(WHITE, 0.24f), Alpha(WHITE, 0));   // sheen
    Color ink = OnColor(mid);

    // the braces of a C block
    const float fs = s * 0.66f;
    float bw = Measure("{", fs, F_MONOBOLD).x;
    float ty = r.y + (s - fs) / 2 - s * 0.05f;
    Text("{", r.x + s * 0.25f - bw / 2, ty, fs, Alpha(ink, 0.95f), F_MONOBOLD);
    Text("}", r.x + s * 0.75f - bw / 2, ty, fs, Alpha(ink, 0.95f), F_MONOBOLD);

    // the pointer star; orange like every pointer in the app, unless that
    // would not stand out on this theme's colours
    Color star = ContrastRatio(col::pointer, mid) >= 1.8f ? Mix(col::pointer, WHITE, 0.15f) : ink;
    const float len = s * 0.15f, th = std::max(1.5f, s * 0.075f);
    const float spin = time * (0.8f + 2.4f * hot);
    const Vector2 sc = {c.x, c.y - s * 0.06f};
    for (int k = 0; k < 3; k++) {
        float a = spin + k * 1.0472f;   // 60 degrees apart
        Vector2 d = {std::cos(a) * len, std::sin(a) * len};
        DrawLineEx({sc.x - d.x, sc.y - d.y}, {sc.x + d.x, sc.y + d.y}, th, star);
    }
    DrawCircleV(sc, th * 0.6f, star);
}

void Sparkle(Vector2 c, float radius, Color col) {
    // |x|^p + |y|^p = r^p with p < 1: four points joined by curves bending inwards
    const int n = 64;
    const float p = 0.55f;
    Vector2 pts[n];
    for (int i = 0; i < n; i++) {
        float a = i * 2 * PI / n;
        float ca = std::cos(a), sa = std::sin(a);
        float rr = radius / std::pow(std::pow(std::fabs(ca), p) + std::pow(std::fabs(sa), p), 1.0f / p);
        pts[i] = {c.x + ca * rr, c.y + sa * rr};
    }
    for (int i = 0; i < n; i++) {   // both windings: one of them faces the camera
        DrawTriangle(c, pts[i], pts[(i + 1) % n], col);
        DrawTriangle(c, pts[(i + 1) % n], pts[i], col);
    }
}

// A curly brace centred on c: h tall, d wide, line th thick. closing: "}" instead of "{".
static void DrawBrace(Vector2 c, float h, float d, float th, bool closing, Color col) {
    const float k = closing ? -1.0f : 1.0f;              // mirror for "}"
    const float top = c.y - h / 2, bot = c.y + h / 2, x = c.x, r = d * 0.5f;
    auto P = [&](float dx, float y) { return Vector2{x + k * dx, y}; };
    // top hook, stem, the point in the middle - and the same, mirrored, below
    DrawSplineSegmentBezierCubic(P(r, top), P(r * 0.15f, top), P(0, top + r * 0.3f), P(0, top + r), th, col);
    DrawLineEx(P(0, top + r), P(0, c.y - r), th, col);
    DrawSplineSegmentBezierCubic(P(0, c.y - r), P(0, c.y - r * 0.3f), P(-r * 0.2f, c.y), P(-r, c.y), th, col);
    DrawSplineSegmentBezierCubic(P(-r, c.y), P(-r * 0.2f, c.y), P(0, c.y + r * 0.3f), P(0, c.y + r), th, col);
    DrawLineEx(P(0, c.y + r), P(0, bot - r), th, col);
    DrawSplineSegmentBezierCubic(P(0, bot - r), P(0, bot - r * 0.3f), P(r * 0.15f, bot), P(r, bot), th, col);
    for (Vector2 j : {P(r, top), P(0, top + r), P(0, c.y - r), P(-r, c.y), P(0, c.y + r), P(0, bot - r), P(r, bot)})
        DrawCircleV(j, th / 2, col);   // round ends and joints
}

void DrawAiLogo(Rectangle r, float time, float hot) {
    const float s = r.width, rad = s * 0.28f;
    const Vector2 c = {r.x + s / 2, r.y + s / 2};
    if (hot > 0.01f) SoftGlow(c, s * 1.15f, col::accent2, 0.30f * hot);
    Shadow(r, rad, 0.9f);
    // a night tile, lit from the top by the theme's colours
    const Color night = {16, 14, 30, 255};   // the same dark tile on light and dark themes
    Color top = Mix(night, col::accent, 0.32f), bottom = Mix(night, col::accent2, 0.14f);
    GradientRoundRectV(r, rad, top, bottom);
    GradientRoundRectV({r.x, r.y, s, s * 0.5f}, rad, Alpha(WHITE, 0.14f), Alpha(WHITE, 0));   // sheen
    RoundRectLines(r, rad, std::max(1.0f, s * 0.025f), Alpha(Mix(col::accent, col::accent2, 0.5f), 0.55f + 0.35f * hot));

    // the braces of a C block, drawn as curves (not font glyphs) so they sit
    // exactly symmetric around the centre, in the theme's two colours
    const float bh = s * 0.52f, bd = s * 0.15f, th = std::max(1.6f, s * 0.07f);
    DrawBrace({r.x + s * 0.25f, c.y}, bh, bd, th, false, Mix(col::accent, WHITE, 0.15f));
    DrawBrace({r.x + s * 0.75f, c.y}, bh, bd, th, true, Mix(col::accent2, WHITE, 0.15f));

    // the sparkle where SegFault has its pointer star; it breathes
    const float breathe = 1 + (0.05f + 0.05f * hot) * std::sin(time * (2.2f + 2.0f * hot));
    SoftGlow(c, s * 0.28f, Mix(col::accent, col::accent2, 0.5f), 0.45f + 0.25f * hot);
    Sparkle(c, s * 0.19f * breathe, WHITE);
    // and a small one beside it, twinkling out of step
    float tw = 0.7f + 0.3f * std::sin(time * 3.1f + 1.7f);
    Sparkle({c.x + s * 0.13f, c.y - s * 0.14f}, s * 0.05f * tw, Mix(col::accent2, WHITE, 0.4f));
}

void SoftGlow(Vector2 center, float radius, Color c, float strength) {
    // Stacked faint circles fade smoothly; a single low-alpha gradient shows facets.
    const int layers = 24;
    for (int i = 0; i < layers; i++)
        DrawCircleV(center, radius * (1.0f - (float)i / layers), Alpha(c, strength / layers));
}

void Backdrop(float time) {
    bool light = Themes()[CurrentTheme()].light;
    DrawRectangleGradientV(0, 0, (int)W, (int)H, col::bg, Mix(col::bg, col::bg2, 0.8f));
    float a = light ? 0.13f : 0.11f;
    float dx = std::sin(time * 0.13f) * 30, dy = std::cos(time * 0.11f) * 20;
    SoftGlow({1080 + dx, 90 + dy}, 560, col::accent, a);
    SoftGlow({140 - dx, 760 - dy}, 520, col::accent2, a * 0.7f);
}

static void ArrowHead(Vector2 tip, Vector2 dir, Color c, float head) {
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 0.001f) return;
    dir = {dir.x / len, dir.y / len};
    Vector2 n = {-dir.y, dir.x};
    Vector2 base = {tip.x - dir.x * head, tip.y - dir.y * head};
    Vector2 a = {base.x + n.x * head * 0.5f, base.y + n.y * head * 0.5f};
    Vector2 b = {base.x - n.x * head * 0.5f, base.y - n.y * head * 0.5f};
    DrawTriangle(tip, b, a, c);
    DrawTriangle(tip, a, b, c);
}

void Arrow(Vector2 from, Vector2 to, Color c, float thick, float head) {
    Vector2 d = {to.x - from.x, to.y - from.y};
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1) return;
    Vector2 end = {to.x - d.x / len * head * 0.8f, to.y - d.y / len * head * 0.8f};
    DrawLineEx(from, end, thick, c);
    DrawCircleV(from, thick * 1.6f, c);
    ArrowHead(to, d, c, head);
}

void CurvedArrow(Vector2 a, Vector2 ctrl, Vector2 b, Color c, float thick, float head) {
    const int N = 24;
    Vector2 prev = a;
    for (int i = 1; i <= N; i++) {
        float t = (float)i / N * 0.94f;
        float u = 1 - t;
        Vector2 p = {u * u * a.x + 2 * u * t * ctrl.x + t * t * b.x, u * u * a.y + 2 * u * t * ctrl.y + t * t * b.y};
        DrawLineEx(prev, p, thick, c);
        prev = p;
    }
    DrawCircleV(a, thick * 1.6f, c);
    Vector2 dir = {b.x - prev.x, b.y - prev.y};
    DrawLineEx(prev, b, thick, c);
    ArrowHead(b, dir, c, head);
}

void DotGrid(Rectangle r, float gap, Color c) {
    for (float y = r.y; y < r.y + r.height; y += gap)
        for (float x = r.x; x < r.x + r.width; x += gap)
            DrawRectangleV({x, y}, {1.5f, 1.5f}, c);
}

float Badge(float x, float y, const std::string& s, Color c, float size) {
    float w = Measure(s, size, F_BOLD).x + 16;
    RoundRect({x, y, w, size + 10}, (size + 10) / 2, Alpha(c, 0.16f));
    RoundRectLines({x, y, w, size + 10}, (size + 10) / 2, 1, Alpha(c, 0.32f));
    Text(s, x + 8, y + 5, size, c, F_BOLD);
    return w;
}

// ============================================================== widgets ====
bool Button(Rectangle r, const std::string& label, ButtonKind kind, bool enabled, float size) {
    bool hover = enabled && gInput && MouseIn(r);
    bool down = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (hover) gHand = true;
    Color fill, fg = col::text, line = {0, 0, 0, 0};
    switch (kind) {
        case BTN_PRIMARY: fill = col::accent; fg = OnColor(col::accent); break;
        case BTN_SUCCESS: fill = Mix(col::success, col::bg, 0.2f); fg = OnColor(fill); break;
        case BTN_DANGER:  fill = Alpha(col::danger, 0.18f); fg = col::danger; line = Alpha(col::danger, 0.5f); break;
        case BTN_GHOST:   fill = {0, 0, 0, 0}; line = col::border; break;
        default:          fill = col::panelHi; line = Alpha(col::border, 0.8f); break;
    }
    if (hover) fill = (kind == BTN_GHOST) ? Alpha(col::panelHi, 0.8f) : Mix(fill, WHITE, 0.12f);
    if (down) fill = Mix(fill, BLACK, 0.15f);
    if (!enabled) { fill = Alpha(fill, 0.4f); fg = Alpha(fg, 0.4f); line = Alpha(line, 0.4f); }
    Rectangle rr = r;
    if (down) { rr.y += 1; }
    if (enabled && (kind == BTN_PRIMARY || kind == BTN_SUCCESS)) {
        if (hover) Shadow(rr, 10, 1.6f);
        GradientRoundRect(rr, 10, fill, Mix(fill, kind == BTN_PRIMARY ? col::accent2 : WHITE, 0.22f));
    } else {
        RoundRect(rr, 10, fill);
    }
    if (line.a) RoundRectLines(rr, 10, 1, line);
    TextCenter(label, rr, size, fg, F_BOLD);
    return enabled && Clicked(r);
}

bool IconButton(Rectangle r, const std::string& glyph, bool enabled) {
    return Button(r, glyph, BTN_GHOST, enabled, 16);
}

bool ListItem(Rectangle r, const std::string& title, const std::string& sub, bool selected, Color accent, bool done) {
    bool hover = gInput && MouseIn(r);
    if (hover) gHand = true;
    Color fill = selected ? Alpha(accent, 0.16f) : hover ? col::panelHi : Color{0, 0, 0, 0};
    RoundRect(r, 8, fill);
    if (selected) RoundRect({r.x, r.y + 8, 3, r.height - 16}, 1.5f, accent);
    float tx = r.x + 14;
    float maxW = r.width - 28 - (done ? 20 : 0);
    if (sub.empty()) {
        Text(Ellipsize(title, 15, F_BOLD, maxW), tx, r.y + (r.height - 15) / 2 - 1, 15, selected ? col::heading : col::text, F_BOLD);
    } else {
        Text(Ellipsize(title, 15, F_BOLD, maxW), tx, r.y + 8, 15, selected ? col::heading : col::text, F_BOLD);
        Text(Ellipsize(sub, 12.5f, F_UI, maxW), tx, r.y + 28, 12.5f, col::dim, F_UI);
    }
    if (done) {
        Vector2 c = {r.x + r.width - 18, r.y + r.height / 2};
        DrawCircleV(c, 8, Alpha(col::success, 0.2f));
        DrawLineEx({c.x - 4, c.y}, {c.x - 1, c.y + 3}, 2, col::success);
        DrawLineEx({c.x - 1, c.y + 3}, {c.x + 4, c.y - 3}, 2, col::success);
    }
    return Clicked(r);
}

bool Tab(Rectangle r, const std::string& label, bool active) {
    bool hover = gInput && MouseIn(r);
    if (hover) gHand = true;
    if (active) {
        RoundRect(r, 9, Alpha(col::accent, 0.14f));
        RoundRectLines(r, 9, 1, Alpha(col::accent, 0.38f));
    } else if (hover) {
        RoundRect(r, 9, Alpha(col::panelHi, 0.7f));
    }
    TextCenter(label, r, 15, active ? col::heading : hover ? col::text : col::dim, F_BOLD);
    return Clicked(r);
}

static int gActiveBox = -1;
static int gActiveBoxFrame = 0;
void ClearTextFocus() { gActiveBox = -1; }
bool AnyTextFocused() { return gActiveBox != -1; }
void FocusWidget(int id) { gActiveBox = id; }
bool IsFocused(int id) { return gActiveBox == id; }

bool TextBox(Rectangle r, std::string& text, int id, int maxLen, bool numericOnly, const std::string& placeholder, bool masked) {
    bool hover = gInput && MouseIn(r);
    if (hover) gIBeam = true;
    if (gInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hover) { gActiveBox = id; gActiveBoxFrame = 0; }
        else if (gActiveBox == id) gActiveBox = -1;
    }
    bool active = gActiveBox == id;
    bool submitted = false;
    if (active && gInput) {
        gActiveBoxFrame++;
        int ch = GetCharPressed();
        while (ch > 0) {
            bool ok = (ch >= 32 && ch < 127) || ch >= 160;   // ASCII and accented letters (ñ, á, ¿ ...)
            if (numericOnly) ok = (ch >= '0' && ch <= '9') || ch == '-' || ch == ' ' || ch == ',';
            if (ok && (int)text.size() < maxLen) {
                if (ch < 128) text += (char)ch;
                else { int n = 0; const char* u = CodepointToUTF8(ch, &n); text.append(u, n); }
            }
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
            while (text.size() > 1 && ((unsigned char)text.back() & 0xC0) == 0x80) text.pop_back();   // a whole UTF-8 letter
            text.pop_back();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) submitted = true;

        // Clipboard. A hidden password can be pasted (from a password manager)
        // but never copied or cut out of the box.
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                    IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (ctrl && IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            if (clip) text = PasteInto(text, clip, maxLen, numericOnly, !masked);
        }
        if (ctrl && !masked && !text.empty() && (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_X))) {
            SetClipboardText(text.c_str());
            if (IsKeyPressed(KEY_X)) text.clear();
            else Toast("Copied", col::accent2);
        }
    }
    if (active) RoundRectLines({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 11, 3, Alpha(col::accent, 0.22f));
    RoundRect(r, 8, col::codeBg);
    RoundRectLines(r, 8, active ? 1.5f : 1.0f, active ? col::accent : hover ? col::faint : col::border);
    float size = 16;
    float ty = r.y + (r.height - size) / 2;
    BeginClip({r.x + 4, r.y, r.width - 8, r.height});
    const std::string shown = masked ? std::string(text.size(), '*') : text;
    float tw = Measure(shown, size, F_MONO).x;
    float tx = r.x + 10;
    if (tw > r.width - 24) tx -= tw - (r.width - 24);
    if (text.empty() && !active) Text(placeholder, r.x + 10, ty, size, col::faint, F_MONO);
    Text(shown, tx, ty, size, col::text, F_MONO);
    if (active && fmodf((float)GetTime(), 1.0f) < 0.55f)
        DrawRectangleRec({tx + tw + 1, ty - 1, 2, size + 3}, col::accent);
    EndClip();
    return submitted;
}

void ProgressBar(Rectangle r, float t, Color fill, Color back) {
    t = std::clamp(t, 0.0f, 1.0f);
    RoundRect(r, r.height / 2, back);
    if (t > 0) {
        Rectangle bar = {r.x, r.y, std::max(r.height, r.width * std::min(t, 1.0f)), r.height};
        if (fill.a == 255) GradientRoundRect(bar, r.height / 2, fill, Mix(fill, col::heading, 0.22f));
        else RoundRect(bar, r.height / 2, fill);
    }
}

// ============================================================ scrolling ====
void Scroll::Update(Rectangle view, float contentHeight) {
    float maxScroll = std::max(0.0f, contentHeight - view.height);
    if (ScrollBarInput(view, contentHeight, y, target, dragging, grab)) return;
    if (gInput && MouseIn(view)) target -= GetMouseWheelMove() * 60.0f;
    target = std::clamp(target, 0.0f, maxScroll);
    y += (target - y) * std::min(1.0f, GetFrameTime() * 14.0f);
    if (std::fabs(target - y) < 0.5f) y = target;
}
void Scroll::ScrollTo(float value, Rectangle view, float contentHeight) {
    float maxScroll = std::max(0.0f, contentHeight - view.height);
    target = std::clamp(value, 0.0f, maxScroll);
}

static std::vector<Rectangle> gClipStack;

// A little wider than the thumb, so it is easy to hit.
static Rectangle ScrollBarHitArea(Rectangle view) {
    const float w = 14;
    return {view.x + view.width - w, view.y, w, view.height};
}

bool OverScrollBar(Rectangle view, float contentHeight) {
    if (!gInput || contentHeight <= view.height + 1 || !MouseIn(ScrollBarHitArea(view))) return false;
    // A code block inside a lesson can be scrolled out of its panel.
    return gClipStack.empty() || MouseIn(gClipStack.back());
}

bool ScrollBarInput(Rectangle view, float contentHeight, float& y, float& target, bool& dragging, float& grab) {
    if (contentHeight <= view.height + 1 || !gInput) { dragging = false; return false; }
    bool over = OverScrollBar(view, contentHeight);
    if (over) gHand = true;
    if (over && !gClickConsumed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        gClickConsumed = true;
        ScrollThumb th = ThumbFor(view.y, view.height, contentHeight, y);
        float my = Mouse().y;
        // On the thumb: keep it under the mouse. On the track: jump there.
        grab = (my >= th.top && my <= th.top + th.height) ? my - th.top : th.height / 2;
        dragging = true;
    }
    if (!dragging) return false;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { dragging = false; return false; }
    gHand = true;
    target = y = ScrollForThumb(view.y, view.height, contentHeight, Mouse().y - grab);
    return true;
}

void DrawScrollBar(Rectangle view, float contentHeight, float y, bool dragging) {
    if (contentHeight <= view.height + 1) return;
    bool hot = dragging || OverScrollBar(view, contentHeight);
    float w = hot ? 8.0f : 4.0f;
    ScrollThumb th = ThumbFor(view.y, view.height, contentHeight, y);
    RoundRect({view.x + view.width - 2 - w, th.top, w, th.height}, w / 2,
              dragging ? col::accent : Alpha(col::faint, hot ? 1.0f : 0.7f));
}

static void ApplyClip(Rectangle r) {
    BeginScissorMode((int)std::floor(r.x * gScale), (int)std::floor(r.y * gScale),
                     (int)std::ceil(r.width * gScale), (int)std::ceil(r.height * gScale));
}
void BeginClip(Rectangle r) {
    if (!gClipStack.empty()) {   // intersect with the parent clip
        Rectangle p = gClipStack.back();
        float x0 = std::max(r.x, p.x), y0 = std::max(r.y, p.y);
        float x1 = std::min(r.x + r.width, p.x + p.width), y1 = std::min(r.y + r.height, p.y + p.height);
        r = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
        EndScissorMode();
    }
    gClipStack.push_back(r);
    ApplyClip(r);
}
void EndClip() {
    EndScissorMode();
    if (!gClipStack.empty()) gClipStack.pop_back();
    if (!gClipStack.empty()) ApplyClip(gClipStack.back());
}

void ScrollBar(Rectangle view, float contentHeight, const Scroll& s) {
    DrawScrollBar(view, contentHeight, s.y, s.dragging);
}

// ================================================================= code ====
float CodeLineHeight(float size) { return std::round(size * 1.6f); }
float CodeCharWidth(float size) { return Measure("M", size, F_MONO).x; }

static const std::set<std::string>& TypeWords() {
    static const std::set<std::string> s = {"int", "char", "bool", "void", "float", "double", "long",
        "unsigned", "short", "const", "auto", "Node", "DoublyLinkedNode", "LinkedList", "DoublyLinkedList",
        "size_t", "static"};
    return s;
}
static const std::set<std::string>& KeyWords() {
    static const std::set<std::string> s = {"return", "if", "else", "for", "while", "do", "break",
        "continue", "new", "delete", "struct", "class", "public", "private", "sizeof", "using", "namespace",
        "true", "false", "nullptr", "NULL", "this", "include", "std"};
    return s;
}

static bool IsIdent(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

void CodeLine(float x, float y, const std::string& rawLine, float size, float alpha) {
    // expand tabs
    std::string line;
    for (char c : rawLine) { if (c == '\t') line += "    "; else line += c; }
    float cw = CodeCharWidth(size);
    const Color cText = Alpha(col::synText, alpha), cType = Alpha(col::synType, alpha),
                cKey = Alpha(col::synKey, alpha), cNum = Alpha(col::synNum, alpha),
                cStr = Alpha(col::synStr, alpha), cCom = Alpha(col::synCom, alpha),
                cFn = Alpha(col::synFn, alpha), cPtr = Alpha(col::pointer, alpha),
                cPre = Alpha(col::synPre, alpha), cPunct = Alpha(col::synPunct, alpha);
    size_t i = 0;
    auto put = [&](size_t start, size_t len, Color c) {
        Text(line.substr(start, len), x + start * cw, y, size, c, F_MONO);
    };
    // preprocessor line
    size_t firstNon = line.find_first_not_of(' ');
    if (firstNon != std::string::npos && line[firstNon] == '#') { put(0, line.size(), cPre); return; }
    while (i < line.size()) {
        char c = line[i];
        if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') { put(i, line.size() - i, cCom); break; }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < line.size() && line[j] != c) { if (line[j] == '\\') j++; j++; }
            j = std::min(j + 1, line.size());
            put(i, j - i, cStr); i = j; continue;
        }
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < line.size() && (std::isalnum((unsigned char)line[j]) || line[j] == '.')) j++;
            put(i, j - i, cNum); i = j; continue;
        }
        if (IsIdent(c)) {
            size_t j = i;
            while (j < line.size() && IsIdent(line[j])) j++;
            std::string w = line.substr(i, j - i);
            size_t k = j;
            while (k < line.size() && line[k] == ' ') k++;
            Color cc = cText;
            if (TypeWords().count(w)) cc = cType;
            else if (KeyWords().count(w)) cc = cKey;
            else if (k < line.size() && line[k] == '(') cc = cFn;
            put(i, j - i, cc); i = j; continue;
        }
        if (c == '-' && i + 1 < line.size() && line[i + 1] == '>') { put(i, 2, cPtr); i += 2; continue; }
        if (c == '*' || c == '&') {
            // '&&' is logical AND, not a pointer operator
            if (c == '&' && i + 1 < line.size() && line[i + 1] == '&') { put(i, 2, cPunct); i += 2; continue; }
            put(i, 1, cPtr); i++; continue;
        }
        if (c != ' ') put(i, 1, cPunct);
        i++;
    }
}

std::vector<std::string> SplitLines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\r') continue;
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

void CodeView(Rectangle r, const std::vector<std::string>& lines, Scroll& scroll, int highlight,
              float size, const std::vector<Color>* lineTint, bool autoScroll) {
    RoundRect(r, 10, col::codeBg);
    RoundRectLines(r, 10, 1, Alpha(col::border, 0.6f));
    float lh = CodeLineHeight(size);
    float pad = 10;
    float gutter = CodeCharWidth(size * 0.85f) * 3 + 18;
    Rectangle view = {r.x, r.y + pad, r.width, r.height - pad * 2};
    float contentH = lines.size() * lh;
    // Only when the line changes: afterwards the student can scroll away from it.
    if (autoScroll && highlight >= 0 && highlight != scroll.lastAuto) {
        scroll.lastAuto = highlight;
        float hy = highlight * lh;
        if (hy < scroll.target + lh || hy > scroll.target + view.height - lh * 2)
            scroll.ScrollTo(hy - view.height / 2 + lh, view, contentH);
    }
    scroll.Update(view, contentH);
    BeginClip(view);
    for (size_t i = 0; i < lines.size(); i++) {
        float y = view.y + i * lh - scroll.y;
        if (y + lh < view.y || y > view.y + view.height) continue;
        if (lineTint && i < lineTint->size() && (*lineTint)[i].a > 0)
            DrawRectangleRec({r.x + 2, y, r.width - 4, lh}, (*lineTint)[i]);
        if ((int)i == highlight) {
            DrawRectangleRec({r.x + 2, y, r.width - 4, lh}, col::codeLine);
            DrawRectangleRec({r.x + 2, y, 3, lh}, col::accent);
        }
        std::string num = std::to_string(i + 1);
        TextRight(num, r.x + gutter - 10, y + (lh - size * 0.85f) / 2, size * 0.85f,
                  (int)i == highlight ? col::text : col::faint, F_MONO);
        CodeLine(r.x + gutter, y + (lh - size) / 2, lines[i], size);
    }
    EndClip();
    ScrollBar(view, contentH, scroll);

    // A Copy button while the mouse is over the code. Only where it can be
    // seen: a lesson can scroll a code block partly out of its panel.
    Rectangle copy = {r.x + r.width - 92, r.y + r.height - 40, 74, 30};
    bool visible = gClipStack.empty() ||
                   (copy.y >= gClipStack.back().y && copy.y + copy.height <= gClipStack.back().y + gClipStack.back().height);
    if (gInput && MouseIn(r) && visible && !lines.empty() && Button(copy, "Copy", BTN_DEFAULT, true, 13)) {
        std::string all;
        for (size_t i = 0; i < lines.size(); i++) all += lines[i] + (i + 1 < lines.size() ? "\n" : "");
        SetClipboardText(all.c_str());
        Toast("Code copied", col::accent2);
    }
}

// =============================================================== toasts ====
struct ToastMsg { std::string msg; Color c; float t; };
static std::vector<ToastMsg> gToasts;

void Toast(const std::string& msg, Color c) {
    gToasts.push_back({msg, c, 0});
    if (gToasts.size() > 4) gToasts.erase(gToasts.begin());
}

void DrawToasts(float dt) {
    float y = 78;
    for (auto& t : gToasts) {
        t.t += dt;
        float a = t.t < 0.2f ? t.t / 0.2f : t.t > 2.2f ? std::max(0.0f, 1 - (t.t - 2.2f) / 0.4f) : 1;
        float w = Measure(t.msg, 15, F_BOLD).x + 40;
        Rectangle r = {W / 2 - w / 2, y - (1 - a) * 10, w, 36};
        Shadow(r, 18, a * 1.5f);
        RoundRect(r, 18, Alpha(Mix(col::panel, t.c, 0.16f), a));
        RoundRectLines(r, 18, 1.5f, Alpha(t.c, a));
        TextCenter(t.msg, r, 15, Alpha(col::heading, a), F_BOLD);
        y += 44;
    }
    gToasts.erase(std::remove_if(gToasts.begin(), gToasts.end(), [](const ToastMsg& t) { return t.t > 2.6f; }),
                  gToasts.end());
}

} // namespace ui
