// ============================================================================
// study.cpp  -  Study: lessons organised by section, read like an article.
// Blocks: text, code, step-through walkthroughs, tables, tips, common
// mistakes, quick checks, interactive demos and links to practice problems.
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>

using namespace ui;

namespace {

class StudyScene : public Scene {
public:
    const char* Title() const override { return "Study"; }

    void Enter(const std::string& param) override {
        const auto& ls = Lessons();
        int idx = -1;
        for (size_t i = 0; i < ls.size(); i++) if (ls[i].id == param) idx = (int)i;
        if (idx < 0 && !param.empty() && param.rfind("section:", 0) == 0) {
            int s = std::atoi(param.c_str() + 8);
            for (size_t i = 0; i < ls.size(); i++) if (ls[i].section == s) { idx = (int)i; break; }
        }
        if (idx < 0 && lesson < 0) {
            idx = 0;
            for (size_t i = 0; i < ls.size(); i++) if (!app.progress.Has("lesson:" + ls[i].id)) { idx = (int)i; break; }
        }
        if (idx >= 0 && idx != lesson) Select(idx);
        if (lesson < 0) Select(0);
    }

    void Frame(float) override {
        Sidebar();
        Reader();
    }

private:
    int lesson = -1;
    std::map<int, int> walkStep;       // block index -> current step
    std::map<int, int> checkAnswer;    // block index -> chosen option (original index)
    std::map<int, Scroll> blockScroll;
    std::set<int> expanded;
    Scroll sideScroll, readScroll;
    float contentH = 1000;

    void Select(int i) {
        lesson = i;
        walkStep.clear(); checkAnswer.clear(); blockScroll.clear();
        readScroll.Reset();
        expanded.insert(Lessons()[i].section);
    }

    // ------------------------------------------------------------ sidebar --
    void Sidebar() {
        Rectangle r = {24, 76, 300, 708};
        Panel(r, col::panel, 14);
        Rectangle view = {r.x + 8, r.y + 8, r.width - 16, r.height - 16};
        const auto& ls = Lessons();
        // measure
        float h = 8;
        for (int s = 0; s < S_COUNT; s++) {
            h += 58;
            if (expanded.count(s)) for (const Lesson& l : ls) if (l.section == s) h += 40;
        }
        sideScroll.Update(view, h);
        BeginClip(view);
        float y = view.y + 4 - sideScroll.y;
        bool inView = MouseIn(view);
        for (int s = 0; s < S_COUNT; s++) {
            int total = 0, done = 0;
            for (const Lesson& l : ls) if (l.section == s) { total++; done += app.progress.Has("lesson:" + l.id); }
            Rectangle hdr = {view.x, y, view.width - 6, 52};
            bool open = expanded.count(s) > 0;
            bool hover = inView && InputEnabled() && MouseIn(hdr);
            RoundRect(hdr, 10, hover ? col::panelHi : Color{0, 0, 0, 0});
            Color sc = SectionColor(s);
            Rectangle num = {hdr.x + 10, hdr.y + 11, 30, 30};
            RoundRect(num, 8, done == total && total ? sc : Alpha(sc, 0.16f));
            char nb[4]; snprintf(nb, sizeof(nb), "%02d", s + 1);
            TextCenter(nb, num, 13, done == total && total ? col::bg : sc, F_HEAVY);
            Text(Ellipsize(SectionName(s), 15.5f, F_BOLD, hdr.width - 110), hdr.x + 52, hdr.y + 9, 15.5f, WHITE, F_BOLD);
            Text(std::to_string(done) + " of " + std::to_string(total) + " lessons", hdr.x + 52, hdr.y + 30, 12.5f, col::dim, F_MEDIUM);
            // chevron
            Vector2 cv = {hdr.x + hdr.width - 20, hdr.y + 26};
            if (open) { DrawLineEx({cv.x - 5, cv.y - 3}, {cv.x, cv.y + 3}, 2, col::dim); DrawLineEx({cv.x, cv.y + 3}, {cv.x + 5, cv.y - 3}, 2, col::dim); }
            else { DrawLineEx({cv.x - 3, cv.y - 5}, {cv.x + 3, cv.y}, 2, col::dim); DrawLineEx({cv.x + 3, cv.y}, {cv.x - 3, cv.y + 5}, 2, col::dim); }
            if (hover && Clicked(hdr)) { if (open) expanded.erase(s); else expanded.insert(s); }
            y += 58;
            if (!open) continue;
            for (size_t i = 0; i < ls.size(); i++) {
                if (ls[i].section != s) continue;
                Rectangle row = {view.x + 20, y, view.width - 26, 36};
                bool sel = (int)i == lesson;
                bool hv = inView && InputEnabled() && MouseIn(row);
                RoundRect(row, 8, sel ? Alpha(sc, 0.18f) : hv ? col::panelHi : Color{0, 0, 0, 0});
                if (sel) DrawRectangleRec({row.x, row.y + 8, 3, row.height - 16}, sc);
                bool d = app.progress.Has("lesson:" + ls[i].id);
                if (d) {
                    Vector2 c = {row.x + 18, row.y + 18};
                    DrawLineEx({c.x - 5, c.y}, {c.x - 1.5f, c.y + 4}, 2, col::success);
                    DrawLineEx({c.x - 1.5f, c.y + 4}, {c.x + 5, c.y - 4}, 2, col::success);
                } else DrawCircleV({row.x + 18, row.y + 18}, 3, sel ? sc : col::faint);
                Text(Ellipsize(ls[i].title, 14.5f, F_MEDIUM, row.width - 44), row.x + 34, row.y + 9, 14.5f, sel ? WHITE : col::text, sel ? F_BOLD : F_MEDIUM);
                if (hv && Clicked(row)) Select((int)i);
                y += 40;
            }
        }
        EndClip();
        ScrollBar(view, h, sideScroll);
    }

    // ------------------------------------------------------------- reader --
    void Reader() {
        const auto& ls = Lessons();
        const Lesson& L = ls[lesson];
        Rectangle panel = {336, 76, 920, 708};
        Panel(panel, col::panel, 14);
        Rectangle view = {panel.x + 4, panel.y + 4, panel.width - 8, panel.height - 8};
        readScroll.Update(view, contentH);
        bool inView = MouseIn(view);
        const float w = 744;
        const float x = view.x + (view.width - w) / 2;
        BeginClip(view);
        float y = view.y + 34 - readScroll.y;
        Color sc = SectionColor(L.section);
        // lesson position inside the section
        int pos = 0, count = 0;
        for (size_t i = 0; i < ls.size(); i++) if (ls[i].section == L.section) { count++; if ((int)i <= lesson) pos++; }
        char head[96];
        snprintf(head, sizeof(head), "SECTION %d  -  %s", L.section + 1, SectionName(L.section));
        std::string hs = head;
        for (auto& ch : hs) ch = (char)toupper((unsigned char)ch);
        Text(hs, x, y, 13, sc, F_HEAVY);
        TextRight("Lesson " + std::to_string(pos) + " of " + std::to_string(count), x + w, y, 13, col::dim, F_BOLD);
        y += 30;
        for (const std::string& line : Wrap(L.title, 32, F_DISPLAY, w)) { Text(line, x, y, 32, WHITE, F_DISPLAY); y += 42; }
        y += 2;
        y += RichText({x, y, w, 0}, L.summary, 18, col::dim) + 18;
        DrawRectangleRec({x, y, w, 1}, col::border);
        y += 28;

        for (size_t i = 0; i < L.blocks.size(); i++) y += DrawBlock((int)i, L.blocks[i], x, y, w, inView) + 26;

        // footer
        DrawRectangleRec({x, y, w, 1}, col::border);
        y += 24;
        bool done = app.progress.Has("lesson:" + L.id);
        bool hasNext = lesson + 1 < (int)ls.size();
        if (!done) {
            if (Button({x, y, 230, 48}, "Mark as complete", BTN_SUCCESS) && inView) {
                if (app.progress.Mark("lesson:" + L.id)) app.progress.AddXP(15, "lesson complete");
            }
        } else {
            Rectangle ok = {x, y, 230, 48};
            RoundRect(ok, 10, Alpha(col::success, 0.12f));
            TextCenter("Completed", ok, 16, col::success, F_BOLD);
        }
        if (hasNext) {
            std::string label = "Next: " + ls[lesson + 1].title + "  >";
            float bw = std::min(w - 246, Measure(label, 15, F_BOLD).x + 40);
            if (Button({x + w - bw, y, bw, 48}, Ellipsize(label, 15, F_BOLD, bw - 30), BTN_PRIMARY, true, 15) && inView) {
                if (app.progress.Mark("lesson:" + L.id)) app.progress.AddXP(15, "lesson complete");
                Select(lesson + 1);
                EndClip();
                return;
            }
        }
        y += 48 + 50;
        contentH = y + readScroll.y - view.y;
        EndClip();
        ScrollBar(view, contentH, readScroll);
    }

    float Callout(const std::string& label, const std::string& text, Color c, float x, float y, float w) {
        float th = RichText({0, 0, w - 52, 0}, text, 16.5f, col::text, false);
        float h = th + 58;
        RoundRect({x, y, w, h}, 12, Alpha(c, 0.09f));
        RoundRectLines({x, y, w, h}, 12, 1, Alpha(c, 0.35f));
        DrawRectangleRec({x, y + 12, 4, h - 24}, c);
        Text(label, x + 26, y + 17, 12.5f, c, F_HEAVY);
        RichText({x + 26, y + 40, w - 52, 0}, text, 16.5f, col::text);
        return h;
    }

    float DrawBlock(int bi, const Block& b, float x, float y, float w, bool inView) {
        switch (b.type) {
        case BlockType::Text:
            return RichText({x, y, w, 0}, b.text, 17.5f, col::text);
        case BlockType::Code: {
            auto lines = SplitLines(b.text);
            float h = lines.size() * CodeLineHeight(15.5f) + 24;
            CodeView({x, y, w, h}, lines, blockScroll[bi], -1, 15.5f);
            return h;
        }
        case BlockType::Walk: {
            auto lines = SplitLines(b.text);
            int& st = walkStep[bi];
            st = std::clamp(st, 0, (int)b.steps.size() - 1);
            const WalkStep& s = b.steps[st];
            Text("STEP THROUGH IT", x, y, 12.5f, col::accent, F_HEAVY);
            TextRight("step " + std::to_string(st + 1) + " of " + std::to_string(b.steps.size()), x + w, y, 13, col::dim, F_BOLD);
            float yy = y + 24;
            float h = lines.size() * CodeLineHeight(15.5f) + 24;
            CodeView({x, yy, w, h}, lines, blockScroll[bi], s.line, 15.5f);
            yy += h - 6;
            float th = RichText({0, 0, w - 220, 0}, s.text, 16.5f, col::text, false);
            float ch = std::max(76.0f, th + 34);
            Rectangle card = {x, yy, w, ch};
            RoundRect(card, 12, col::panelHi);
            RoundRectLines(card, 12, 1, Alpha(col::accent, 0.5f));
            RichText({x + 20, yy + 17, w - 220, 0}, s.text, 16.5f, col::text);
            float by = yy + ch / 2 - 20;
            if (Button({x + w - 192, by, 84, 40}, "<", BTN_GHOST, st > 0, 16) && inView) st--;
            bool last = st + 1 >= (int)b.steps.size();
            if (Button({x + w - 100, by, 84, 40}, last ? "Again" : "Next", last ? BTN_DEFAULT : BTN_PRIMARY, true, 15) && inView)
                st = last ? 0 : st + 1;
            // progress dots
            float dx = x + 2;
            for (size_t k = 0; k < b.steps.size(); k++) {
                DrawCircleV({dx + k * 14.0f, yy + ch + 14}, 3.5f, (int)k <= st ? col::accent : col::border);
            }
            return (yy + ch + 24) - y;
        }
        case BlockType::Table: {
            auto rows = SplitLines(b.text);
            std::vector<std::vector<std::string>> cells;
            size_t cols = 0;
            for (auto& r : rows) {
                std::vector<std::string> c;
                size_t start = 0;
                while (true) {
                    size_t e = r.find('|', start);
                    c.push_back(r.substr(start, e == std::string::npos ? std::string::npos : e - start));
                    if (e == std::string::npos) break;
                    start = e + 1;
                }
                cols = std::max(cols, c.size());
                cells.push_back(c);
            }
            std::vector<float> cw(cols, 40);
            for (size_t r = 0; r < cells.size(); r++)
                for (size_t c = 0; c < cells[r].size(); c++)
                    cw[c] = std::max(cw[c], Measure(cells[r][c], 15, r == 0 ? F_BOLD : F_MONO).x + 36);
            float total = 0;
            for (float v : cw) total += v;
            for (float& v : cw) v *= w / total;
            const float rh = 40;
            float h = rh * cells.size();
            RoundRect({x, y, w, h}, 12, col::bg2);
            RoundRectLines({x, y, w, h}, 12, 1, col::border);
            for (size_t r = 0; r < cells.size(); r++) {
                float ry = y + r * rh;
                if (r == 0) RoundRect({x, ry, w, rh}, 12, col::panelHi);
                if (r > 0) DrawRectangleRec({x + 1, ry, w - 2, 1}, Alpha(col::border, 0.7f));
                float cx = x;
                for (size_t c = 0; c < cells[r].size(); c++) {
                    Text(Ellipsize(cells[r][c], 15, r == 0 ? F_BOLD : F_MONO, cw[c] - 24), cx + 18, ry + 11, 15,
                         r == 0 ? col::dim : col::text, r == 0 ? F_BOLD : F_MONO);
                    cx += cw[c];
                }
            }
            return h;
        }
        case BlockType::Tip:
            return Callout("TIP", b.text, col::accent2, x, y, w);
        case BlockType::Warn:
            return Callout("COMMON MISTAKE", b.text, col::danger, x, y, w);
        case BlockType::Demo: {
            Rectangle r = {x, y, w, 84};
            RoundRect(r, 12, Mix(col::panel, col::pointer, 0.08f));
            RoundRectLines(r, 12, 1, Alpha(col::pointer, 0.45f));
            Rectangle ic = {r.x + 20, r.y + 20, 44, 44};
            RoundRect(ic, 10, Alpha(col::pointer, 0.18f));
            // little play triangle
            DrawTriangle({ic.x + 17, ic.y + 13}, {ic.x + 17, ic.y + 31}, {ic.x + 31, ic.y + 22}, col::pointer);
            Text("INTERACTIVE DEMO", r.x + 80, r.y + 20, 12, col::pointer, F_HEAVY);
            Text(Ellipsize(b.text, 17, F_BOLD, w - 280), r.x + 80, r.y + 42, 17, WHITE, F_BOLD);
            if (Button({r.x + w - 170, r.y + 22, 150, 40}, "Open demo  >", BTN_DEFAULT, true, 15) && inView)
                app.Open(b.scene, b.param, Lessons()[lesson].id);
            return r.height;
        }
        case BlockType::Practice: {
            std::vector<std::string> ids;
            size_t start = 0;
            while (start <= b.text.size()) {
                size_t e = b.text.find(',', start);
                ids.push_back(b.text.substr(start, e == std::string::npos ? std::string::npos : e - start));
                if (e == std::string::npos) break;
                start = e + 1;
            }
            Text("PRACTISE IT - WRITE THE CODE YOURSELF", x, y, 12.5f, col::success, F_HEAVY);
            float yy = y + 26;
            for (const std::string& id : ids) {
                const Problem* p = ProblemById(id);
                if (!p) continue;
                Rectangle row = {x, yy, w, 52};
                bool hover = inView && InputEnabled() && MouseIn(row);
                RoundRect(row, 10, hover ? col::panelHi : col::bg2);
                RoundRectLines(row, 10, 1, hover ? col::success : col::border);
                bool solved = app.progress.Has("solved:" + p->id);
                if (solved) {
                    Vector2 c = {row.x + 24, row.y + 26};
                    DrawCircleV(c, 10, Alpha(col::success, 0.2f));
                    DrawLineEx({c.x - 5, c.y}, {c.x - 1.5f, c.y + 4}, 2.2f, col::success);
                    DrawLineEx({c.x - 1.5f, c.y + 4}, {c.x + 5, c.y - 4}, 2.2f, col::success);
                } else DrawRing({row.x + 24, row.y + 26}, 7, 9, 0, 360, 24, col::faint);
                Text(p->title, row.x + 48, row.y + 16, 16, WHITE, F_BOLD);
                Text(DifficultyName(p->difficulty), row.x + w - 160, row.y + 17, 14.5f, DifficultyColor(p->difficulty), F_BOLD);
                Text("Solve  >", row.x + w - 82, row.y + 17, 14.5f, hover ? col::success : col::dim, F_BOLD);
                if (hover && Clicked(row)) app.Open(SceneId::Practice, p->id, Lessons()[lesson].id);
                yy += 60;
            }
            return yy - y - 8;
        }
        case BlockType::Check: {
            int& ans = checkAnswer.emplace(bi, -1).first->second;
            float qh = RichText({0, 0, w - 48, 0}, b.text, 17, WHITE, false);
            // fixed shuffle per block
            std::vector<int> order(b.options.size());
            for (size_t k = 0; k < order.size(); k++) order[k] = (int)k;
            std::mt19937 rng((unsigned)(lesson * 97 + bi * 31 + 5));
            std::shuffle(order.begin(), order.end(), rng);
            float h = 52 + qh + 12 + order.size() * 50.0f;
            float eh = 0;
            if (ans >= 0) { eh = RichText({0, 0, w - 48, 0}, b.explain, 15.5f, col::dim, false) + 46; h += eh; }
            Rectangle card = {x, y, w, h + 10};
            RoundRect(card, 12, col::bg2);
            RoundRectLines(card, 12, 1, col::border);
            Text("QUICK CHECK", x + 24, y + 20, 12.5f, col::warn, F_HEAVY);
            RichText({x + 24, y + 44, w - 48, 0}, b.text, 17, WHITE);
            float oy = y + 52 + qh + 6;
            for (size_t k = 0; k < order.size(); k++) {
                int o = order[k];
                Rectangle ob = {x + 24, oy, w - 48, 42};
                bool hover = ans < 0 && inView && InputEnabled() && MouseIn(ob);
                Color fill = hover ? col::panelHi : col::panel, line = hover ? col::warn : col::border;
                if (ans >= 0 && o == 0) { fill = Alpha(col::success, 0.15f); line = col::success; }
                else if (ans >= 0 && o == ans) { fill = Alpha(col::danger, 0.15f); line = col::danger; }
                RoundRect(ob, 9, fill);
                RoundRectLines(ob, 9, 1, line);
                Text(std::string(1, (char)('A' + k)), ob.x + 14, ob.y + 11, 15, col::dim, F_HEAVY);
                const std::string& t = b.options[o];
                bool code = t.find_first_of(";{}*&[]=()") != std::string::npos;
                Text(Ellipsize(t, 15.5f, code ? F_MONO : F_MEDIUM, ob.width - 60), ob.x + 40, ob.y + 11, 15.5f, col::text, code ? F_MONO : F_MEDIUM);
                if (hover && Clicked(ob)) ans = o;
                oy += 50;
            }
            if (ans >= 0) {
                bool ok = ans == 0;
                Text(ok ? "Correct!" : "Not quite.", x + 24, oy + 6, 16, ok ? col::success : col::danger, F_HEAVY);
                RichText({x + 24, oy + 32, w - 48, 0}, b.explain, 15.5f, col::dim);
            }
            return card.height;
        }
        }
        return 0;
    }
};

} // namespace

Scene* CreateStudyScene() { return new StudyScene(); }
