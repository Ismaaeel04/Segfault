// ============================================================================
// intro.cpp  -  The start-up screen: the {*} logo, the name letter by letter
// and a loading bar that follows what the app is really doing (it finds the
// compilers in the background). Then the login screen, or home if the last
// session is remembered. Any key or click skips it.
// ============================================================================
#include "app/app.h"
#include "app/runner.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>

using namespace ui;

namespace {

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float EaseOutCubic(float t) { t = Clamp01(t); return 1 - (1 - t) * (1 - t) * (1 - t); }
float EaseOutBack(float t) {
    t = Clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * std::pow(t - 1, 3.0f) + c1 * std::pow(t - 1, 2.0f);
}
// 0 before `start`, 1 after `start + length`
float Phase(float t, float start, float length) { return Clamp01((t - start) / length); }

class IntroScene : public Scene {
public:
    const char* Title() const override { return ""; }
    void Enter(const std::string& param) override {
        next = param == "home" ? SceneId::Home : SceneId::Login;
        t = 0;
        leaving = -1;
    }

    void Frame(float dt) override {
        t += dt;
        const bool ready = runner::DetectionDone();
        // the bar fills in about 2 s, then waits for the compilers, for 3.5 s at most
        float target = std::min(t / 2.0f, ready ? 1.0f : 0.9f);
        if (t > 3.5f) target = 1;
        progress += (target - progress) * std::min(1.0f, dt * 8);
        if (leaving < 0 && progress > 0.995f && t > 2.2f) leaving = t;
        if (leaving < 0 && t > 0.25f && (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) leaving = t;   // skip
        if (leaving >= 0 && t - leaving > 0.35f) { app.Go(next); return; }

        const Vector2 c = {W / 2, H / 2 - 70};
        DrawCodeFragments(c);

        // ---- the logo: pops in with a little overshoot, glow breathing behind it
        float pop = EaseOutBack(Phase(t, 0.05f, 0.7f));
        float glow = 0.5f + 0.5f * std::sin(t * 2.4f);
        SoftGlow(c, 230 + glow * 30, col::accent, 0.22f * Phase(t, 0, 0.6f));
        SoftGlow({c.x + 40, c.y + 30}, 180, col::accent2, 0.12f * Phase(t, 0.2f, 0.6f));
        for (int k = 0; k < 2; k++) {   // two rings spreading out
            float r = std::fmod(t * 0.55f + k * 0.5f, 1.0f);
            DrawRing(c, 70 + r * 120, 72 + r * 120, 0, 360, 64, Alpha(col::accent, 0.25f * (1 - r) * Phase(t, 0.3f, 0.5f)));
        }
        float size = 128 * pop;
        if (size > 1) DrawLogo({c.x - size / 2, c.y - size / 2, size, size}, t * 1.4f, 0.4f + 0.4f * glow);

        // ---- the name, letter by letter
        const std::string name = "SegFault";
        const float fs = 58;
        float total = Measure(name, fs, F_DISPLAY).x, x = W / 2 - total / 2, y = c.y + 104;
        for (size_t i = 0; i < name.size(); i++) {
            std::string ch(1, name[i]);
            float a = EaseOutCubic(Phase(t, 0.45f + i * 0.055f, 0.35f));
            float advance = Measure(name.substr(0, i + 1), fs, F_DISPLAY).x - Measure(name.substr(0, i), fs, F_DISPLAY).x;
            if (a > 0) Text(ch, x, y + (1 - a) * 18, fs, Alpha(col::heading, a), F_DISPLAY);
            x += advance;
        }
        float tag = EaseOutCubic(Phase(t, 1.0f, 0.5f));
        TextCenter("Learn C & C++ by writing it", {0, y + 70 + (1 - tag) * 8, W, 24}, 18, Alpha(col::dim, tag), F_MEDIUM);

        // ---- the loading bar and what it is doing
        float barIn = EaseOutCubic(Phase(t, 0.7f, 0.4f));
        Rectangle bar = {W / 2 - 180, y + 130, 360, 6};
        RoundRect(bar, 3, Alpha(col::panelHi, barIn));
        if (progress > 0.01f) {
            Rectangle fill = {bar.x, bar.y, std::max(bar.height, bar.width * progress), bar.height};
            GradientRoundRect(fill, 3, col::accent, Mix(col::accent, col::accent2, 0.8f));
            SoftGlow({fill.x + fill.width, fill.y + 3}, 18, col::accent2, 0.5f * barIn);
        }
        TextCenter(Status(ready), {0, bar.y + 18, W, 20}, 14, Alpha(col::faint, barIn), F_MEDIUM);

        float hint = Phase(t, 1.6f, 0.6f) * (0.55f + 0.45f * std::sin(t * 3));
        TextCenter("press any key to skip", {0, H - 46, W, 18}, 12.5f, Alpha(col::faint, hint * 0.8f), F_MEDIUM);

        // ---- fade out into the next screen
        if (leaving >= 0) DrawRectangleRec({0, 0, W, H}, Alpha(col::bg, Clamp01((t - leaving) / 0.35f)));
    }

private:
    SceneId next = SceneId::Login;
    float t = 0, progress = 0, leaving = -1;

    std::string Status(bool ready) const {
        if (progress < 0.3f) return "Loading fonts and themes";
        if (progress < 0.6f) return "Loading " + std::to_string(Lessons().size()) + " lessons and " +
                                    std::to_string(Problems().size()) + " problems";
        if (!ready) return "Looking for a C / C++ compiler";
        if (const Compiler* c = runner::Active()) return "Compiler: " + c->label;
        return "Ready";
    }

    // bits of C drifting up and fading, behind the logo
    void DrawCodeFragments(Vector2 c) const {
        static const char* bits[] = {"int*", "&x", "->next", "malloc", "nullptr", "*p = 42;", "{ }", "free(p)",
                                     "char[]", "i++", "NULL", "delete[]", "p + 1", "struct Node"};
        const int n = sizeof(bits) / sizeof(bits[0]);
        float in = Phase(t, 0.2f, 0.8f);
        for (int i = 0; i < n; i++) {
            float seed = i * 2.399f;                                   // golden angle: spread evenly
            float speed = 10 + (i % 4) * 4;
            float life = std::fmod(t * speed / 200 + i * 0.137f, 1.0f);
            float ang = seed, dist = 210 + (i % 5) * 46;
            Vector2 p = {c.x + std::cos(ang) * dist * 1.5f, c.y + 40 + std::sin(ang) * dist * 0.75f - life * 90};
            float a = std::sin(life * 3.14159f) * 0.22f * in;
            Text(bits[i], p.x, p.y, 15, Alpha(i % 3 == 0 ? col::accent : i % 3 == 1 ? col::accent2 : col::dim, a), F_MONO);
        }
    }
};

} // namespace

Scene* CreateIntroScene() { return new IntroScene(); }
