// ============================================================================
// intro.cpp  -  "SegFault OS" starting up, the way a PC does:
//   1. the firmware's POST screen: grey text on black, with this machine's
//      real processor and memory (counted up), and what the app really loads
//      (the compiler it finds, the lessons, the problems);
//   2. a moment of black while the "monitor" changes mode;
//   3. the system's logo on black with a ring of dots chasing round under it
//      (the timing of the progress ring Windows shows while it starts);
// then the sign-in screen (login.cpp), or the desktop if the last session is
// still open. A key or a click skips to the next part.
// ============================================================================
#include "app/app.h"
#include "app/process.h"
#include "app/runner.h"
#include "app/sound.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <vector>

using namespace ui;

namespace {

const Color kGrey = {192, 192, 192, 255};    // the firmware's text
const Color kWhite = {255, 255, 255, 255};

// --------------------------------------------------- the progress ring ---
// Each dot follows these keyframes (a 5.5 s cycle, every dot 0.24 s behind the
// one before): quick, slow, quick, slow round the circle, then it vanishes and
// waits for the next lap. Angles are clockwise from the top.
struct Key { float at, angle, alpha; int ease; };   // ease: 0 linear, 1 out, 2 in-out
const Key kRing[] = {
    {0.00f, 225, 1, 1}, {0.07f, 345, 1, 0}, {0.30f, 455, 1, 2}, {0.39f, 690, 1, 0},
    {0.70f, 815, 1, 1}, {0.75f, 945, 1, 1}, {0.76f, 945, 0, 0}, {1.00f, 945, 0, 0},
};

float Ease(float u, int kind) {
    if (kind == 1) return 1 - (1 - u) * (1 - u);
    if (kind == 2) return u * u * (3 - 2 * u);
    return u;
}

void DrawProgressRing(Vector2 c, float radius, float dot, float time) {
    const float cycle = 5.5f, lag = 0.24f;
    for (int i = 0; i < 5; i++) {
        float tt = time - i * lag;
        if (tt < 0) continue;
        float p = std::fmod(tt, cycle) / cycle;
        int k = 0;
        while (k < 6 && p >= kRing[k + 1].at) k++;
        const Key& a = kRing[k];
        const Key& b = kRing[k + 1];
        float u = Ease((p - a.at) / std::max(1e-4f, b.at - a.at), a.ease);
        float ang = (a.angle + (b.angle - a.angle) * u) * DEG2RAD;
        float alpha = a.alpha + (b.alpha - a.alpha) * u;
        if (alpha <= 0.01f) continue;
        DrawCircleV({c.x + std::sin(ang) * radius, c.y - std::cos(ang) * radius}, dot, Alpha(kWhite, alpha));
    }
}

class BootScene : public Scene {
public:
    const char* Title() const override { return ""; }
    void Enter(const std::string& param) override {
        next = param;
        t = 0;
        postEnd = logoStart = leaving = -1;
        compilerAt = compilerShown = -1;
        cpu = CpuName();
        if (cpu.empty()) cpu = "Unknown processor";
        cores = std::max(1u, std::thread::hardware_concurrency());
        ramK = TotalRamBytes() / 1024;
        if (ramK == 0) ramK = 8ull * 1024 * 1024;
        std::time_t now = std::time(nullptr);
        std::tm tm = *std::localtime(&now);
        char d[16];
        std::strftime(d, sizeof(d), "%m/%d/%y", &tm);
        date = d;
        // SEGFAULT_BOOT_LOGO: start at the logo (to check it with --shot)
        if (std::getenv("SEGFAULT_BOOT_LOGO")) { postEnd = 0; logoStart = 0; }
    }

    void Frame(float dt) override {
        t += dt;
        bool key = t > 0.25f && (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        DrawRectangleRec(ScreenRect(), BLACK);

        if (postEnd < 0) {   // ---- 1. POST
            bool done = Post();
            if (done || key) { postEnd = t; logoStart = t + (key ? 0.25f : 0.45f); }
            return;
        }
        if (t < logoStart) return;   // ---- 2. black, between the two

        // ---- 3. the logo and the ring
        float lt = t - logoStart;
        if (leaving < 0 && ((lt > 3.2f && runner::DetectionDone()) || lt > 6.0f || (key && lt > 0.2f))) {
            leaving = t;
            snd::Play(snd::STARTUP);
        }
        float in = std::clamp(lt / 0.5f, 0.0f, 1.0f);
        const float s = 132;
        Rectangle logo = {W / 2 - s / 2, H * 0.40f - s / 2, s, s};
        DrawLogo(logo, 0, 0);   // still, like a boot logo
        if (in < 1) DrawRectangleRec(ScreenRect(), Alpha(BLACK, 1 - in));
        if (lt > 0.7f) DrawProgressRing({W / 2, H * 0.73f}, 19, 3.1f, lt - 0.7f);
        if (leaving >= 0) {
            float k = std::clamp((t - leaving) / 0.45f, 0.0f, 1.0f);
            DrawRectangleRec(ScreenRect(), Alpha(BLACK, k));
            if (k >= 1) Leave();
        }
    }

private:
    std::string next, cpu, date;
    unsigned cores = 1;
    unsigned long long ramK = 0;
    float t = 0, postEnd = -1, logoStart = -1, leaving = -1, compilerAt = -1, compilerShown = -1;

    // The firmware screen. Lines appear one after another; returns true once
    // everything is shown and has stayed a moment.
    bool Post() {
        const float fs = 17, lh = 23, x = -ExtraX() + 40;
        float y = 34;
        auto Line = [&](float at, const std::string& s, Color c = kGrey) {
            if (t >= at) Text(s, x, y, fs, c, F_MONO);
            y += lh;
        };
        if (t < 0.25f) return false;   // the screen wakes up
        // the vendor's mark, top right, as on a real board
        DrawLogo({W + ExtraX() - 40 - 76, 30, 76, 76}, 0, 0);
        Line(0.25f, "SegFault BIOS v5.50, (C) 2026 SegFault Systems, Inc.", kWhite);
        Line(0.25f, "SegFault OS Mainboard  -  BIOS Date " + date);
        y += lh;
        Line(0.60f, "Main Processor : " + cpu);
        Line(0.60f, "Logical Cores  : " + std::to_string(cores));
        // the memory test counts up to what the machine really has
        if (t >= 0.80f) {
            float k = std::clamp((t - 0.80f) / 0.9f, 0.0f, 1.0f);
            unsigned long long shown = (unsigned long long)(ramK * (k * k * (3 - 2 * k)));
            shown -= shown % 1024;
            Text("Memory Testing : " + std::to_string(k >= 1 ? ramK : shown) + "K" + (k >= 1 ? " OK" : ""), x, y, fs, kGrey, F_MONO);
        }
        y += lh * 2;
        // the compiler: "Detecting..." until the search in the background ends (3 s at most)
        const float detectAt = 1.9f;
        if (t >= detectAt && compilerAt < 0) compilerAt = t;
        bool found = compilerAt >= 0 && (runner::DetectionDone() || t - compilerAt > 3.0f);
        if (found && compilerShown < 0) compilerShown = t;
        if (t >= detectAt) {
            std::string what = "Detecting C/C++ Compiler ... ";
            if (compilerShown >= 0) {
                const Compiler* c = runner::Active();
                what += c ? c->label : "None";
            } else {
                static const char* dots[] = {"", ".", "..", "..."};
                what += dots[(int)(t * 4) % 4];
            }
            Text(what, x, y, fs, kGrey, F_MONO);
        }
        y += lh;
        float after = compilerShown >= 0 ? compilerShown : 1e9f;
        Line(after + 0.20f, "Lessons ROM    : " + std::to_string(Lessons().size()) + " lessons");
        Line(after + 0.35f, "Problem Bank   : " + std::to_string(Problems().size()) + " problems, " + std::to_string(Questions().size()) + " questions");
        Line(after + 0.50f, "USB Device(s)  : 1 Keyboard, 1 Mouse");
        y += lh;
        Line(after + 0.75f, "Starting SegFault OS...");
        // the bottom line, all the time
        Text("Press DEL to run Setup, F12 for Boot Menu", x, H - 46, fs, kGrey, F_MONO);
        return t > after + 1.35f;
    }

    void Leave() {
        // nobody signed in: the sign-in screen; the last session still open: straight in
        if (next != "home" || app.user.empty()) app.Go(SceneId::Login);
        else if (app.bootToApp) app.Go(SceneId::Home);
        else app.Go(SceneId::Desktop, "home");
    }
};

} // namespace

Scene* CreateIntroScene() { return new BootScene(); }
