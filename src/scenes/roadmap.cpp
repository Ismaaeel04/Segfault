// ============================================================================
// roadmap.cpp  -  The course as a map: the nine sections in the order they
// build on each other, with the two mock exam papers as milestones. Each node
// shows how far you are; the side panel shows problems by difficulty and the
// days you practised. Click a section to study it, a paper to take it.
// ============================================================================
#include "app/app.h"
#include "app/streak.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

using namespace ui;

namespace {

struct Node {
    int section;          // S_..., or -1 / -2 for mock paper 1 / 2
    Vector2 c;            // centre
    std::vector<int> from;   // indexes of the nodes it builds on
};

// top to bottom: numbers -> loops -> arrays -> (2D arrays, strings, pointers)
// -> pointer arithmetic -> memory -> linked lists; papers after their topics
const float kX = 474;
std::vector<Node> Nodes() {
    return {
        {S_NUMBERS,  {kX, 122}, {}},
        {S_LOOPS,    {kX, 205}, {0}},
        {S_ARRAYS,   {kX, 288}, {1}},
        {S_GRID,     {kX - 250, 380}, {2}},
        {S_STRINGS,  {kX, 380}, {2}},
        {S_POINTERS, {kX + 250, 380}, {2}},
        {-1,         {kX - 250, 482}, {3, 4}},     // mock paper 1
        {S_PTRARITH, {kX + 125, 482}, {4, 5}},
        {S_MEMORY,   {kX + 125, 574}, {7}},
        {S_LISTS,    {kX + 125, 666}, {8}},
        {-2,         {kX - 125, 740}, {9}},        // mock paper 2
    };
}

std::string Today(int* y = nullptr, int* m = nullptr, int* d = nullptr) {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    if (y) *y = tm.tm_year + 1900;
    if (m) *m = tm.tm_mon + 1;
    if (d) *d = tm.tm_mday;
    return streak::Format(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

class RoadmapScene : public Scene {
public:
    const char* Title() const override { return "Roadmap"; }
    void Enter(const std::string&) override { t = 0; }

    void Frame(float dt) override {
        t += dt;
        const std::vector<Node> nodes = Nodes();
        std::vector<float> done(nodes.size());
        for (size_t i = 0; i < nodes.size(); i++) done[i] = Mastery(nodes[i]);
        // next up: the first unfinished node whose earlier nodes are all started
        int next = -1;
        for (size_t i = 0; i < nodes.size() && next < 0; i++) {
            if (done[i] >= 0.999f) continue;
            bool ready = true;
            for (int f : nodes[i].from) ready &= done[f] > 0;
            if (ready || nodes[i].from.empty()) next = (int)i;
        }

        Rectangle area = {24, 80, 904, 704};
        Panel(area, col::panel, 18);
        DotGrid({area.x + 10, area.y + 10, area.width - 20, area.height - 20}, 24, Alpha(col::border, 0.6f));
        Text("Your roadmap", area.x + 24, area.y + 20, 20, col::heading, F_HEAVY);
        Text("Each topic builds on the ones above it.", area.x + 24, area.y + 46, 13.5f, col::dim, F_MEDIUM);
        Legend({area.x + area.width - 300, area.y + 26});

        // links first, under the nodes
        for (size_t i = 0; i < nodes.size(); i++)
            for (int f : nodes[i].from) {
                Vector2 a = {nodes[f].c.x, nodes[f].c.y + kH / 2}, b = {nodes[i].c.x, nodes[i].c.y - kH / 2};
                float my = (a.y + b.y) / 2;
                bool lit = done[f] >= 0.999f;
                Color c = lit ? Alpha(NodeColor(nodes[f]), 0.9f) : Alpha(col::border, 1);
                DrawSplineSegmentBezierCubic(a, {a.x, my}, {b.x, my}, b, lit ? 3.0f : 2.0f, c);
            }
        int hovered = -1;
        for (size_t i = 0; i < nodes.size(); i++)
            if (DrawNode(nodes[i], done[i], (int)i == next)) hovered = (int)i;
        if (hovered >= 0) Tooltip(nodes[hovered]);

        float x = 944, w = W - 24 - x;
        Stats({x, 80, w, 268});
        Calendar({x, 360, w, 424});
    }

private:
    static constexpr float kW = 196, kH = 58;
    float t = 0;

    static Color NodeColor(const Node& n) { return n.section >= 0 ? SectionColor(n.section) : col::warn; }

    static float Mastery(const Node& n) {
        if (n.section >= 0) return SectionMastery(n.section);
        int best = app.progress.Best("mock:" + std::to_string(-n.section));
        return best < 0 ? 0.0f : best / 100.0f;
    }

    void Legend(Vector2 p) {
        auto item = [&](float x, Color c, const char* label, bool ring) {
            if (ring) DrawRing({x + 6, p.y + 7}, 4, 6.5f, 0, 360, 24, c); else DrawCircleV({x + 6, p.y + 7}, 5, c);
            Text(label, x + 16, p.y, 12.5f, col::dim, F_MEDIUM);
        };
        item(p.x, col::faint, "not started", true);
        item(p.x + 96, col::accent2, "in progress", false);
        item(p.x + 196, col::success, "done", false);
    }

    // returns true while hovered
    bool DrawNode(const Node& n, float m, bool isNext) {
        Rectangle r = {n.c.x - kW / 2, n.c.y - kH / 2, kW, kH};
        bool hover = InputEnabled() && MouseIn(r);
        Color c = NodeColor(n);
        if (isNext) {   // breathing ring around the next topic
            float p = 0.5f + 0.5f * std::sin(t * 3);
            RoundRectLines({r.x - 5, r.y - 5, r.width + 10, r.height + 10}, 16, 2, Alpha(c, 0.35f + 0.4f * p));
        }
        if (hover) { r.y -= 2; Shadow(r, 12, 1.4f); }
        RoundRect(r, 12, hover ? col::panelHi : Mix(col::panel, col::panelHi, 0.55f));
        RoundRectLines(r, 12, hover ? 1.5f : 1, hover ? c : col::border);
        RoundRect({r.x, r.y + 10, 3.5f, r.height - 20}, 1.75f, c);

        std::string name = n.section >= 0 ? SectionName(n.section) : n.section == -1 ? "Mock exam: Paper 1" : "Mock exam: Paper 2";
        Text(Ellipsize(name, 14.5f, F_BOLD, kW - 52), r.x + 16, r.y + 11, 14.5f, col::heading, F_BOLD);
        // the bar, as on the NeetCode map, but with the percentage beside it
        Rectangle bar = {r.x + 16, r.y + 38, kW - 70, 5};
        ProgressBar(bar, m, c);
        std::string pct = n.section >= 0 ? std::to_string((int)std::lround(m * 100)) + "%"
                        : m > 0 ? std::to_string((int)std::lround(m * 100)) + "%" : "-";
        TextRight(pct, r.x + r.width - 14, r.y + 32, 12.5f, col::dim, F_BOLD);
        // state in the corner
        Vector2 dot = {r.x + r.width - 18, r.y + 18};
        if (m >= 0.999f) {
            DrawCircleV(dot, 8, col::success);
            Color on = OnColor(col::success);
            DrawLineEx({dot.x - 3.5f, dot.y}, {dot.x - 1, dot.y + 3}, 2, on);
            DrawLineEx({dot.x - 1, dot.y + 3}, {dot.x + 4, dot.y - 3}, 2, on);
        } else if (m > 0) {
            DrawCircleV(dot, 5, col::accent2);
        } else {
            DrawRing(dot, 4, 6.5f, 0, 360, 24, col::faint);
        }
        if (isNext) Badge(r.x + r.width - 56, r.y - 12, "NEXT", c, 11);

        if (hover) WantHand();
        if (Clicked(r)) {
            if (n.section >= 0) app.Go(SceneId::Study, "section:" + std::to_string(n.section));
            else app.Go(SceneId::Mock, "");
        }
        return hover;
    }

    void Tooltip(const Node& n) {
        std::vector<std::string> lines;
        if (n.section >= 0) {
            int lt = 0, ld = 0, pt = 0, pd = 0;
            for (const Lesson& l : Lessons()) if (l.section == n.section) { lt++; ld += app.progress.Has("lesson:" + l.id); }
            for (const Problem& p : Problems()) if (p.section == n.section) { pt++; pd += app.progress.Has("solved:" + p.id); }
            lines = {std::to_string(ld) + " / " + std::to_string(lt) + " lessons done",
                     std::to_string(pd) + " / " + std::to_string(pt) + " problems solved",
                     SectionShort(n.section), "Click to study it"};
        } else {
            int best = app.progress.Best("mock:" + std::to_string(-n.section));
            lines = {best < 0 ? std::string("Not taken yet") : "Best score: " + std::to_string(best) + "%",
                     n.section == -1 ? "Numbers, loops, arrays, 2D arrays, strings" : "Pointers, memory, linked lists",
                     "Click to take a mock exam"};
        }
        float w = 0;
        for (const std::string& l : lines) w = std::max(w, Measure(l, 13.5f, F_MEDIUM).x);
        w += 28;
        float h = lines.size() * 20 + 20;
        Rectangle r = {n.c.x - w / 2, n.c.y + kH / 2 + 10, w, h};
        if (r.y + h > H - 20) r.y = n.c.y - kH / 2 - 10 - h;
        r.x = std::clamp(r.x, 36.0f, 916 - w);
        Shadow(r, 10, 1.5f);
        RoundRect(r, 10, col::panelHi);
        RoundRectLines(r, 10, 1, NodeColor(n));
        for (size_t i = 0; i < lines.size(); i++)
            Text(lines[i], r.x + 14, r.y + 10 + i * 20, 13.5f, i + 1 == lines.size() ? NodeColor(n) : col::text,
                 i + 1 == lines.size() ? F_BOLD : F_MEDIUM);
    }

    // ---- problems by difficulty, around a ring
    void Stats(Rectangle r) {
        Panel(r, col::panel, 18);
        int total[3] = {0, 0, 0}, solved[3] = {0, 0, 0};
        for (const Problem& p : Problems()) {
            int d = std::clamp(p.difficulty, 0, 2);
            total[d]++;
            solved[d] += app.progress.Has("solved:" + p.id);
        }
        int all = total[0] + total[1] + total[2], got = solved[0] + solved[1] + solved[2];

        Vector2 c = {r.x + r.width - 86, r.y + 96};
        const float start = 135, sweep = 270;   // a ring with a gap at the bottom
        DrawRing(c, 52, 60, start, start + sweep, 64, col::panelHi);
        float a = start;
        for (int d = 0; d < 3; d++) {
            if (!solved[d] || !all) continue;
            float s = sweep * solved[d] / all;
            DrawRing(c, 52, 60, a, a + s, 32, DifficultyColor(d));
            a += s;
        }
        TextCenter(std::to_string(got), {c.x - 40, c.y - 24, 80, 34}, 30, col::heading, F_DISPLAY);
        TextCenter("/ " + std::to_string(all) + " solved", {c.x - 50, c.y + 10, 100, 18}, 12.5f, col::dim, F_MEDIUM);

        const char* names[3] = {"Easy", "Medium", "Hard"};
        float y = r.y + 34;
        for (int d = 0; d < 3; d++) {
            Text(names[d], r.x + 22, y, 14, DifficultyColor(d), F_BOLD);
            TextRight(std::to_string(solved[d]) + "/" + std::to_string(total[d]), r.x + 150, y, 13.5f, col::text, F_MONO);
            ProgressBar({r.x + 22, y + 22, 128, 4}, total[d] ? (float)solved[d] / total[d] : 0, DifficultyColor(d));
            y += 44;
        }
        int lessons = 0;
        for (const Lesson& l : Lessons()) lessons += app.progress.Has("lesson:" + l.id);
        const Progress& p = app.progress;
        DrawRectangleRec({r.x + 20, r.y + 184, r.width - 40, 1}, col::border);
        auto stat = [&](float x, const std::string& v, const char* label) {
            Text(v, x, r.y + 198, 20, col::heading, F_DISPLAY);
            Text(label, x, r.y + 226, 12.5f, col::dim, F_MEDIUM);
        };
        stat(r.x + 22, std::to_string(lessons) + "/" + std::to_string(Lessons().size()), "lessons");
        stat(r.x + 118, p.quizAnswered ? std::to_string((int)std::lround(100.0 * p.quizCorrect / p.quizAnswered)) + "%" : "-", "quiz");
        stat(r.x + 196, "Lv " + std::to_string(p.Level()), std::to_string(p.xp).append(" XP").c_str());
    }

    // ---- the month, with the days you earned XP
    void Calendar(Rectangle r) {
        Panel(r, col::panel, 18);
        int y, m, d;
        std::string today = Today(&y, &m, &d);
        static const char* months[] = {"January", "February", "March", "April", "May", "June", "July",
                                       "August", "September", "October", "November", "December"};
        Text(std::string(months[m - 1]) + " " + std::to_string(y), r.x + 22, r.y + 20, 17, col::heading, F_HEAVY);
        int active = 0;
        for (int k = 1; k <= streak::DaysInMonth(y, m); k++) active += app.progress.days.count(streak::Format(y, m, k));
        TextRight(std::to_string(active) + " active days", r.x + r.width - 22, r.y + 23, 13, col::dim, F_MEDIUM);

        const char* wd[] = {"M", "T", "W", "T", "F", "S", "S"};
        float cw = (r.width - 44) / 7, gx = r.x + 22, gy = r.y + 56;
        for (int k = 0; k < 7; k++) TextCenter(wd[k], {gx + k * cw, gy, cw, 16}, 12, col::faint, F_BOLD);
        gy += 24;
        int first = streak::Weekday(y, m, 1);
        for (int k = 1; k <= streak::DaysInMonth(y, m); k++) {
            int cell = first + k - 1;
            Vector2 c = {gx + (cell % 7) * cw + cw / 2, gy + (cell / 7) * 38 + 14};
            bool on = app.progress.days.count(streak::Format(y, m, k)) > 0;
            if (on) DrawCircleV(c, 14, Alpha(col::success, 0.85f));
            if (k == d) DrawRing(c, 15, 17.5f, 0, 360, 36, col::accent);
            TextCenter(std::to_string(k), {c.x - 14, c.y - 9, 28, 18}, 12.5f,
                       on ? OnColor(col::success) : k > d ? col::faint : col::text, on || k == d ? F_BOLD : F_MEDIUM);
        }

        // streaks
        float sy = r.y + r.height - 94;
        DrawRectangleRec({r.x + 20, sy - 12, r.width - 40, 1}, col::border);
        int cur = streak::Current(app.progress.days, today), best = streak::Best(app.progress.days);
        auto box = [&](float x, const char* label, int value, Color c) {
            Rectangle b = {x, sy, (r.width - 56) / 2, 62};
            RoundRect(b, 12, Mix(col::panel, col::panelHi, 0.6f));
            Text(label, b.x + 14, b.y + 10, 12.5f, col::dim, F_MEDIUM);
            Text(std::to_string(value) + (value == 1 ? " day" : " days"), b.x + 14, b.y + 30, 19, c, F_HEAVY);
        };
        box(r.x + 22, "Current streak", cur, cur ? col::warn : col::text);
        box(r.x + 34 + (r.width - 56) / 2, "Best streak", best, col::accent2);
        TextCenter(app.progress.days.count(today) ? "You practised today. Nice." : "Earn some XP today to keep your streak.",
                   {r.x, r.y + r.height - 24, r.width, 16}, 12, col::faint, F_MEDIUM);
    }
};

} // namespace

Scene* CreateRoadmapScene() { return new RoadmapScene(); }
