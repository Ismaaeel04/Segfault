// ============================================================================
// intro.cpp  -  "SegFault OS" booting: a console that lists what the app is
// really loading (lessons, problems, the compiler it finds in the background),
// then the logo with a spinner, then the sign-in screen (login.cpp), or the
// desktop if the last session is still open. Any key or click skips ahead.
// ============================================================================
#include "app/app.h"
#include "app/runner.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace ui;

namespace {

struct BootLine {
    std::string text;      // "Mounting /lessons"
    std::string detail;    // "24 lessons"
    bool waitsForCompiler = false;
};

class BootScene : public Scene {
public:
    const char* Title() const override { return ""; }
    void Enter(const std::string& param) override {
        next = param;
        t = 0;
        leaving = -1;
        splashAt = -1;
        compilerAt = -1;
        compilerDone = -1;
        lines = {
            {"Checking memory", "0x7ffe0000 - 0x7fffffff"},
            {"Allocating heap", "malloc(65536)"},
            {"Mounting /lessons", std::to_string(Lessons().size()) + " lessons"},
            {"Mounting /problems", std::to_string(Problems().size()) + " problems"},
            {"Loading /quiz", std::to_string(Questions().size()) + " questions"},
            {"Loading theme", Themes()[CurrentTheme()].name},
            {"Looking for a C / C++ compiler", "", true},
            {"Dereferencing pointers", "*p = 42"},
        };
    }

    void Frame(float dt) override {
        t += dt;
        const float step = 0.16f, start = 0.35f;
        // how many lines are on screen; the compiler line waits (3 s at most)
        int shown = std::clamp((int)((t - start) / step) + 1, 0, (int)lines.size());
        int compilerLine = 6;
        if (shown > compilerLine) {
            if (compilerAt < 0) compilerAt = t;
            bool found = runner::DetectionDone() || t - compilerAt > 3.0f;
            if (!found) shown = compilerLine + 1;
            else if (compilerDone < 0) compilerDone = t;
            if (compilerDone >= 0) shown = std::min((int)lines.size(), compilerLine + 1 + (int)((t - compilerDone) / step));
        }
        bool finished = shown == (int)lines.size() && compilerDone >= 0 && t - compilerDone > step * 2 + 0.5f;
        bool key = t > 0.3f && (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        // the console, then the logo with a spinner for a moment, then sign in
        if (splashAt < 0 && (finished || key)) splashAt = t;
        else if (splashAt >= 0 && leaving < 0 && ((key && t - splashAt > 0.25f) || t - splashAt > 1.7f)) leaving = t;
        if (leaving >= 0 && t - leaving > 0.4f) { Leave(); return; }

        DrawRectangleRec(ScreenRect(), Mix(col::bg, BLACK, 0.55f));
        const float x = 120, fs = 16, lh = 30;
        float y = 120;
        DrawLogo({x, y - 6, 44, 44}, t * 1.2f, 0.3f);
        Text("SegFault OS 1.0", x + 60, y - 2, 22, col::heading, F_MONOBOLD);
        Text("C & C++ exam prep  -  x86_64", x + 60, y + 24, 13, col::faint, F_MONO);
        y += 84;

        const float dots = x + 470;
        for (int i = 0; i < shown; i++) {
            const BootLine& l = lines[i];
            bool waiting = l.waitsForCompiler && compilerDone < 0;
            float age = Phase(t, start + i * step, 0.12f);
            Color a = Alpha(col::text, age);
            // [ OK ] / [ .. ] / [WARN]
            std::string tag = "  OK  ";
            Color tagCol = col::success;
            std::string detail = l.detail;
            if (l.waitsForCompiler) {
                const Compiler* c = runner::Active();
                if (waiting) { tag = Spinner(); tagCol = col::warn; }
                else if (c) detail = c->label;
                else { tag = " WARN "; tagCol = col::warn; detail = "none found"; }
            }
            Text("[", x, y, fs, Alpha(col::faint, age), F_MONO);
            Text(tag, x + CodeCharWidth(fs), y, fs, Alpha(tagCol, age), F_MONOBOLD);
            Text("]", x + CodeCharWidth(fs) * 7, y, fs, Alpha(col::faint, age), F_MONO);
            Text(l.text, x + CodeCharWidth(fs) * 9, y, fs, a, F_MONO);
            // dot leader up to the detail
            float tx = x + CodeCharWidth(fs) * 9 + Measure(l.text, fs, F_MONO).x + 10;
            for (float dx = tx; dx < dots - 8; dx += CodeCharWidth(fs)) Text(".", dx, y, fs, Alpha(col::faint, age * 0.7f), F_MONO);
            Text(detail, dots, y, fs, Alpha(col::accent2, age), F_MONO);
            y += lh;
        }
        // blinking cursor, then "Starting desktop..."
        if (shown == (int)lines.size() && compilerDone >= 0) {
            y += 10;
            Text(appStart(), x, y, fs, col::heading, F_MONO);
        }
        if (std::fmod(t, 1.0f) < 0.55f)
            DrawRectangleRec({x + (shown == (int)lines.size() && compilerDone >= 0 ? Measure(appStart(), fs, F_MONO).x + 8 : 0),
                              y + (shown == (int)lines.size() && compilerDone >= 0 ? 0 : 4), CodeCharWidth(fs), fs + 2}, col::accent);

        // progress along the bottom
        float p = std::min(1.0f, (float)shown / lines.size() * (compilerDone >= 0 ? 1.0f : 0.92f));
        Rectangle bar = {x, H - 110, W - 240, 4};
        RoundRect(bar, 2, col::panelHi);
        if (p > 0) GradientRoundRect({bar.x, bar.y, std::max(4.0f, bar.width * p), bar.height}, 2, col::accent, col::accent2);
        Text("press any key to skip", x, H - 92, 12.5f, Alpha(col::faint, 0.8f), F_MEDIUM);

        if (splashAt >= 0) Splash(Phase(t, splashAt, 0.35f));
        if (leaving >= 0) DrawRectangleRec(ScreenRect(), Alpha(BLACK, std::clamp((t - leaving) / 0.4f, 0.0f, 1.0f)));
    }

private:
    std::string next;
    std::vector<BootLine> lines;
    float t = 0, leaving = -1, splashAt = -1, compilerAt = -1, compilerDone = -1;

    // The logo, and five dots chasing round under it (they speed up and slow
    // down like the ones Windows shows while it starts).
    void Splash(float k) {
        Rectangle scr = ScreenRect();
        DrawRectangleRec(scr, Alpha(Mix(col::bg, BLACK, 0.7f), k));
        const float s = 112;
        Vector2 c = {W / 2, H / 2 - 60};
        SoftGlow(c, 220, col::accent, 0.16f * k);
        if (k > 0.05f) DrawLogo({c.x - s / 2, c.y - s / 2, s, s}, t * 1.5f, 0.5f);
        if (k < 1) DrawRectangleRec(scr, Alpha(Mix(col::bg, BLACK, 0.7f), 1 - k));   // fades the logo in
        Vector2 sc = {W / 2, H / 2 + 92};
        for (int i = 0; i < 5; i++) {
            float u = std::fmod(t * 0.62f - i * 0.075f + 10, 1.0f);
            float e = u * u * (3 - 2 * u);   // ease in and out
            float a = -PI / 2 + e * 2 * PI;
            DrawCircleV({sc.x + std::cos(a) * 20, sc.y + std::sin(a) * 20}, 3.2f, Alpha(WHITE, 0.9f * k));
        }
        TextCenter("SegFault OS", {0, H / 2 + 140, W, 24}, 17, Alpha(col::dim, k), F_BOLD);
    }

    static float Phase(float t, float s, float len) { return std::clamp((t - s) / len, 0.0f, 1.0f); }
    std::string Spinner() const {
        static const char* f[] = {" |    ", " /    ", " -    ", " \\    "};
        return f[(int)(t * 10) % 4];
    }
    static std::string appStart() { return "Starting desktop..."; }

    void Leave() {
        // nobody signed in: the sign-in screen; the last session still open: straight in
        if (next != "home" || app.user.empty()) app.Go(SceneId::Login);
        else if (app.bootToApp) app.Go(SceneId::Home);
        else app.Go(SceneId::Desktop, "home");
    }
};

} // namespace

Scene* CreateIntroScene() { return new BootScene(); }
