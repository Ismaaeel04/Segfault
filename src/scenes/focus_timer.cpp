// ============================================================================
// focus_timer.cpp  -  The Focus screen: the Pomodoro timer as a big ring,
// its lengths, and how much you focused today and this week.
// The timer itself is app/focus.cpp, so it keeps going on other screens.
// ============================================================================
#include "app/app.h"
#include "app/focus.h"
#include "app/streak.h"
#include "app/theme.h"
#include "app/ui.h"
#include <algorithm>
#include <cmath>
#include <ctime>

using namespace ui;

namespace {

class FocusScene : public Scene {
public:
    const char* Title() const override { return "Focus"; }
    void Enter(const std::string& param) override {
        t = 0;
        if (param == "start") focus::Start();   // --shot / tests
    }

    void Frame(float dt) override {
        t += dt;
        bool input = InputEnabled();
        if (input && IsKeyPressed(KEY_SPACE) && !AnyTextFocused()) Toggle();
        TimerCard({40, 96, 700, 664}, input);
        Lengths({770, 96, 470, 196}, input);
        Today({770, 308, 470, 250});
        HowItWorks({770, 574, 470, 186});
    }

private:
    float t = 0;

    static Color PhaseColour(focus::Phase p) { return p == focus::WORK ? col::accent : col::success; }
    static void Toggle() { if (focus::Running()) focus::Pause(); else focus::Start(); }

    void TimerCard(Rectangle r, bool input) {
        Panel(r);
        const focus::Phase ph = focus::CurrentPhase();
        const Color c = PhaseColour(ph), c2 = ph == focus::WORK ? col::accent2 : Mix(col::success, col::accent2, 0.4f);
        PanelTint(r, 12, c, 0.10f);
        const Vector2 m = {r.x + r.width / 2, r.y + 290};
        const float R = 196, thick = 14;
        const float rem = focus::Remaining(), len = focus::Length();
        const float frac = std::clamp(1 - rem / len, 0.0f, 1.0f);
        const bool running = focus::Running();

        if (running) SoftGlow(m, R + 40, c, 0.06f + 0.03f * std::sin(t * 2));
        DrawRing(m, R - thick, R, 0, 360, 160, col::panelHi);
        // the elapsed part, coloured along its length, with round ends
        if (frac > 0.001f) {
            const int segs = std::max(2, (int)(frac * 120));
            for (int i = 0; i < segs; i++) {
                float a0 = -90 + 360 * frac * i / segs, a1 = -90 + 360 * frac * (i + 1) / segs;
                DrawRing(m, R - thick, R, a0, a1 + 0.3f, 3, Mix(c, c2, (float)i / segs));
            }
            DrawCircleV({m.x, m.y - R + thick / 2}, thick / 2, c);
            float a = (-90 + 360 * frac) * DEG2RAD;
            Vector2 tip = {m.x + std::cos(a) * (R - thick / 2), m.y + std::sin(a) * (R - thick / 2)};
            DrawCircleV(tip, thick / 2, c2);
            DrawCircleV(tip, thick / 2 + 5, Alpha(c2, 0.18f));
        }
        // inside: the phase, the time, the sessions
        std::string name = focus::PhaseName(ph);
        float bw = Measure(name, 13, F_BOLD).x + 26;
        Rectangle badge = {m.x - bw / 2, m.y - 92, bw, 26};
        RoundRect(badge, 13, Alpha(c, 0.16f));
        TextCenter(name, badge, 13, c, F_BOLD);
        float blink = !running && focus::Active() && std::fmod(t, 1.2f) > 0.6f ? 0.35f : 1.0f;   // paused
        TextCenter(focus::Clock(rem), {m.x - 160, m.y - 52, 320, 84}, 78, Alpha(col::heading, blink), F_MONOBOLD);
        TextCenter(running ? "of " + focus::Clock(len) : focus::Active() ? "paused" : "ready",
                   {m.x - 100, m.y + 36, 200, 20}, 14, col::faint, F_MEDIUM);
        for (int i = 0; i < 4; i++) {
            Vector2 d = {m.x - 33 + i * 22.0f, m.y + 78};
            bool done = i < focus::SessionsDone();
            bool now = i == focus::SessionsDone() && ph == focus::WORK && focus::Active();
            DrawCircleV(d, 6, done ? col::accent : col::panelHi);
            if (now) DrawRing(d, 6.5f, 8.5f, 0, 360, 24, Alpha(col::accent, 0.5f + 0.5f * std::sin(t * 4)));
        }

        // controls
        const float by = r.y + r.height - 104;
        Rectangle main = {m.x - 84, by, 168, 50};
        std::string label = running ? "Pause" : focus::Active() ? "Resume" : "Start";
        if (Button(main, label, BTN_PRIMARY, input, 17)) Toggle();
        if (Button({main.x - 132, by + 7, 116, 36}, "Reset", BTN_GHOST, input && (focus::Active() || focus::SessionsDone() > 0), 14)) focus::Reset();
        if (Button({main.x + main.width + 16, by + 7, 116, 36}, ph == focus::WORK ? "Skip" : "End break", BTN_GHOST, input, 14)) focus::Skip();
        TextCenter("Space starts and pauses. It keeps running on other screens.", {r.x, r.y + r.height - 38, r.width, 18}, 12.5f, col::faint, F_MEDIUM);
    }

    // a row of choices: returns the picked value or -1
    int Chips(float x, float y, const char* label, const int* values, int n, int current, bool enabled) {
        Text(label, x, y + 9, 14, col::text, F_BOLD);
        int picked = -1;
        float cx = x + 110;
        for (int i = 0; i < n; i++) {
            Rectangle b = {cx, y, 64, 34};
            bool on = values[i] == current;
            bool hover = enabled && MouseIn(b);
            RoundRect(b, 9, on ? Alpha(col::accent, enabled ? 0.9f : 0.45f) : hover ? col::panelHi : Alpha(col::panelHi, 0.6f));
            if (!on) RoundRectLines(b, 9, 1, col::border);
            TextCenter(std::to_string(values[i]) + " min", b, 13, on ? OnColor(col::accent) : enabled ? col::text : col::faint, F_BOLD);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) picked = values[i];
            cx += 72;
        }
        return picked;
    }

    void Lengths(Rectangle r, bool input) {
        Panel(r);
        Text("Lengths", r.x + 22, r.y + 18, 17, col::heading, F_HEAVY);
        bool can = input && !focus::Active();
        if (focus::Active()) TextRight("reset the timer to change them", r.x + r.width - 22, r.y + 21, 12.5f, col::faint, F_MEDIUM);
        static const int work[] = {15, 25, 45, 50}, rest[] = {5, 10, 15};
        int w = Chips(r.x + 22, r.y + 62, "Focus", work, 4, app.focusWork, can);
        int b = Chips(r.x + 22, r.y + 110, "Break", rest, 3, app.focusBreak, can);
        if (w > 0 || b > 0) {
            if (w > 0) app.focusWork = w;
            if (b > 0) app.focusBreak = b;
            focus::Reset();
            SaveAppSettings();
        }
        Text("every 4th break is 15 min", r.x + 132, r.y + 160, 12.5f, col::faint, F_MEDIUM);
    }

    void Today(Rectangle r) {
        Panel(r);
        Text("Time focused", r.x + 22, r.y + 18, 17, col::heading, F_HEAVY);
        if (app.user.empty()) {
            TextCenter("Log in to keep a record of your sessions", {r.x, r.y + r.height / 2 - 10, r.width, 20}, 14, col::dim, F_MEDIUM);
            return;
        }
        int today = focus::TodayMinutes();
        Text(std::to_string(today), r.x + 22, r.y + 50, 40, col::heading, F_HEAVY);
        float nw = Measure(std::to_string(today), 40, F_HEAVY).x;
        Text("min today", r.x + 30 + nw, r.y + 72, 14, col::dim, F_MEDIUM);

        // the last 7 days
        std::time_t now = std::time(nullptr);
        std::tm tm = *std::localtime(&now);
        long d0 = streak::DayNumber(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        int mins[7], top = 25, week = 0;
        for (int i = 0; i < 7; i++) {
            mins[i] = 0;
            long d = d0 - 6 + i;
            for (auto& f : app.progress.focusMinutes) { long v; if (streak::Parse(f.first, v) && v == d) mins[i] = f.second; }
            top = std::max(top, mins[i]);
            week += mins[i];
        }
        char wk[48];
        std::snprintf(wk, sizeof(wk), "%dh %02dm this week", week / 60, week % 60);
        TextRight(wk, r.x + r.width - 22, r.y + 21, 13, col::dim, F_BOLD);
        static const char* names[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        const float bx = r.x + 28, base = r.y + r.height - 34, maxH = 92, gap = (r.width - 56) / 7;
        for (int i = 0; i < 7; i++) {
            long d = d0 - 6 + i;
            int wd = (int)(((d % 7) + 7 + 3) % 7);
            float h = mins[i] > 0 ? std::max(6.0f, maxH * mins[i] / top) : 4;
            Rectangle bar = {bx + i * gap + gap / 2 - 16, base - h, 32, h};
            bool isToday = i == 6;
            if (mins[i] > 0) GradientRoundRectV(bar, 6, isToday ? col::accent2 : Alpha(col::accent, 0.85f), isToday ? col::accent : Alpha(col::accent, 0.55f));
            else RoundRect(bar, 2, col::panelHi);
            if (MouseIn({bar.x - 6, base - maxH, bar.width + 12, maxH + 22}) && mins[i] > 0)
                TextCenter(std::to_string(mins[i]) + "m", {bar.x - 20, bar.y - 20, 72, 16}, 12, col::heading, F_BOLD);
            TextCenter(names[wd], {bar.x - 10, base + 8, 52, 16}, 12, isToday ? col::heading : col::faint, isToday ? F_BOLD : F_MEDIUM);
        }
    }

    void HowItWorks(Rectangle r) {
        Panel(r);
        Text("How it works", r.x + 22, r.y + 18, 17, col::heading, F_HEAVY);
        RichText({r.x + 22, r.y + 52, r.width - 44, r.height - 60},
                 "- Work on **one** thing until the timer rings: a lesson, a problem, a mock paper.\n"
                 "- Then take the break, away from the screen. The app tells you when it's over.\n"
                 "- After four sessions, take a longer one. Only finished sessions count.",
                 14, col::text);
    }
};

} // namespace

Scene* CreateFocusScene() { return new FocusScene(); }
