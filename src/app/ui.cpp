#include "ui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace ui {

// ============================================================== colours ====
namespace col {
    const Color bg       = {11, 13, 19, 255};
    const Color bg2      = {16, 19, 27, 255};
    const Color panel    = {22, 26, 37, 255};
    const Color panelHi  = {31, 37, 52, 255};
    const Color border   = {45, 53, 74, 255};
    const Color text     = {236, 239, 246, 255};
    const Color dim      = {163, 171, 192, 255};
    const Color faint    = {106, 115, 140, 255};
    const Color accent   = {139, 108, 255, 255};   // violet
    const Color accent2  = {45, 212, 191, 255};    // teal
    const Color warn     = {251, 191, 36, 255};
    const Color danger   = {248, 113, 113, 255};
    const Color success  = {74, 222, 128, 255};
    const Color pointer  = {251, 146, 60, 255};    // pointers are always orange
    const Color heap     = {192, 132, 252, 255};
    const Color stack    = {96, 165, 250, 255};
    const Color codeBg   = {14, 17, 25, 255};
    const Color codeLine = {139, 108, 255, 46};
}

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

void ClearFontCache() {
    for (auto& kv : gFontCache) UnloadFont(kv.second);
    gFontCache.clear();
}

void UnloadFonts() {
    ClearFontCache();
    for (int i = 0; i < F_COUNT; i++) if (gFontData[i]) { UnloadFileData(gFontData[i]); gFontData[i] = nullptr; }
}

Font GetFont(FontKind kind, float size) {
    int px = (int)std::lround(size * gScale);
    if (px < 6) px = 6;
    int key = kind * 10000 + px;
    auto it = gFontCache.find(key);
    if (it != gFontCache.end()) return it->second;
    Font f;
    if (gFontData[kind]) {
        f = LoadFontFromMemory(".ttf", gFontData[kind], gFontSize[kind], px, nullptr, 0);
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
                        Text(p.s, px + 2, y + (size - ps) * 0.5f + 1, ps, Mix(col::accent, WHITE, 0.45f), F_MONO);
                        px += pw + 4;
                    } else {
                        Text(p.s, px, y, ps, p.style == 1 ? WHITE : c, StyleFont(p.style));
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
    DrawRectangleRounded(r, std::min(1.0f, radius * 2 / m), 8, c);
}
void RoundRectLines(Rectangle r, float radius, float thick, Color c) {
    float m = std::min(r.width, r.height);
    if (m <= 0) return;
    DrawRectangleRoundedLinesEx(r, std::min(1.0f, radius * 2 / m), 8, thick, c);
}
void Panel(Rectangle r, Color fill, float radius) {
    RoundRect(r, radius, fill);
    RoundRectLines(r, radius, 1, Alpha(col::border, 0.7f));
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
    RoundRect({x, y, w, size + 10}, (size + 10) / 2, Alpha(c, 0.18f));
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
        case BTN_PRIMARY: fill = col::accent; fg = WHITE; break;
        case BTN_SUCCESS: fill = Mix(col::success, col::bg, 0.25f); fg = {10, 30, 18, 255}; break;
        case BTN_DANGER:  fill = Alpha(col::danger, 0.18f); fg = col::danger; line = Alpha(col::danger, 0.5f); break;
        case BTN_GHOST:   fill = {0, 0, 0, 0}; line = col::border; break;
        default:          fill = col::panelHi; line = Alpha(col::border, 0.8f); break;
    }
    if (hover) fill = (kind == BTN_GHOST) ? Alpha(col::panelHi, 0.8f) : Mix(fill, WHITE, 0.12f);
    if (down) fill = Mix(fill, BLACK, 0.15f);
    if (!enabled) { fill = Alpha(fill, 0.4f); fg = Alpha(fg, 0.4f); line = Alpha(line, 0.4f); }
    Rectangle rr = r;
    if (down) { rr.y += 1; }
    RoundRect(rr, 9, fill);
    if (line.a) RoundRectLines(rr, 9, 1, line);
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
    if (selected) DrawRectangleRec({r.x, r.y + 8, 3, r.height - 16}, accent);
    float tx = r.x + 14;
    float maxW = r.width - 28 - (done ? 20 : 0);
    if (sub.empty()) {
        Text(Ellipsize(title, 15, F_BOLD, maxW), tx, r.y + (r.height - 15) / 2 - 1, 15, selected ? WHITE : col::text, F_BOLD);
    } else {
        Text(Ellipsize(title, 15, F_BOLD, maxW), tx, r.y + 8, 15, selected ? WHITE : col::text, F_BOLD);
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
    if (active) RoundRect(r, 8, col::panelHi);
    TextCenter(label, r, 15, active ? WHITE : hover ? col::text : col::dim, F_BOLD);
    if (active) DrawRectangleRec({r.x + 12, r.y + r.height - 3, r.width - 24, 3}, col::accent);
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
            bool ok = ch >= 32 && ch < 127;
            if (numericOnly) ok = (ch >= '0' && ch <= '9') || ch == '-' || ch == ' ' || ch == ',';
            if (ok && (int)text.size() < maxLen) text += (char)ch;
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) text.pop_back();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) submitted = true;
    }
    RoundRect(r, 8, col::codeBg);
    RoundRectLines(r, 8, active ? 2.0f : 1.0f, active ? col::accent : hover ? col::faint : col::border);
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
    if (t > 0) RoundRect({r.x, r.y, std::max(r.height, r.width * t), r.height}, r.height / 2, fill);
}

// ============================================================ scrolling ====
void Scroll::Update(Rectangle view, float contentHeight) {
    float maxScroll = std::max(0.0f, contentHeight - view.height);
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
    if (contentHeight <= view.height + 1) return;
    float trackH = view.height - 8;
    float h = std::max(30.0f, trackH * view.height / contentHeight);
    float t = s.y / (contentHeight - view.height);
    RoundRect({view.x + view.width - 6, view.y + 4 + (trackH - h) * t, 4, h}, 2, Alpha(col::faint, 0.7f));
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
    const Color cText = Alpha({214, 220, 236, 255}, alpha), cType = Alpha({103, 232, 249, 255}, alpha),
                cKey = Alpha({196, 167, 255, 255}, alpha), cNum = Alpha({253, 186, 116, 255}, alpha),
                cStr = Alpha({134, 239, 172, 255}, alpha), cCom = Alpha({100, 110, 136, 255}, alpha),
                cFn = Alpha({250, 224, 120, 255}, alpha), cPtr = Alpha(col::pointer, alpha),
                cPre = Alpha({244, 143, 177, 255}, alpha), cPunct = Alpha({150, 160, 186, 255}, alpha);
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
    if (autoScroll && highlight >= 0) {
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
        RoundRect(r, 18, Alpha(Mix(col::panelHi, t.c, 0.2f), a));
        RoundRectLines(r, 18, 1.5f, Alpha(t.c, a));
        TextCenter(t.msg, r, 15, Alpha(WHITE, a), F_BOLD);
        y += 44;
    }
    gToasts.erase(std::remove_if(gToasts.begin(), gToasts.end(), [](const ToastMsg& t) { return t.t > 2.6f; }),
                  gToasts.end());
}

} // namespace ui
