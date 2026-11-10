// ============================================================================
// practice.cpp  -  LeetCode-style practice: a filterable problem list and the
// problem workspace (write code, run the tests, submit).
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include "workspace.h"
#include <algorithm>
#include <cmath>

using namespace ui;

namespace {

class PracticeScene : public Scene {
public:
    const char* Title() const override { return ws.problem && open ? ws.problem->title.c_str() : "Practice"; }

    void Enter(const std::string& param) override {
        const Problem* p = param.empty() ? nullptr : ProblemById(param);
        if (p) { ws.Open(p, "practice_" + p->id, false); open = true; }
        else { if (open) ws.SaveNow(); open = false; }
    }

    void Frame(float) override {
        if (open) { ws.Frame({24, 76, 1232, 708}); ws.TakeSolved(); return; }
        List();
    }

private:
    Workspace ws;
    bool open = false;
    int filterSection = -1, filterDiff = -1;
    Scroll listScroll;

    bool Chip(float& x, float y, const std::string& label, bool active, Color c) {
        float w = Measure(label, 14, F_BOLD).x + 28;
        Rectangle r = {x, y, w, 34};
        bool hover = InputEnabled() && MouseIn(r);
        RoundRect(r, 17, active ? Alpha(c, 0.22f) : hover ? col::panelHi : col::panel);
        RoundRectLines(r, 17, 1, active ? c : col::border);
        TextCenter(label, r, 14, active ? col::heading : col::dim, F_BOLD);
        x += w + 8;
        return Clicked(r);
    }

    void List() {
        const auto& ps = Problems();
        int solved = 0, perDiff[3] = {}, perDiffSolved[3] = {};
        for (const Problem& p : ps) {
            bool s = app.progress.Has("solved:" + p.id);
            solved += s; perDiff[p.difficulty]++; perDiffSolved[p.difficulty] += s;
        }
        // header
        Text("Practice", 32, 86, 30, col::heading, F_DISPLAY);
        Text("Write real C / C++ code and run it against tests.", 34, 126, 15.5f, col::dim, F_MEDIUM);

        // stats card
        Rectangle st = {876, 80, 380, 98};
        Panel(st, col::panel, 14);
        Vector2 c = {st.x + 52, st.y + 49};
        float t = ps.empty() ? 0 : (float)solved / ps.size();
        DrawRing(c, 30, 36, -90, 270, 48, col::panelHi);
        if (t > 0) DrawRing(c, 30, 36, -90, -90 + 360 * t, 48, col::success);
        TextCenter(std::to_string(solved), {c.x - 30, c.y - 16, 60, 22}, 20, col::heading, F_HEAVY);
        TextCenter("/" + std::to_string(ps.size()), {c.x - 30, c.y + 6, 60, 14}, 12, col::dim, F_MEDIUM);
        for (int d = 0; d < 3; d++) {
            float y = st.y + 16 + d * 24;
            Text(DifficultyName(d), st.x + 104, y, 14, DifficultyColor(d), F_BOLD);
            ProgressBar({st.x + 180, y + 6, 120, 6}, perDiff[d] ? (float)perDiffSolved[d] / perDiff[d] : 0, DifficultyColor(d));
            TextRight(std::to_string(perDiffSolved[d]) + "/" + std::to_string(perDiff[d]), st.x + st.width - 18, y, 14, col::text, F_BOLD);
        }

        // filters
        float x = 32, y = 192;
        if (Chip(x, y, "All topics", filterSection < 0, col::accent)) filterSection = -1;
        for (int s = 0; s < S_COUNT; s++) {
            if (x + Measure(SectionName(s), 14, F_BOLD).x + 28 > 1256) { x = 32; y += 42; }
            if (Chip(x, y, SectionName(s), filterSection == s, SectionColor(s))) filterSection = filterSection == s ? -1 : s;
        }
        y += 46;
        float dx = 32;
        Text("Difficulty", dx, y + 8, 14, col::dim, F_BOLD);
        dx += 82;
        if (Chip(dx, y, "Any", filterDiff < 0, col::accent)) filterDiff = -1;
        for (int d = 0; d < 3; d++) if (Chip(dx, y, DifficultyName(d), filterDiff == d, DifficultyColor(d))) filterDiff = filterDiff == d ? -1 : d;
        CompilerChip({1256 - 300, y, 300, 34});
        // free code, outside the problems
        if (Button({1256 - 300 - 12 - 154, y, 154, 34}, "SegFault Editor", BTN_DEFAULT, true, 13.5f))
            app.Open(SceneId::Editor, "", "");

        // table
        Rectangle tbl = {24, y + 48, 1232, 784 - (y + 48)};
        Panel(tbl, col::panel, 14);
        float hx = tbl.x + 24;
        float colTitle = hx + 46, colSection = tbl.x + 760, colDiff = tbl.x + 1010, colStatus = tbl.x + 1110;
        float hy = tbl.y + 14;
        Text("#", hx, hy, 13, col::faint, F_BOLD);
        Text("TITLE", colTitle, hy, 13, col::faint, F_BOLD);
        Text("TOPIC", colSection, hy, 13, col::faint, F_BOLD);
        Text("DIFFICULTY", colDiff, hy, 13, col::faint, F_BOLD);
        Text("STATUS", colStatus, hy, 13, col::faint, F_BOLD);
        DrawRectangleRec({tbl.x + 12, tbl.y + 40, tbl.width - 24, 1}, col::border);
        Rectangle view = {tbl.x + 8, tbl.y + 46, tbl.width - 16, tbl.height - 54};
        std::vector<int> rows;
        for (int i = 0; i < (int)ps.size(); i++) {
            if (filterSection >= 0 && ps[i].section != filterSection) continue;
            if (filterDiff >= 0 && ps[i].difficulty != filterDiff) continue;
            rows.push_back(i);
        }
        const float rh = 48;
        listScroll.Update(view, rows.size() * rh + 8);
        BeginClip(view);
        for (size_t k = 0; k < rows.size(); k++) {
            const Problem& p = ps[rows[k]];
            Rectangle row = {view.x, view.y + k * rh - listScroll.y, view.width - 10, rh - 4};
            if (row.y + row.height < view.y || row.y > view.y + view.height) continue;
            bool hover = InputEnabled() && MouseIn(row) && MouseIn(view);
            RoundRect(row, 10, hover ? col::panelHi : (k % 2 ? Alpha(col::bg2, 0.6f) : Color{0, 0, 0, 0}));
            float ty = row.y + (row.height - 16) / 2 - 1;
            Text(std::to_string(rows[k] + 1), hx, ty, 15, col::faint, F_MEDIUM);
            Text(p.title, colTitle, ty, 16, hover ? col::heading : col::text, F_BOLD);
            if (p.cppOnly) Badge(colTitle + Measure(p.title, 16, F_BOLD).x + 12, ty - 2, "C++", col::accent, 11.5f);
            DrawCircleV({colSection + 5, row.y + row.height / 2}, 4, SectionColor(p.section));
            Text(SectionName(p.section), colSection + 16, ty + 1, 14.5f, col::dim, F_MEDIUM);
            Text(DifficultyName(p.difficulty), colDiff, ty, 15, DifficultyColor(p.difficulty), F_BOLD);
            if (app.progress.Has("solved:" + p.id)) {
                Vector2 cc = {colStatus + 10, row.y + row.height / 2};
                DrawCircleV(cc, 10, Alpha(col::success, 0.2f));
                DrawLineEx({cc.x - 5, cc.y}, {cc.x - 1.5f, cc.y + 4}, 2.2f, col::success);
                DrawLineEx({cc.x - 1.5f, cc.y + 4}, {cc.x + 5, cc.y - 4}, 2.2f, col::success);
                Text("Solved", colStatus + 28, ty + 1, 14, col::success, F_BOLD);
            } else if (app.progress.Has("tried:" + p.id)) {
                DrawRing({colStatus + 10, row.y + row.height / 2}, 6, 8.5f, 0, 360, 24, col::warn);
                Text("Attempted", colStatus + 28, ty + 1, 14, col::warn, F_BOLD);
            }
            if (hover && Clicked(row)) app.Open(SceneId::Practice, p.id, "");
        }
        EndClip();
        ScrollBar(view, rows.size() * rh + 8, listScroll);
        if (rows.empty()) TextCenter("No problems match these filters.", view, 16, col::faint, F_MEDIUM);
    }
};

} // namespace

Scene* CreatePracticeScene() { return new PracticeScene(); }
