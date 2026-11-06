#include "workspace.h"
#include <algorithm>
#include <cmath>

using namespace ui;

// ================================================================ helpers ===
void DrawSpinner(Vector2 c, float radius, Color col) {
    float a = (float)GetTime() * 360.0f;
    DrawRing(c, radius - 3, radius, a, a + 270, 32, col);
}

Color StatusColor(RunReport::Status s) {
    switch (s) {
        case RunReport::Accepted: return col::success;
        case RunReport::TimeLimit: case RunReport::NeedsCpp: case RunReport::NoCompiler: return col::warn;
        default: return col::danger;
    }
}

void CompilerChip(Rectangle r) {
    std::string label;
    bool ok = true;
    if (!runner::DetectionDone()) label = "Looking for a compiler...";
    else if (runner::Compilers().empty()) { label = "No compiler found"; ok = false; }
    else label = runner::Active()->label;
    bool many = runner::DetectionDone() && runner::Compilers().size() > 1;
    bool hover = many && InputEnabled() && MouseIn(r);
    RoundRect(r, r.height / 2, hover ? col::panelHi : col::bg2);
    RoundRectLines(r, r.height / 2, 1, col::border);
    DrawCircleV({r.x + 16, r.y + r.height / 2}, 4, ok ? col::success : col::danger);
    std::string shown = Ellipsize(label + (many ? "   v" : ""), 13.5f, F_MEDIUM, r.width - 36);
    Text(shown, r.x + 28, r.y + (r.height - 13.5f) / 2 - 1, 13.5f, col::text, F_MEDIUM);
    if (many && Clicked(r)) {
        runner::SetActive((runner::ActiveIndex() + 1) % (int)runner::Compilers().size());
        Toast("Compiler: " + runner::Active()->label, col::accent2);
    }
}

// =============================================================== workspace ===
void Workspace::Open(const Problem* p, const std::string& key, bool exam) {
    if (problem && problem != p) SaveNow();
    problem = p;
    saveKey = key;
    examMode = exam;
    std::string saved = key.empty() ? "" : LoadSavedCode(key);
    editor.SetText(saved.empty() ? p->starter : saved);
    hasReport = false; leftTab = 0; consoleTab = 0; selCase = 0; hintsShown = 0;
    solutionUnlocked = app.progress.Has("solved:" + p->id);
    descScroll.Reset(); consoleScroll.Reset(); solScroll.Reset();
    dirtySince = -1;
}

void Workspace::SaveNow() {
    if (problem && !saveKey.empty() && dirtySince >= 0) SaveCode(saveKey, editor.Text());
    dirtySince = -1;
}

void Workspace::Run(bool submit) {
    if (!problem || job.Busy()) return;
    SaveNow();
    pendingSubmit = submit;
    job.Start(*problem, editor.Text());
    consoleTab = 0;
}

void Workspace::Frame(Rectangle area) {
    if (!problem) return;
    // finished job?
    if (job.Ready()) {
        report = job.Take();
        hasReport = true;
        lastSubmit = pendingSubmit;
        editor.SetErrorLines(report.errorLines);
        consoleScroll.Reset();
        selCase = 0;
        // jump to the first failing case that may be shown
        for (size_t i = 0; i < report.cases.size(); i++) {
            bool visible = lastSubmit || report.cases[i].id <= problem->exampleCases;
            if (examMode && report.cases[i].id > problem->exampleCases) visible = false;
            if (visible && !report.cases[i].ok) { selCase = (int)i; break; }
        }
        if (!examMode) app.progress.Mark("tried:" + problem->id);
        if (lastSubmit && report.status == RunReport::Accepted) {
            solvedEvent = true;
            solutionUnlocked = true;
            if (!examMode && app.progress.Mark("solved:" + problem->id))
                app.progress.AddXP(10 + problem->difficulty * 10, "solved " + problem->title);
        }
    }
    if (editor.TakeChanged()) { if (dirtySince < 0) dirtySince = GetTime(); editor.SetErrorLines({}); }
    if (dirtySince >= 0 && GetTime() - dirtySince > 1.5) SaveNow();

    float leftW = 476;
    Rectangle left = {area.x, area.y, leftW, area.height};
    Rectangle right = {area.x + leftW + 12, area.y, area.width - leftW - 12, area.height};
    Description(left);
    Toolbar({right.x, right.y, right.width, 44});
    float consoleH = 262;
    Rectangle ed = {right.x, right.y + 52, right.width, right.height - 52 - consoleH - 10};
    RoundRect(ed, 12, col::codeBg);
    RoundRectLines(ed, 12, 1, col::border);
    editor.fontSize = 15.5f;
    editor.Frame(ed, 7001);
    Console({right.x, ed.y + ed.height + 10, right.width, consoleH});

    // shortcuts
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (ctrl && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) Run(shift && !examMode);
}

// ------------------------------------------------------------- description --
float Workspace::DescContent(Rectangle r, bool draw) {
    const Problem& p = *problem;
    float x = r.x, w = r.width, y = r.y;
    auto txt = [&](const std::string& s, float size, Color c, FontKind k) {
        if (draw) Text(s, x, y, size, c, k);
    };
    // title + badges
    for (const std::string& line : Wrap(p.title, 25, F_HEAVY, w)) { txt(line, 25, col::heading, F_HEAVY); y += 32; }
    y += 6;
    float bx = x;
    if (draw) {
        bx += Badge(bx, y, DifficultyName(p.difficulty), DifficultyColor(p.difficulty), 13) + 8;
        bx += Badge(bx, y, SectionName(p.section), SectionColor(p.section), 13) + 8;
        if (!examMode && app.progress.Has("solved:" + p.id)) Badge(bx, y, "Solved", col::success, 13);
        else if (p.cppOnly) Badge(bx, y, "C++", col::accent, 13);
    }
    y += 40;
    y += RichText({x, y, w, 0}, p.statement, 16.5f, col::text, draw) + 14;

    for (size_t i = 0; i < p.examples.size(); i++) {
        const Example& e = p.examples[i];
        txt("Example " + std::to_string(i + 1), 15, col::heading, F_BOLD);
        y += 26;
        // body
        float lh = 21, pad = 14;
        std::vector<std::pair<std::string, std::string>> rows = {{"Input", e.input}, {"Output", e.output}};
        if (!e.explain.empty()) rows.push_back({"Explanation", e.explain});
        float cw = CodeCharWidth(14);
        int perLine = std::max(10, (int)((w - pad * 2 - 4) / cw));
        float h = pad;
        std::vector<std::vector<std::string>> wrapped;
        for (auto& row : rows) {
            std::vector<std::string> lines;
            if (row.first == "Explanation") lines = Wrap(row.second, 14.5f, F_UI, w - pad * 2);
            else {
                std::string s = row.second;
                for (const std::string& part : SplitLines(s)) {
                    std::string rest = part;
                    while ((int)rest.size() > perLine) { lines.push_back(rest.substr(0, perLine)); rest = rest.substr(perLine); }
                    lines.push_back(rest);
                }
            }
            wrapped.push_back(lines);
            h += 20 + lines.size() * lh + 6;
        }
        h += pad - 6;
        if (draw) {
            RoundRect({x, y, w, h}, 10, col::bg2);
            DrawRectangleRec({x, y + 10, 3, h - 20}, Alpha(col::border, 1));
        }
        float yy = y + pad;
        for (size_t k = 0; k < rows.size(); k++) {
            if (draw) Text(rows[k].first, x + pad, yy, 13, col::dim, F_BOLD);
            yy += 20;
            for (auto& l : wrapped[k]) {
                if (draw) {
                    if (rows[k].first == "Explanation") Text(l, x + pad, yy, 14.5f, col::dim, F_UI);
                    else Text(l, x + pad, yy, 14, col::text, F_MONO);
                }
                yy += lh;
            }
            yy += 6;
        }
        y += h + 16;
    }
    if (!p.constraints.empty()) {
        txt("Constraints", 15, col::heading, F_BOLD);
        y += 26;
        for (const std::string& c : SplitLines(p.constraints)) {
            if (c.empty()) continue;
            if (draw) DrawCircleV({x + 5, y + 9}, 2.5f, col::dim);
            y += RichText({x + 16, y, w - 16, 0}, "`" + c + "`", 15, col::dim, draw) + 2;
        }
        y += 10;
    }
    if (!p.ruleText.empty()) {
        float h = RichText({0, 0, w - 36, 0}, p.ruleText, 15, col::text, false) + 46;
        if (draw) {
            RoundRect({x, y, w, h}, 10, Alpha(col::warn, 0.10f));
            DrawRectangleRec({x, y + 8, 3, h - 16}, col::warn);
            Text("EXAM RULE", x + 18, y + 13, 12.5f, col::warn, F_BOLD);
            RichText({x + 18, y + 34, w - 36, 0}, p.ruleText, 15, col::text);
        }
        y += h + 14;
    }
    return y - r.y;
}

void Workspace::Description(Rectangle r) {
    Panel(r, col::panel, 14);
    const Problem& p = *problem;
    // tabs
    float tx = r.x + 12;
    const char* names[3] = {"Description", "Hints", "Solution"};
    int tabs = examMode ? 1 : 3;
    for (int i = 0; i < tabs; i++) {
        std::string label = names[i];
        if (i == 1) label += " (" + std::to_string(p.hints.size()) + ")";
        float tw = Measure(label, 15, F_BOLD).x + 30;
        if (Tab({tx, r.y + 10, tw, 38}, label, leftTab == i)) { leftTab = i; descScroll.Reset(); }
        tx += tw + 4;
    }
    DrawRectangleRec({r.x + 12, r.y + 52, r.width - 24, 1}, col::border);
    Rectangle view = {r.x + 22, r.y + 62, r.width - 40, r.height - 72};

    if (leftTab == 0) {
        float h = DescContent({0, 0, view.width - 6, 0}, false) + 20;
        descScroll.Update(view, h);
        BeginClip(view);
        DescContent({view.x, view.y + 8 - descScroll.y, view.width - 6, 0}, true);
        EndClip();
        ScrollBar(view, h, descScroll);
    } else if (leftTab == 1) {
        float y = view.y + 8;
        Text("Stuck? Open one hint at a time.", view.x, y, 15, col::dim, F_MEDIUM);
        y += 34;
        for (int i = 0; i < (int)p.hints.size(); i++) {
            if (i < hintsShown) {
                float h = RichText({0, 0, view.width - 36, 0}, p.hints[i], 15.5f, col::text, false) + 46;
                RoundRect({view.x, y, view.width, h}, 10, col::bg2);
                Text("HINT " + std::to_string(i + 1), view.x + 18, y + 13, 12.5f, col::accent2, F_BOLD);
                RichText({view.x + 18, y + 34, view.width - 36, 0}, p.hints[i], 15.5f, col::text);
                y += h + 10;
            } else {
                if (Button({view.x, y, view.width, 46}, "Show hint " + std::to_string(i + 1), BTN_GHOST)) hintsShown = i + 1;
                break;
            }
        }
    } else {
        if (!solutionUnlocked) {
            float y = view.y + 8;
            Text("Solve it first", view.x, y, 21, col::heading, F_HEAVY);
            RichText({view.x, y + 36, view.width, 0},
                     "The solution unlocks when all tests pass. Writing it yourself is what you will have to do in the exam - "
                     "if you are stuck, try the **Hints** tab first.", 15.5f, col::dim);
            if (Button({view.x, y + 132, 210, 44}, "Show it anyway", BTN_GHOST)) solutionUnlocked = true;
        } else {
            Rectangle code = {view.x - 8, view.y + 4, view.width + 12, view.height - 4};
            CodeView(code, SplitLines(p.solution), solScroll, -1, 14.5f);
        }
    }
}

// ----------------------------------------------------------------- toolbar --
void Workspace::Toolbar(Rectangle r) {
    CompilerChip({r.x, r.y + 4, 250, 36});
    bool busy = job.Busy();
    float bx = r.x + r.width;
    if (!examMode) {
        bx -= 120;
        if (Button({bx, r.y + 2, 120, 40}, "Submit", BTN_SUCCESS, !busy, 15)) Run(true);
        bx -= 110;
        if (Button({bx, r.y + 2, 102, 40}, "Run", BTN_PRIMARY, !busy, 15)) Run(false);
    } else {
        bx -= 160;
        if (Button({bx, r.y + 2, 160, 40}, "Run examples", BTN_PRIMARY, !busy, 15)) Run(false);
    }
    bx -= 96;
    if (Button({bx, r.y + 2, 88, 40}, "Reset", BTN_GHOST, !busy, 14)) {
        editor.SetText(problem->starter);
        dirtySince = GetTime();
        Toast("Code reset to the starter template", col::warn);
    }
}

// ----------------------------------------------------------------- console --
void Workspace::Console(Rectangle r) {
    Panel(r, col::panel, 12);
    const Problem& p = *problem;
    const char* tabs[2] = {"Test results", "Output"};
    float tx = r.x + 10;
    for (int i = 0; i < 2; i++) {
        float tw = Measure(tabs[i], 14, F_BOLD).x + 26;
        if (Tab({tx, r.y + 6, tw, 34}, tabs[i], consoleTab == i)) { consoleTab = i; consoleScroll.Reset(); }
        tx += tw + 4;
    }
    DrawRectangleRec({r.x + 10, r.y + 44, r.width - 20, 1}, col::border);
    Rectangle body = {r.x + 20, r.y + 54, r.width - 40, r.height - 62};

    if (job.Busy()) {
        DrawSpinner({body.x + 14, body.y + 22}, 12, col::accent);
        const Compiler* c = runner::Active();
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1fs", job.Elapsed());
        Text(std::string("Compiling and running") + (c ? " with " + c->label : "") + "...", body.x + 40, body.y + 12, 16, col::heading, F_BOLD);
        Text(buf, body.x + 40, body.y + 38, 13.5f, col::dim, F_MEDIUM);
        return;
    }
    if (!hasReport) {
        if (runner::DetectionDone() && runner::Compilers().empty()) {
            Text("No C/C++ compiler was found on this computer", body.x, body.y + 6, 17, col::warn, F_BOLD);
            RichText({body.x, body.y + 38, body.width, 0},
                     "Install one of these and restart Segfault:\n"
                     "- **Visual Studio 2022 Community** with *Desktop development with C++* (Windows)\n"
                     "- **MinGW-w64 / MSYS2** (`g++` in your PATH)\n"
                     "- macOS: `xcode-select --install`   Linux: `sudo apt install g++`", 15, col::dim);
            return;
        }
        RichText(body, examMode
                 ? "**Run examples** compiles your code and checks it against the example cases. "
                   "The hidden tests are only run when you submit the exam."
                 : "**Run** (Ctrl+Enter) checks your code against the " + std::to_string(p.exampleCases) + " example cases. "
                   "**Submit** (Ctrl+Shift+Enter) runs all the tests, including hidden edge cases. A problem is solved when every test passes.",
                 15.5f, col::dim);
        return;
    }

    // --- status line ---
    const RunReport& rep = report;
    bool showHidden = lastSubmit && !examMode;
    int shownTotal = 0, shownPassed = 0;
    for (const CaseResult& c : rep.cases) {
        bool vis = showHidden || c.id <= p.exampleCases;
        if (vis) { shownTotal++; shownPassed += c.ok; }
    }
    RunReport::Status st = rep.status;
    std::string title = StatusText(st);
    Color sc = StatusColor(st);
    bool testsRan = st == RunReport::Accepted || st == RunReport::WrongAnswer ||
                    st == RunReport::RuntimeError || st == RunReport::TimeLimit;
    if (!showHidden && testsRan && shownTotal > 0 && shownPassed == shownTotal && st != RunReport::Accepted &&
        st != RunReport::RuntimeError && st != RunReport::TimeLimit) {
        title = "Examples passed";
        sc = col::success;
    }
    if (!showHidden && st == RunReport::Accepted) title = "Examples passed";

    if (consoleTab == 1) {
        std::string out = rep.stdoutText.empty() ? "(your code did not print anything)" : rep.stdoutText;
        auto lines = SplitLines(out);
        float lh = 20;
        consoleScroll.Update(body, lines.size() * lh);
        BeginClip(body);
        for (size_t i = 0; i < lines.size(); i++)
            Text(lines[i], body.x, body.y + i * lh - consoleScroll.y, 14, rep.stdoutText.empty() ? col::faint : col::text, F_MONO);
        EndClip();
        ScrollBar(body, lines.size() * lh, consoleScroll);
        return;
    }

    Text(title, body.x, body.y, 21, sc, F_HEAVY);
    float tw = Measure(title, 21, F_HEAVY).x;
    std::string sub;
    if (testsRan) sub = std::to_string(shownPassed) + " / " + std::to_string(shownTotal) + (showHidden ? " tests passed" : " example cases passed");
    if (rep.runMs > 0 && testsRan) sub += "   -   " + std::to_string((int)rep.runMs) + " ms";
    Text(sub, body.x + tw + 16, body.y + 6, 14, col::dim, F_MEDIUM);
    if (examMode && testsRan) TextRight("hidden tests run when you submit the exam", body.x + body.width, body.y + 6, 12.5f, col::faint, F_MEDIUM);
    float y = body.y + 38;

    if (st == RunReport::CompileError || st == RunReport::RuleBroken || st == RunReport::NeedsCpp || st == RunReport::NoCompiler) {
        std::string msg = st == RunReport::CompileError ? rep.compileLog
                        : st == RunReport::RuleBroken ? rep.ruleMessage
                        : st == RunReport::NeedsCpp ? "This problem uses C++ (references, classes or new). The active compiler only understands C.\n"
                                                      "Pick another compiler with the button above, or install Visual Studio / MinGW."
                        : "No compiler available.";
        Rectangle box = {body.x, y, body.width, body.y + body.height - y};
        RoundRect(box, 8, col::codeBg);
        auto lines = SplitLines(msg);
        std::vector<std::string> wrapped;
        int per = std::max(20, (int)((box.width - 24) / CodeCharWidth(13.5f)));
        for (auto& l : lines) { std::string s = l; while ((int)s.size() > per) { wrapped.push_back(s.substr(0, per)); s = s.substr(per); } wrapped.push_back(s); }
        Rectangle view = {box.x + 12, box.y + 8, box.width - 20, box.height - 14};
        consoleScroll.Update(view, wrapped.size() * 19.0f);
        BeginClip(view);
        for (size_t i = 0; i < wrapped.size(); i++) {
            const std::string& l = wrapped[i];
            Color c = l.find("error") != std::string::npos ? col::danger : l.find("warning") != std::string::npos ? col::warn : col::text;
            Text(l, view.x, view.y + i * 19.0f - consoleScroll.y, 13.5f, c, F_MONO);
        }
        EndClip();
        ScrollBar(view, wrapped.size() * 19.0f, consoleScroll);
        return;
    }
    if (st == RunReport::RuntimeError) {
        RichText({body.x, y, body.width, 0}, "Your program crashed (exit code " + std::to_string(rep.exitCode) +
                 "). Common causes: dereferencing `nullptr`, reading outside an array, or using deleted memory.", 14.5f, col::dim);
        y += 46;
    } else if (st == RunReport::TimeLimit) {
        RichText({body.x, y, body.width, 0}, "Your program took too long. Look for a loop that never ends, or an algorithm that is too slow "
                 "(for example checking every divisor up to n instead of up to sqrt(n)).", 14.5f, col::dim);
        y += 46;
    }
    // case chips
    float cx = body.x;
    std::vector<int> visibleIdx;
    for (size_t i = 0; i < rep.cases.size(); i++) {
        const CaseResult& c = rep.cases[i];
        bool vis = showHidden || c.id <= p.exampleCases;
        if (!vis) continue;
        visibleIdx.push_back((int)i);
        std::string label = "Case " + std::to_string(c.id);
        float w = Measure(label, 13.5f, F_BOLD).x + 34;
        if (cx + w > body.x + body.width) break;
        Rectangle chip = {cx, y, w, 30};
        bool sel = selCase == (int)i;
        Color cc = !c.ran ? col::faint : c.ok ? col::success : col::danger;
        RoundRect(chip, 8, sel ? Alpha(cc, 0.22f) : col::bg2);
        if (sel) RoundRectLines(chip, 8, 1.5f, cc);
        DrawCircleV({chip.x + 13, chip.y + 15}, 4, cc);
        Text(label, chip.x + 23, chip.y + 7, 13.5f, sel ? col::heading : col::text, F_BOLD);
        if (Clicked(chip)) selCase = (int)i;
        cx += w + 6;
    }
    if (std::find(visibleIdx.begin(), visibleIdx.end(), selCase) == visibleIdx.end() && !visibleIdx.empty()) selCase = visibleIdx[0];
    y += 40;
    if (selCase < 0 || selCase >= (int)rep.cases.size() || visibleIdx.empty()) return;
    const CaseResult& c = rep.cases[selCase];
    struct Row { const char* label; std::string value; Color col; } rows[3] = {
        {"Input", c.input, col::text},
        {"Expected", c.expected, col::success},
        {"Your output", c.ran ? c.got : "(not reached - the program stopped before this case)", c.ok ? col::success : col::danger}};
    float colW = (body.width - 20) / 3;
    for (int k = 0; k < 3; k++) {
        float x = body.x + k * (colW + 10);
        Text(rows[k].label, x, y, 12.5f, col::dim, F_BOLD);
        Rectangle box = {x, y + 20, colW, body.y + body.height - y - 22};
        RoundRect(box, 8, col::codeBg);
        int per = std::max(8, (int)((box.width - 20) / CodeCharWidth(13.5f)));
        std::vector<std::string> lines;
        for (auto& l : SplitLines(rows[k].value)) { std::string s = l; while ((int)s.size() > per) { lines.push_back(s.substr(0, per)); s = s.substr(per); } lines.push_back(s); }
        BeginClip(box);
        for (size_t i = 0; i < lines.size(); i++) Text(lines[i], box.x + 10, box.y + 8 + i * 19.0f, 13.5f, rows[k].col, F_MONO);
        EndClip();
    }
}
