// ============================================================================
// mock.cpp  -  Mock coding exam. Like the real exam you WRITE CODE: a few
// problems, a timer, example tests you can run as often as you like, and
// hidden tests that grade everything when you submit.
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include "workspace.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <random>

using namespace ui;

namespace {

struct Paper {
    int id;
    const char* title;
    const char* blurb;
    const char* topics;
    int paper;            // 1, 2 or 0 = both
    int counts[3];        // easy, medium, hard
    int minutes;
};

const Paper kPapers[3] = {
    {1, "Paper 1: C fundamentals", "Like Coding Exam 1 (Lab1).", "Numbers, loops, arrays, 2D arrays, strings", 1, {1, 2, 1}, 75},
    {2, "Paper 2: Pointers & lists", "Like Coding Exam 2.", "Pointers, pointer arithmetic, memory, linked lists", 2, {1, 2, 1}, 75},
    {3, "Quick mock", "A short warm-up.", "Two problems from any section", 0, {1, 1, 0}, 25},
};

class MockScene : public Scene {
public:
    const char* Title() const override { return state == S_RUN ? "Mock Exam - in progress" : "Mock Exam"; }

    void Frame(float dt) override {
        (void)dt;
        if (confirm) SetInputEnabled(false);
        if (state == S_SETUP) Setup();
        else if (state == S_RUN) Running();
        else if (state == S_GRADING) Grading();
        else Results();
        if (confirm) ConfirmModal();
    }

private:
    enum State { S_SETUP, S_RUN, S_GRADING, S_RESULTS } state = S_SETUP;
    const Paper* paper = nullptr;
    std::vector<const Problem*> probs;
    std::vector<std::unique_ptr<Workspace>> spaces;
    int current = 0;
    double startTime = 0, endTime = 0;
    bool confirm = false;
    RunJob grader;
    int gradeIndex = 0;
    std::vector<RunReport> grades;
    std::vector<bool> untouched;      // code never changed from the starter: scores 0
    int reviewSel = 0, reviewTab = 0;
    Scroll reviewScroll, codeScroll;
    std::mt19937 rng{std::random_device{}()};

    float Remaining() const { return (float)(paper->minutes * 60 - (GetTime() - startTime)); }

    void Start(const Paper& pp) {
        paper = &pp;
        const Compiler* c = runner::Active();
        bool cpp = c && c->cpp;
        probs.clear();
        for (int d = 0; d < 3; d++) {
            std::vector<const Problem*> pool;
            for (const Problem& p : Problems()) {
                if (p.difficulty != d) continue;
                if (pp.paper && p.paper != pp.paper) continue;
                if (p.cppOnly && !cpp) continue;
                pool.push_back(&p);
            }
            std::shuffle(pool.begin(), pool.end(), rng);
            // prefer different sections
            int need = pp.counts[d];
            for (const Problem* p : pool) {
                if (need == 0) break;
                bool same = false;
                for (const Problem* q : probs) same |= q->section == p->section;
                if (!same) { probs.push_back(p); need--; }
            }
            for (const Problem* p : pool) {
                if (need == 0) break;
                if (std::find(probs.begin(), probs.end(), p) == probs.end()) { probs.push_back(p); need--; }
            }
        }
        spaces.clear();
        for (const Problem* p : probs) {
            spaces.push_back(std::make_unique<Workspace>());
            spaces.back()->Open(p, "", true);
        }
        current = 0;
        startTime = GetTime();
        state = S_RUN;
    }

    void Submit() {
        confirm = false;
        endTime = GetTime();
        grades.assign(probs.size(), RunReport());
        untouched.assign(probs.size(), false);
        auto squash = [](const std::string& s) { std::string o; for (char ch : s) if (!isspace((unsigned char)ch)) o += ch; return o; };
        for (size_t i = 0; i < probs.size(); i++) untouched[i] = squash(spaces[i]->Code()) == squash(probs[i]->starter);
        gradeIndex = 0;
        state = S_GRADING;
        grader.Start(*probs[0], spaces[0]->Code());
    }

    // -------------------------------------------------------------- setup --
    void Setup() {
        Text("Mock Exam", 32, 86, 30, col::heading, F_DISPLAY);
        RichText({34, 128, 900, 0},
                 "The real exam is not multiple choice: you **write code**. A mock exam gives you a few problems and a timer. "
                 "You can run the **example** tests as often as you like. When you submit, **hidden tests** grade every problem.",
                 16, col::dim);
        for (int i = 0; i < 3; i++) {
            const Paper& pp = kPapers[i];
            Rectangle r = {32 + i * 410.0f, 210, 394, 330};
            bool hover = InputEnabled() && MouseIn(r);
            Panel(r, hover ? col::panelHi : col::panel, 16);
            if (hover) RoundRectLines(r, 16, 1.5f, i == 2 ? col::accent2 : col::accent);
            Color c = i == 0 ? col::stack : i == 1 ? col::pointer : col::accent2;
            Text(i == 2 ? "WARM-UP" : ("PAPER " + std::to_string(i + 1)).c_str(), r.x + 28, r.y + 26, 13, c, F_BOLD);
            Text(pp.title, r.x + 28, r.y + 50, 22, col::heading, F_HEAVY);
            Text(pp.blurb, r.x + 28, r.y + 84, 15, col::dim, F_MEDIUM);
            int n = pp.counts[0] + pp.counts[1] + pp.counts[2];
            float y = r.y + 126;
            auto row = [&](const std::string& k, const std::string& v) {
                Text(k, r.x + 28, y, 14, col::faint, F_BOLD);
                Text(v, r.x + 130, y, 15, col::text, F_MEDIUM);
                y += 30;
            };
            row("Problems", std::to_string(n) + "  (" + std::to_string(pp.counts[0]) + " easy, " + std::to_string(pp.counts[1]) + " medium" +
                (pp.counts[2] ? ", " + std::to_string(pp.counts[2]) + " hard)" : ")"));
            row("Time", std::to_string(pp.minutes) + " minutes");
            row("Topics", "");
            for (const std::string& l : Wrap(pp.topics, 15, F_MEDIUM, r.width - 158)) { Text(l, r.x + 130, y - 30, 15, col::text, F_MEDIUM); y += 22; }
            int best = app.progress.Best("mock:" + std::to_string(pp.id));
            Text(best >= 0 ? "Best score: " + std::to_string(best) + "%" : "Not taken yet", r.x + 28, r.y + r.height - 86, 14,
                 best >= 0 ? col::success : col::faint, F_BOLD);
            bool ready = runner::DetectionDone() && !runner::Compilers().empty();
            if (Button({r.x + 28, r.y + r.height - 60, r.width - 56, 44}, "Start", i == 2 ? BTN_DEFAULT : BTN_PRIMARY, ready))
                Start(pp);
        }
        Rectangle info = {32, 560, 1224, 212};
        Panel(info, col::panel, 16);
        Text("How it works", info.x + 28, info.y + 22, 18, col::heading, F_HEAVY);
        RichText({info.x + 28, info.y + 56, 560, 0},
                 "- Switch between problems with the tabs at the top.\n"
                 "- **Run examples** (Ctrl+Enter) shows only the example cases.\n"
                 "- No hints and no solutions until the end.\n"
                 "- When the time runs out the exam is submitted automatically.", 15, col::dim);
        RichText({info.x + 640, info.y + 56, 556, 0},
                 "Your score is the share of **all** tests passed, including the hidden edge cases - "
                 "empty arrays, position 0, the last position, negative numbers. That is how exam code gets marked.\n\n"
                 "Compiler: **" + std::string(runner::Active() ? runner::Active()->label : "none found") + "**", 15, col::dim);
    }

    // ------------------------------------------------------------ running --
    void Running() {
        float rem = Remaining();
        if (rem <= 0) { Toast("Time is up - submitting", col::warn); Submit(); return; }
        // problem tabs
        float x = 24;
        for (size_t i = 0; i < probs.size(); i++) {
            std::string label = std::to_string(i + 1) + ".  " + probs[i]->title;
            float w = std::min(300.0f, Measure(label, 14.5f, F_BOLD).x + 44);
            Rectangle r = {x, 74, w, 40};
            bool active = (int)i == current;
            bool hover = InputEnabled() && MouseIn(r);
            RoundRect(r, 10, active ? col::panelHi : hover ? col::panel : col::bg2);
            if (active) RoundRectLines(r, 10, 1.5f, col::accent);
            Color dot = col::faint;
            const Workspace& ws = *spaces[i];
            if (ws.HasReport()) {
                const RunReport& rep = ws.Report();
                bool exOk = true;
                for (const CaseResult& c : rep.cases) if (c.id <= probs[i]->exampleCases && !c.ok) exOk = false;
                dot = rep.status == RunReport::CompileError ? col::danger : exOk && !rep.cases.empty() ? col::success : col::warn;
            }
            DrawCircleV({r.x + 16, r.y + 20}, 4.5f, dot);
            Text(Ellipsize(label, 14.5f, F_BOLD, w - 40), r.x + 28, r.y + 11, 14.5f, active ? col::heading : col::dim, F_BOLD);
            if (Clicked(r)) current = (int)i;
            x += w + 8;
        }
        // timer + submit
        int s = (int)std::ceil(rem);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
        Color tc = s < 300 ? col::danger : s < 900 ? col::warn : col::text;
        Rectangle timer = {1256 - 300, 74, 128, 40};
        RoundRect(timer, 10, col::bg2);
        TextCenter(buf, timer, 20, tc, F_MONOBOLD);
        if (Button({1256 - 164, 74, 164, 40}, "Submit exam", BTN_SUCCESS)) confirm = true;
        spaces[current]->Frame({24, 124, 1232, 660});
    }

    void ConfirmModal() {
        SetInputEnabled(true);
        DrawRectangleRec({0, 0, W, H}, Alpha(BLACK, 0.6f));
        Rectangle r = {W / 2 - 250, H / 2 - 110, 500, 220};
        Panel(r, col::panel, 16);
        Text("Submit the exam?", r.x + 30, r.y + 28, 23, col::heading, F_HEAVY);
        RichText({r.x + 30, r.y + 70, r.width - 60, 0}, "Every problem will be graded with all its tests, including the hidden ones. You cannot change your code afterwards.", 15, col::dim);
        if (Button({r.x + 30, r.y + 150, 205, 44}, "Keep working")) confirm = false;
        if (Button({r.x + 265, r.y + 150, 205, 44}, "Submit", BTN_SUCCESS)) Submit();
    }

    // ------------------------------------------------------------ grading --
    void Grading() {
        if (grader.Ready()) {
            grades[gradeIndex] = grader.Take();
            gradeIndex++;
            if (gradeIndex < (int)probs.size()) grader.Start(*probs[gradeIndex], spaces[gradeIndex]->Code());
            else Finish();
        }
        Rectangle r = {W / 2 - 260, 260, 520, 220};
        Panel(r, col::panel, 18);
        DrawSpinner({r.x + r.width / 2, r.y + 70}, 26, col::accent);
        TextCenter("Grading your exam...", {r.x, r.y + 116, r.width, 30}, 21, col::heading, F_HEAVY);
        std::string sub = "Problem " + std::to_string(std::min(gradeIndex + 1, (int)probs.size())) + " of " + std::to_string(probs.size());
        TextCenter(sub, {r.x, r.y + 152, r.width, 24}, 15, col::dim, F_MEDIUM);
        ProgressBar({r.x + 60, r.y + 188, r.width - 120, 6}, (float)gradeIndex / probs.size(), col::accent);
    }

    int Score() const {
        if (probs.empty()) return 0;
        double sum = 0;
        for (size_t i = 0; i < grades.size(); i++)
            if (!untouched[i] && grades[i].total) sum += (double)grades[i].passed / grades[i].total;
        return (int)std::round(100 * sum / probs.size());
    }

    void Finish() {
        state = S_RESULTS;
        reviewSel = 0; reviewTab = 0; codeScroll.Reset();
        int score = Score();
        app.progress.SetBest("mock:" + std::to_string(paper->id), score);
        int xp = score / 4;
        for (size_t i = 0; i < probs.size(); i++)
            if (grades[i].status == RunReport::Accepted && app.progress.Mark("solved:" + probs[i]->id)) xp += 10;
        if (xp > 0) app.progress.AddXP(xp, "mock exam");
    }

    // ------------------------------------------------------------ results --
    void Results() {
        int score = Score();
        Rectangle left = {24, 80, 420, 704};
        Panel(left, col::panel, 16);
        Text(paper->title, left.x + 28, left.y + 24, 19, col::heading, F_HEAVY);
        int used = (int)(endTime - startTime);
        Text("Time used: " + std::to_string(used / 60) + " min " + std::to_string(used % 60) + " s", left.x + 28, left.y + 54, 14, col::dim, F_MEDIUM);
        Color sc = score >= 70 ? col::success : score >= 40 ? col::warn : col::danger;
        Vector2 c = {left.x + left.width / 2, left.y + 180};
        DrawRing(c, 70, 82, -90, 270, 64, col::panelHi);
        if (score > 0) DrawRing(c, 70, 82, -90, -90 + 3.6f * score, 64, sc);
        TextCenter(std::to_string(score) + "%", {c.x - 70, c.y - 26, 140, 40}, 38, col::heading, F_DISPLAY);
        TextCenter(score >= 70 ? "Exam ready" : score >= 40 ? "Getting there" : "Keep practising", {c.x - 90, c.y + 18, 180, 20}, 14, sc, F_BOLD);
        float y = left.y + 290;
        for (size_t i = 0; i < probs.size(); i++) {
            const RunReport& g = grades[i];
            Rectangle row = {left.x + 16, y, left.width - 32, 62};
            bool sel = (int)i == reviewSel;
            RoundRect(row, 10, sel ? col::panelHi : col::bg2);
            if (sel) RoundRectLines(row, 10, 1.5f, col::accent);
            Text(Ellipsize(std::to_string(i + 1) + ". " + probs[i]->title, 15.5f, F_BOLD, row.width - 110), row.x + 16, row.y + 11, 15.5f, col::heading, F_BOLD);
            Text(untouched[i] ? "Not attempted" : StatusText(g.status), row.x + 16, row.y + 36, 13, untouched[i] ? col::faint : StatusColor(g.status), F_BOLD);
            std::string frac = std::to_string(untouched[i] ? 0 : g.passed) + "/" + std::to_string(g.total);
            TextRight(frac, row.x + row.width - 16, row.y + 12, 16, g.total && g.passed == g.total ? col::success : col::text, F_HEAVY);
            ProgressBar({row.x + row.width - 96, row.y + 40, 80, 5}, g.total && !untouched[i] ? (float)g.passed / g.total : 0, StatusColor(g.status));
            if (Clicked(row)) { reviewSel = (int)i; codeScroll.Reset(); }
            y += 70;
        }
        float by = left.y + left.height - 112;
        if (Button({left.x + 24, by, left.width - 48, 44}, "Try another mock exam", BTN_PRIMARY)) state = S_SETUP;
        if (Button({left.x + 24, by + 52, left.width - 48, 44}, "Practise these problems", BTN_GHOST))
            app.Open(SceneId::Practice, probs[reviewSel]->id, "");

        // review panel
        Rectangle right = {456, 80, 800, 704};
        Panel(right, col::panel, 16);
        const char* tabs[2] = {"Your code", "Reference solution"};
        float tx = right.x + 14;
        for (int i = 0; i < 2; i++) {
            float tw = Measure(tabs[i], 15, F_BOLD).x + 30;
            if (Tab({tx, right.y + 12, tw, 38}, tabs[i], reviewTab == i)) { reviewTab = i; codeScroll.Reset(); }
            tx += tw + 4;
        }
        const RunReport& g = grades[reviewSel];
        float ry = right.y + 60;
        // failing case summary
        const CaseResult* bad = nullptr;
        for (const CaseResult& cs : g.cases) if (!cs.ok) { bad = &cs; break; }
        if (g.status == RunReport::CompileError) {
            RichText({right.x + 24, ry, right.width - 48, 0}, "**Compile error** - none of the tests could run. Look at the first error in your code below.", 15, col::danger);
            ry += 34;
        } else if (bad) {
            std::string msg = "**First failing test:** `" + bad->input + "`  expected `" + bad->expected + "`, got `" + (bad->ran ? bad->got : std::string("(crashed)")) + "`";
            ry += RichText({right.x + 24, ry, right.width - 48, 0}, msg, 15, col::text) + 8;
        } else {
            RichText({right.x + 24, ry, right.width - 48, 0}, "**All tests passed.** Compare with the reference solution anyway: there is often a shorter way.", 15, col::success);
            ry += 34;
        }
        std::string code = reviewTab == 0 ? spaces[reviewSel]->Code() : probs[reviewSel]->solution;
        CodeView({right.x + 16, ry, right.width - 32, right.y + right.height - ry - 16}, SplitLines(code), codeScroll, -1, 14.5f);
    }
};

} // namespace

Scene* CreateMockScene() { return new MockScene(); }
