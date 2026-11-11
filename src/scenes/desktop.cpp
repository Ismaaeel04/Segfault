// ============================================================================
// desktop.cpp  -  The "SegFault OS" desktop: a wallpaper, three icons
// (SegFault, SegFault Editor, Focus), a countdown to the exam, and a taskbar
// with a start menu and a clock. Double-click an icon (or click it on the
// taskbar) and it opens out of it as a window. Logging out comes back here.
// "Shut down" closes the program.
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

float EaseOutCubic(float t) { t = std::clamp(t, 0.0f, 1.0f); return 1 - (1 - t) * (1 - t) * (1 - t); }
float EaseInOut(float t) { t = std::clamp(t, 0.0f, 1.0f); return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0f) / 2; }
Rectangle Lerp(Rectangle a, Rectangle b, float k) {
    return {a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.width + (b.width - a.width) * k, a.height + (b.height - a.height) * k};
}

// day number -> y/m/d (Howard Hinnant's civil_from_days, the inverse of streak::DayNumber)
void CivilFromDays(long z, int& y, int& m, int& d) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yoe + era * 400) + (m <= 2);
}

long TodayNumber() {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    return streak::DayNumber(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

enum AppKind { APP_SEGFAULT, APP_EDITOR, APP_FOCUS, APP_AI, APP_COUNT };
const char* kAppName[APP_COUNT] = {"SegFault", "SegFault Editor", "Focus", "SegFault AI"};
const char* kAppLine[APP_COUNT] = {"Learn C & C++ by writing it", "Your own C and C++ files", "A Pomodoro timer for studying",
                                   "A tutor for the exam"};

// The editor's icon: a page of code with a folded corner.
void DrawEditorIcon(Rectangle r, float hot) {
    float s = r.width / 64;
    Rectangle page = {r.x + 10 * s, r.y + 4 * s, 44 * s, 56 * s};
    Shadow(page, 7 * s, 0.6f);
    RoundRect(page, 7 * s, Mix(col::panel, col::panelHi, 0.5f));
    RoundRectLines(page, 7 * s, 1.2f, Mix(col::border, col::accent, hot * 0.6f));
    // folded corner
    Vector2 a = {page.x + page.width - 13 * s, page.y}, b = {page.x + page.width, page.y + 13 * s};
    DrawTriangle(a, {a.x, b.y}, b, Alpha(col::accent, 0.55f));
    const Color c[5] = {col::synKey, col::synFn, col::synStr, col::synType, col::synCom};
    const float w[5] = {18, 26, 20, 14, 22}, ind[5] = {0, 5, 5, 5, 0};
    for (int i = 0; i < 5; i++)
        RoundRect({page.x + 7 * s + ind[i] * s, page.y + 18 * s + i * 7.5f * s, w[i] * s, 3.4f * s}, 1.7f * s, c[i]);
    // the {*} star in the corner
    DrawCircleV({page.x + page.width - 9 * s, page.y + page.height - 9 * s}, 3 * s, col::accent2);
}

// Focus's icon: a ring that is partly filled, like the timer.
void DrawFocusIcon(Rectangle r, float time, float hot) {
    float s = r.width / 64;
    Vector2 m = {r.x + 32 * s, r.y + 33 * s};
    DrawCircleV(m, 27 * s, Mix(col::panel, col::panelHi, 0.5f));
    DrawRing(m, 21 * s, 27 * s, 0, 360, 48, col::panelHi);
    float frac = focus::Active() ? std::clamp(1 - focus::Remaining() / focus::Length(), 0.02f, 1.0f) : 0.68f;
    Color c = focus::CurrentPhase() == focus::WORK ? col::accent : col::success;
    DrawRing(m, 21 * s, 27 * s, -90, -90 + 360 * frac, 48, Mix(c, col::accent2, 0.25f));
    // two hands
    float a = (-90 + 360 * frac) * DEG2RAD;
    DrawLineEx(m, {m.x + std::cos(a) * 13 * s, m.y + std::sin(a) * 13 * s}, 2.6f * s, col::heading);
    DrawLineEx(m, {m.x, m.y - 9 * s}, 2.6f * s, col::heading);
    DrawCircleV(m, 3 * s, c);
    if (hot > 0.5f || focus::Running()) DrawCircleV(m, (5 + std::sin(time * 4)) * s, Alpha(c, 0.25f));
}

void DrawAppIcon(int kind, Rectangle r, float time, float hot) {
    if (kind == APP_SEGFAULT) DrawLogo(r, time, hot);
    else if (kind == APP_EDITOR) DrawEditorIcon(r, hot);
    else if (kind == APP_FOCUS) DrawFocusIcon(r, time, hot);
    else DrawAiLogo(r, time, hot);
}

class DesktopScene : public Scene {
public:
    const char* Title() const override { return ""; }
    void Enter(const std::string& param) override {
        next = param.empty() ? (app.user.empty() ? "login" : "home") : param;
        t = 0;
        selected = -1;
        menu = false;
        opening = -1;
        shutting = -1;
        lastClick = -10;
    }

    void Frame(float dt) override {
        t += dt;
        const Rectangle scr = ScreenRect();   // the whole screen, wider than 1280 on wide displays
        const float L = scr.x, R = scr.x + scr.width;
        const Rectangle bar = {L, H - 54, scr.width, 54};
        const Rectangle startBtn = {W / 2 - 126, H - 49, 44, 44};
        const bool busy = opening >= 0 || shutting >= 0;
        const bool input = InputEnabled() && !busy;

        Wallpaper();
        ExamWidget({R - 336, 36, 300, 168}, input && !menu);

        // ---- the icons: click selects, double-click opens
        bool clickedIcon = false;
        for (int i = 0; i < APP_COUNT; i++) {
            Rectangle icon = IconRect(i);
            bool hover = input && !menu && MouseIn(icon);
            bool sel = selected == i;
            if (hover || sel) RoundRect(icon, 12, Alpha(hover && !sel ? col::heading : col::accent, sel ? 0.22f : 0.08f));
            if (sel) RoundRectLines(icon, 12, 1, Alpha(col::accent, 0.6f));
            float bounce = opening >= 0 && openApp == i ? std::sin(std::min(1.0f, (t - opening) * 3) * 3.14159f) * 6 : 0;
            DrawAppIcon(i, {icon.x + 20, icon.y + 10 - bounce, 64, 64}, t, hover ? 1.0f : 0.2f);
            // the label, on two lines if it needs them
            std::vector<std::string> lines = Wrap(kAppName[i], 14.5f, F_BOLD, icon.width - 4);
            for (size_t k = 0; k < lines.size() && k < 2; k++) {
                float ly = icon.y + 82 + k * 18;
                if (!Themes()[CurrentTheme()].light)   // a shadow, so the label reads on any wallpaper
                    TextCenter(lines[k], {icon.x + 1, ly + 1, icon.width, 20}, 14.5f, Alpha(BLACK, 0.45f), F_BOLD);
                TextCenter(lines[k], {icon.x, ly, icon.width, 20}, 14.5f, col::heading, F_BOLD);
            }
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (sel && t - lastClick < 0.5f) Open(i, icon);   // a double-click, as on Windows
                selected = i;
                lastClick = t;
                clickedIcon = true;
            }
        }
        if (input && !menu && !clickedIcon && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !MouseIn(bar)) selected = -1;
        if (input && selected >= 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) Open(selected, IconRect(selected));
        if (input && !menu && (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP))) {   // arrows move between icons
            int d = IsKeyPressed(KEY_DOWN) ? 1 : -1;
            selected = selected < 0 ? 0 : (selected + d + APP_COUNT) % APP_COUNT;
        }

        // ---- taskbar
        DrawRectangleRec(bar, Alpha(Mix(col::bg2, col::panel, 0.5f), 0.94f));
        DrawRectangleRec({L, bar.y, scr.width, 1}, col::border);
        if (TaskButton(startBtn, input, menu)) menu = !menu;
        for (int i = 0; i < 4; i++)   // the start button: four tiles
            RoundRect({startBtn.x + 12 + (i % 2) * 11.0f, startBtn.y + 12 + (i / 2) * 11.0f, 9, 9}, 2.5f,
                      i == 0 ? col::accent : i == 3 ? col::accent2 : Mix(col::accent, col::accent2, 0.5f));
        for (int i = 0; i < APP_COUNT; i++) {
            Rectangle b = {startBtn.x + 52 + i * 52.0f, H - 49, 44, 44};
            bool running = (opening >= 0 && openApp == i) || (i == APP_FOCUS && focus::Active());
            if (TaskButton(b, input && !menu, opening >= 0 && openApp == i)) Open(i, b);
            DrawAppIcon(i, {b.x + 9, b.y + 9, 26, 26}, 0.6f, 0);
            if (running) RoundRect({b.x + 14, b.y + 39, 16, 3}, 1.5f, i == APP_FOCUS && focus::CurrentPhase() != focus::WORK ? col::success : col::accent);
            if (input && !menu && MouseIn(b)) Tooltip(b, kAppName[i]);
        }
        Clock(bar);
        Text("SegFault OS", L + 18, bar.y + 18, 14, col::dim, F_BOLD);
        if (focus::Active()) {   // the timer, next to the clock
            std::string s = std::string(focus::CurrentPhase() == focus::WORK ? "Focus " : "Break ") + focus::Clock(focus::Remaining());
            float w = Measure(s, 13.5f, F_MONOBOLD).x + 30;
            Rectangle pill = {R - 120 - w, bar.y + 11, w, 32};
            Color c = focus::CurrentPhase() == focus::WORK ? col::accent : col::success;
            bool hover = input && !menu && MouseIn(pill);
            RoundRect(pill, 16, Alpha(c, hover ? 0.24f : 0.14f));
            DrawCircleV({pill.x + 14, pill.y + 16}, 4, focus::Running() ? c : col::faint);
            Text(s, pill.x + 24, pill.y + 8, 13.5f, col::heading, F_MONOBOLD);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Open(APP_FOCUS, pill);
        }

        // ---- start menu
        if (menu) StartMenu(startBtn, input);
        if (input && IsKeyPressed(KEY_ESCAPE)) { menu = false; selected = -1; }

        // ---- opening: a window grows out of the icon to fill the screen
        if (opening >= 0) {
            float k = EaseInOut((t - opening) / 0.45f);
            Rectangle w = Lerp(openFrom, scr, k);
            Shadow(w, 12 * (1 - k), 2);
            RoundRect(w, 12 * (1 - k), col::panel);
            Rectangle title = {w.x, w.y, w.width, std::min(36.0f, w.height)};
            RoundRect(title, 12 * (1 - k), col::bg2);
            float s = std::min(w.width, w.height) * 0.22f;
            if (s > 4) DrawAppIcon(openApp, {w.x + w.width / 2 - s / 2, w.y + w.height / 2 - s / 2, s, s}, t, 0.5f);
            if (w.width > 180) {
                Text(kAppName[openApp], w.x + 14, w.y + 9, 14, col::heading, F_BOLD);
                for (int i = 0; i < 3; i++) DrawCircleV({w.x + w.width - 18 - i * 18.0f, w.y + 18}, 5, i == 0 ? col::danger : i == 1 ? col::warn : col::success);
            }
            if (t - opening > 0.5f) {
                if (openApp == APP_SEGFAULT) app.Go(next == "home" ? SceneId::Home : SceneId::Login);
                else app.Open(openApp == APP_EDITOR ? SceneId::Editor : openApp == APP_FOCUS ? SceneId::Focus : SceneId::Ai, "", next);   // Back comes here
            }
        }
        // ---- shutting down
        if (shutting >= 0) {
            float k = EaseOutCubic((t - shutting) / 0.7f);
            DrawRectangleRec(scr, Alpha(BLACK, k));
            TextCenter("Shutting down...", {0, H / 2 - 12, W, 24}, 16, Alpha(WHITE, k), F_MONO);
            if (t - shutting > 0.9f) app.quitRequested = true;
        }
    }

private:
    std::string next;
    float t = 0, opening = -1, shutting = -1, lastClick = -10;
    int selected = -1, openApp = 0;
    bool menu = false;
    Rectangle openFrom = {0, 0, 0, 0};

    static Rectangle IconRect(int i) { return {-ExtraX() + 44, 40 + i * 136.0f, 104, 126}; }   // the screen's left edge

    void Open(int kind, Rectangle from) {
        if (opening >= 0) return;
        opening = t;
        openApp = kind;
        openFrom = from;
        menu = false;
    }

    void Tooltip(Rectangle b, const std::string& s) {
        float w = Measure(s, 12.5f, F_BOLD).x + 20;
        Rectangle r = {b.x + b.width / 2 - w / 2, b.y - 36, w, 26};
        Shadow(r, 7, 1);
        RoundRect(r, 7, col::panelHi);
        TextCenter(s, r, 12.5f, col::heading, F_BOLD);
    }

    // a big faint {*} and some soft light; the backdrop under it is the theme's
    void Wallpaper() {
        SoftGlow({W / 2, H / 2 - 30}, 420, col::accent, 0.12f);
        SoftGlow({W / 2 + 220, H / 2 + 120}, 300, col::accent2, 0.08f);
        Rectangle scr = ScreenRect();
        DotGrid({scr.x, 0, scr.width, H - 54}, 28, Alpha(col::border, 0.35f));
        const float fs = 240;
        TextCenter("{*}", {0, H / 2 - fs / 2 - 40, W, fs}, fs, Alpha(col::heading, 0.05f), F_MONOBOLD);
        TextCenter("SegFault OS", {0, H / 2 + 92, W, 30}, 22, Alpha(col::heading, 0.09f), F_DISPLAY);
    }

    // ---- the exam countdown: calm far away, amber the last week, red the last days
    void ExamWidget(Rectangle r, bool input) {
        long exam = 0;
        if (!streak::Parse(app.examDate, exam)) { exam = TodayNumber() + 10; }
        const long left = exam - TodayNumber();
        int ey, em, ed;
        CivilFromDays(exam, ey, em, ed);

        // the colour moves smoothly: green (10+ days) -> amber (the last week) -> red (1 and the day)
        Color c;
        if (left < 0) c = col::faint;
        else if (left >= 10) c = col::success;
        else if (left >= 7) c = Mix(col::warn, col::success, (left - 7) / 3.0f);
        else c = Mix(col::danger, col::warn, std::max(0.0f, (left - 1) / 6.0f));
        const bool urgent = left >= 0 && left <= 2;
        float pulse = urgent ? 0.5f + 0.5f * std::sin(t * (left == 0 ? 5.0f : 3.0f)) : 0;

        bool hover = input && MouseIn(r);
        if (urgent) SoftGlow({r.x + r.width / 2, r.y + r.height / 2}, 190, c, 0.10f + 0.08f * pulse);
        Shadow(r, 16, 1.4f);
        RoundRect(r, 16, Alpha(col::panel, 0.92f));
        PanelTint(r, 16, c, 0.20f + 0.08f * pulse);
        RoundRectLines(r, 16, urgent ? 1.6f : 1, Alpha(c, urgent ? 0.5f + 0.4f * pulse : 0.35f));

        static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        static const char* days[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        Text("EXAM", r.x + 20, r.y + 18, 12.5f, c, F_HEAVY);
        std::string when = std::string(days[streak::Weekday(ey, em, ed)]) + " " + std::to_string(ed) + " " + months[(em - 1) % 12];
        Text(when, r.x + 20 + Measure("EXAM", 12.5f, F_HEAVY).x + 10, r.y + 18, 12.5f, col::dim, F_BOLD);

        // the number
        std::string big, small;
        if (left < 0) { big = "Done"; small = "change the date for the next one"; }
        else if (left == 0) { big = "Today"; small = "good luck, you've got this"; }
        else if (left == 1) { big = "1"; small = "day left - tomorrow!"; }
        else { big = std::to_string(left); small = "days left"; }
        float bs = 58;
        Text(big, r.x + 18, r.y + 40, bs, left < 0 ? col::dim : c, F_HEAVY);
        float bw = Measure(big, bs, F_HEAVY).x;
        if (left > 0) Text(small, r.x + 26 + bw, r.y + 78, 15, col::text, F_BOLD);
        else Text(small, r.x + 20, r.y + 112, 13.5f, col::dim, F_MEDIUM);

        // the last two weeks as cells: one empties each day
        if (left > 0) {
            const int cells = 14;
            float cw = (r.width - 40 - (cells - 1) * 4) / cells;
            for (int i = 0; i < cells; i++) {
                Rectangle cell = {r.x + 20 + i * (cw + 4), r.y + r.height - 46, cw, 10};
                bool on = i < std::min<long>(left, cells);
                RoundRect(cell, 3, on ? Alpha(c, 0.85f) : Alpha(col::heading, 0.08f));
            }
            Text(left > 14 ? "more than two weeks" : "the last two weeks", r.x + 20, r.y + r.height - 28, 11.5f, col::faint, F_MEDIUM);
        }

        // change the date: < > while the mouse is over the card
        if (hover) {
            Rectangle minus = {r.x + r.width - 74, r.y + 12, 26, 26}, plus = {r.x + r.width - 42, r.y + 12, 26, 26};
            long d = 0;
            for (int k = 0; k < 2; k++) {
                Rectangle b = k ? plus : minus;
                bool h = MouseIn(b);
                RoundRect(b, 7, h ? col::panelHi : Alpha(col::panelHi, 0.6f));
                Vector2 m = {b.x + 13, b.y + 13};
                float s = k ? 1.0f : -1.0f;
                DrawLineEx({m.x - 2 * s, m.y - 5}, {m.x + 3 * s, m.y}, 2, col::heading);
                DrawLineEx({m.x + 3 * s, m.y}, {m.x - 2 * s, m.y + 5}, 2, col::heading);
                if (h) WantHand();
                if (h && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) d = k ? 1 : -1;
            }
            TextRight("move the date", r.x + r.width - 18, r.y + r.height - 28, 11.5f, col::faint, F_MEDIUM);
            if (d != 0) {
                CivilFromDays(exam + d, ey, em, ed);
                app.examDate = streak::Format(ey, em, ed);
                SaveAppSettings();
            }
        }
    }

    bool TaskButton(Rectangle r, bool input, bool active) {
        bool hover = input && MouseIn(r);
        if (hover || active) RoundRect(r, 8, Alpha(col::heading, active ? 0.14f : 0.08f));
        if (hover) WantHand();
        return input && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

    void Clock(Rectangle bar) {
        std::time_t now = std::time(nullptr);
        std::tm tm = *std::localtime(&now);
        char hm[8], date[16];
        std::snprintf(hm, sizeof(hm), "%02d:%02d", tm.tm_hour, tm.tm_min);
        std::snprintf(date, sizeof(date), "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        TextRight(hm, W + ExtraX() - 18, bar.y + 9, 14, col::heading, F_BOLD);
        TextRight(date, W + ExtraX() - 18, bar.y + 29, 12, col::dim, F_MEDIUM);
    }

    void StartMenu(Rectangle start, bool input) {
        const float h = 510;
        Rectangle m = {W / 2 - 180, H - 54 - 12 - h, 360, h};
        Shadow(m, 14, 2);
        RoundRect(m, 14, Mix(col::panel, col::bg2, 0.3f));
        RoundRectLines(m, 14, 1, col::border);
        Text("Pinned", m.x + 20, m.y + 18, 13, col::dim, F_BOLD);

        // the apps
        for (int i = 0; i < APP_COUNT; i++) {
            Rectangle item = {m.x + 12, m.y + 42 + i * 62.0f, m.width - 24, 58};
            bool hover = input && MouseIn(item);
            if (hover) { RoundRect(item, 10, Alpha(col::heading, 0.08f)); WantHand(); }
            DrawAppIcon(i, {item.x + 10, item.y + 9, 40, 40}, t, hover ? 0.6f : 0);
            Text(kAppName[i], item.x + 62, item.y + 11, 15.5f, col::heading, F_BOLD);
            Text(kAppLine[i], item.x + 62, item.y + 32, 12.5f, col::dim, F_MEDIUM);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Open(i, start);
        }

        // open straight away next time
        Rectangle opt = {m.x + 12, m.y + 298, m.width - 24, 50};
        bool oh = input && MouseIn(opt);
        if (oh) { RoundRect(opt, 10, Alpha(col::heading, 0.06f)); WantHand(); }
        Rectangle box = {opt.x + 12, opt.y + 15, 20, 20};
        RoundRect(box, 5, app.bootToApp ? col::accent : Alpha(col::panelHi, 1));
        RoundRectLines(box, 5, 1, app.bootToApp ? col::accent : col::border);
        if (app.bootToApp) {
            Color on = OnColor(col::accent);
            DrawLineEx({box.x + 5, box.y + 10}, {box.x + 9, box.y + 14}, 2, on);
            DrawLineEx({box.x + 9, box.y + 14}, {box.x + 15, box.y + 6}, 2, on);
        }
        Text("Open SegFault at start-up", opt.x + 44, opt.y + 8, 14, col::text, F_MEDIUM);
        Text("skips this desktop", opt.x + 44, opt.y + 27, 12, col::faint, F_MEDIUM);
        if (oh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.bootToApp = !app.bootToApp; SaveAppSettings(); }

        // full screen, like a real operating system (F11 does the same)
        Rectangle fs = {m.x + 12, m.y + 354, m.width - 24, 50};
        bool fsh = input && MouseIn(fs);
        if (fsh) { RoundRect(fs, 10, Alpha(col::heading, 0.06f)); WantHand(); }
        Rectangle fb = {fs.x + 12, fs.y + 15, 20, 20};
        RoundRect(fb, 5, app.fullscreen ? col::accent : Alpha(col::panelHi, 1));
        RoundRectLines(fb, 5, 1, app.fullscreen ? col::accent : col::border);
        if (app.fullscreen) {
            Color on = OnColor(col::accent);
            DrawLineEx({fb.x + 5, fb.y + 10}, {fb.x + 9, fb.y + 14}, 2, on);
            DrawLineEx({fb.x + 9, fb.y + 14}, {fb.x + 15, fb.y + 6}, 2, on);
        }
        Text("Full screen", fs.x + 44, fs.y + 8, 14, col::text, F_MEDIUM);
        Text("covers the Windows taskbar  -  F11", fs.x + 44, fs.y + 27, 12, col::faint, F_MEDIUM);
        if (fsh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { ToggleBorderlessWindowed(); app.fullscreen = !app.fullscreen; SaveAppSettings(); }

        // footer: shut down
        DrawRectangleRec({m.x, m.y + m.height - 62, m.width, 1}, col::border);
        Rectangle off = {m.x + m.width - 140, m.y + m.height - 50, 128, 38};
        bool fh = input && MouseIn(off);
        if (fh) { RoundRect(off, 9, Alpha(col::danger, 0.15f)); WantHand(); }
        Vector2 c = {off.x + 20, off.y + 19};
        DrawRing(c, 6.5f, 8.5f, -50, 230, 24, fh ? col::danger : col::text);   // power symbol: the gap at the top (0 degrees is to the right)
        DrawLineEx({c.x, c.y - 10}, {c.x, c.y - 2}, 2, fh ? col::danger : col::text);
        Text("Shut down", off.x + 36, off.y + 11, 14, fh ? col::danger : col::text, F_BOLD);
        if (fh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { shutting = t; menu = false; }
        std::string who = app.lastUser.empty() ? "SegFault OS" : app.lastUser;
        Text(who, m.x + 20, m.y + m.height - 40, 14, col::dim, F_MEDIUM);

        // a click anywhere else closes it
        if (input && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !MouseIn(m) && !MouseIn(start)) menu = false;
    }
};

} // namespace

Scene* CreateDesktopScene() { return new DesktopScene(); }
