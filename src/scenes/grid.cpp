// ============================================================================
// grid.cpp  -  Grid Crusher: Lab1 Q19-Q21 as an interactive puzzle.
// The scan animation shows how ReplaceWithMinusOne searches; the final result
// always comes from the REAL compiled functions (PrintArray,
// ReplaceWithMinusOne, FallDownAndReplace).
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include "exam1.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace ui;

namespace {

const int kLab1Grid[4][6] = {{0, 0, 3, 1, 3, 4}, {0, 0, 2, 3, 4, 3}, {0, 0, 1, 3, 3, 2}, {0, 0, 1, 1, 1, 1}};

struct TraceStep { int r, c, dir, length; bool found; };   // dir: 0 skip, 1 horizontal, 2 vertical

class GridScene : public Scene {
public:
    const char* Title() const override { return "Grid Crusher"; }

    void Enter(const std::string&) override {
        if (!initialised) { ResetLab(); initialised = true; }
    }

    void Frame(float dt) override {
        Update(dt);
        Board();
        Console();
        Controls();
        CodePanel();
    }

private:
    int grid[4][6];
    bool initialised = false;
    int totalScore = 0, lastScore = -1, rounds = 0;
    // scan animation
    std::vector<TraceStep> trace;
    int ti = -1;
    float tTimer = 0;
    // fall animation
    bool falling = false;
    float fallT = 0;
    int fromRow[4][6];          // where each cell's value came from (-1..-4 = new from above)
    // predict mode
    bool predicting = false;
    bool picked[4][6] = {};
    std::string message = "Lab1's grid. Press **Step scan** to watch ReplaceWithMinusOne search, or **Predict** the first cluster yourself.";
    std::string printed;
    Scroll codeScroll, conScroll;
    std::string snippet = "ReplaceWithMinusOne";
    int codeLine = -1;

    void Print() {
        CaptureOutput cap;
        PrintArray(grid);
        printed = app.consoleCapture;
    }

    void ResetLab() {
        std::memcpy(grid, kLab1Grid, sizeof(grid));
        totalScore = 0; lastScore = -1; rounds = 0; trace.clear(); ti = -1; falling = false; predicting = false;
        std::memset(picked, 0, sizeof(picked));
        Print();
    }

    void Randomize() {
        for (int r = 0; r < 4; r++) for (int c = 0; c < 6; c++) grid[r][c] = GetRandomValue(0, 9) == 0 ? 0 : GetRandomValue(1, 4);
        // make sure there is at least one cluster most of the time
        int r = GetRandomValue(0, 3), c = GetRandomValue(0, 3), v = GetRandomValue(1, 4);
        for (int k = 0; k < 3; k++) grid[r][c + k] = v;
        totalScore = 0; lastScore = -1; rounds = 0; trace.clear(); ti = -1; falling = false; predicting = false;
        std::memset(picked, 0, sizeof(picked));
        message = "A random grid with at least one cluster. Can you predict which one is found FIRST?";
        Print();
    }

    bool Busy() const { return ti >= 0 || falling; }

    // Mirrors ReplaceWithMinusOne's search order to produce an animation trace.
    void BuildTrace() {
        trace.clear();
        for (int r = 0; r < 4; r++) for (int c = 0; c < 6; c++) {
            int v = grid[r][c];
            if (v < 1 || v > 4) { trace.push_back({r, c, 0, 0, false}); continue; }
            int len = 1;
            while (c + len < 6 && grid[r][c + len] == v) len++;
            trace.push_back({r, c, 1, len, len >= 3});
            if (len >= 3) return;
            len = 1;
            while (r + len < 4 && grid[r + len][c] == v) len++;
            trace.push_back({r, c, 2, len, len >= 3});
            if (len >= 3) return;
        }
    }

    bool HasMinusOne() const {
        for (int r = 0; r < 4; r++) for (int c = 0; c < 6; c++) if (grid[r][c] == -1) return true;
        return false;
    }

    void RunReplace() {
        int score = ReplaceWithMinusOne(grid);           // the real function
        lastScore = score;
        Print();
        if (score > 0) {
            totalScore += score; rounds++;
            message = "Cluster replaced with -1. Score **" + std::to_string(score) + "**. Now press **FallDownAndReplace**.";
        } else {
            message = "ReplaceWithMinusOne returned **0**: no cluster of 3+ identical values (1-4) left.";
        }
    }

    void StartFall() {
        int before[4][6];
        std::memcpy(before, grid, sizeof(grid));
        // mapping for the animation: same algorithm as FallDownAndReplace
        for (int c = 0; c < 6; c++) {
            int write = 3, fresh = 0;
            for (int r = 3; r >= 0; r--) if (before[r][c] != -1) fromRow[write--][c] = r;
            while (write >= 0) fromRow[write--][c] = -1 - fresh++;
        }
        FallDownAndReplace(grid);                         // the real function
        Print();
        falling = true; fallT = 0;
        snippet = "FallDownAndReplace"; codeLine = sources::FindLine(snippet, "array[writeRow][col] = array[row][col]");
        message = "Every column is compacted from the bottom up; the gaps at the top are filled with **-2** (new items, never part of a cluster).";
    }

    void Update(float dt) {
        if (ti >= 0) {
            tTimer += dt;
            const TraceStep& s = trace[ti];
            float hold = s.dir == 0 ? 0.07f : s.found ? 1.5f : 0.24f;
            if (tTimer > hold) {
                tTimer = 0;
                if (s.found || ti + 1 >= (int)trace.size()) {
                    ti = -1;
                    RunReplace();
                } else ti++;
            }
            if (ti >= 0) {
                const TraceStep& t = trace[ti];
                codeLine = sources::FindLine(snippet, t.dir == 0 ? "continue" : t.dir == 1 ? "while (col + length" : "while (row + length");
                if (t.found) codeLine = sources::FindLine(snippet, "return value * length", t.dir == 1 ? 0 : sources::FindLine(snippet, "while (row + length"));
            }
        }
        if (falling) {
            fallT += dt * 1.6f;
            if (fallT >= 1) { falling = false; codeLine = -1; }
        }
    }

    Color ValueColor(int v) const {
        switch (v) {
            case 1: return {45, 212, 191, 255};
            case 2: return {56, 189, 248, 255};
            case 3: return {251, 146, 60, 255};
            case 4: return {244, 114, 182, 255};
            default: return col::panelHi;
        }
    }

    Rectangle Cell(int r, int c) const { return {72 + c * 92.0f, 132 + r * 92.0f, 84, 84}; }

    void Board() {
        Rectangle panel = {24, 80, 620, 480};
        Panel(panel, col::panel, 14);
        Text("int array[4][6]", 44, 96, 15, col::dim, F_MONOBOLD);
        TextRight("rounds " + std::to_string(rounds), panel.x + panel.width - 20, 96, 14, col::dim, F_BOLD);
        for (int c = 0; c < 6; c++) TextCenter(std::to_string(c), {Cell(0, c).x, 116, 84, 14}, 12, col::faint, F_MONO);
        for (int r = 0; r < 4; r++) TextCenter(std::to_string(r), {44, Cell(r, 0).y, 20, 84}, 12, col::faint, F_MONO);
        const TraceStep* s = ti >= 0 ? &trace[ti] : nullptr;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 6; c++) {
            Rectangle b = Cell(r, c);
            if (falling) {
                float from = fromRow[r][c] >= 0 ? (float)fromRow[r][c] : (float)fromRow[r][c] - 0.3f;
                float e = 1 - std::pow(1 - fallT, 3.0f);
                b.y = 132 + (from + (r - from) * e) * 92.0f;
            }
            int v = grid[r][c];
            bool inRun = false;
            if (s && s->dir) {
                for (int k = 0; k < s->length; k++) {
                    if (s->dir == 1 && r == s->r && c == s->c + k) inRun = true;
                    if (s->dir == 2 && c == s->c && r == s->r + k) inRun = true;
                }
            }
            Color fill = ValueColor(v);
            if (v >= 1 && v <= 4) fill = Mix(col::panel, fill, 0.78f);
            if (v == -1) fill = Alpha(col::danger, 0.22f);
            if (v == -2) fill = Mix(col::panelHi, col::faint, 0.25f);
            BeginClip({panel.x, 128, panel.width, 380});
            RoundRect(b, 12, fill);
            if (inRun) RoundRectLines(b, 12, 3, s->found ? WHITE : col::warn);
            if (predicting && picked[r][c]) RoundRectLines({b.x + 4, b.y + 4, b.width - 8, b.height - 8}, 10, 3, col::accent);
            if (s && s->r == r && s->c == c) RoundRectLines({b.x - 4, b.y - 4, b.width + 8, b.height + 8}, 14, 2, col::accent2);
            Color tc = v >= 1 && v <= 4 ? Color{12, 15, 22, 255} : v == -1 ? col::danger : col::faint;
            TextCenter(std::to_string(v), b, 30, tc, F_DISPLAY);
            EndClip();
            if (predicting && !Busy() && Clicked(Cell(r, c))) picked[r][c] = !picked[r][c];
        }
        if (s) {
            std::string d = s->dir == 0 ? "skip: " + std::to_string(grid[s->r][s->c]) + " is never in a cluster"
                          : std::string(s->dir == 1 ? "horizontal" : "vertical") + " run of " + std::to_string(s->length) +
                            (s->found ? "  ->  CLUSTER!" : "  (need 3)");
            Text("[" + std::to_string(s->r) + "][" + std::to_string(s->c) + "]  " + d, 44, 526, 15, s->found ? col::success : col::warn, F_MONOBOLD);
        } else if (predicting) {
            Text("Click the cells of the FIRST cluster, then press Check.", 44, 526, 15, col::accent, F_BOLD);
        }
    }

    void Console() {
        Rectangle r = {24, 572, 620, 212};
        RoundRect(r, 12, col::codeBg);
        RoundRectLines(r, 12, 1, Alpha(col::border, 0.6f));
        Text("PrintArray(array)  -  real output", r.x + 16, r.y + 12, 11.5f, col::dim, F_BOLD);
        auto lines = SplitLines(printed);
        for (size_t i = 0; i < lines.size(); i++) Text(lines[i], r.x + 16, r.y + 38 + i * 24.0f, 18, col::text, F_MONO);
        Text("score so far", r.x + 400, r.y + 50, 13, col::dim);
        Text(std::to_string(totalScore), r.x + 400, r.y + 70, 44, col::warn, F_DISPLAY);
        if (lastScore >= 0) Text("last: " + std::to_string(lastScore), r.x + 400, r.y + 128, 14, col::dim, F_BOLD);
    }

    void Controls() {
        Rectangle r = {660, 80, 596, 236};
        Panel(r, col::panel, 14);
        float x = r.x + 20, y = r.y + 18;
        bool minus = HasMinusOne();
        if (Button({x, y, 170, 42}, "Step scan", BTN_PRIMARY, !Busy() && !minus && !predicting)) {
            snippet = "ReplaceWithMinusOne"; BuildTrace(); ti = 0; tTimer = 0;
            message = "Scanning left-to-right, top-to-bottom. At each cell: **horizontal first**, then vertical.";
        }
        if (Button({x + 182, y, 190, 42}, "ReplaceWithMinusOne", BTN_DEFAULT, !Busy() && !minus && !predicting)) {
            snippet = "ReplaceWithMinusOne"; codeLine = -1; RunReplace();
        }
        if (Button({x + 384, y, 172, 42}, "FallDownAndReplace", BTN_DEFAULT, !Busy() && minus)) StartFall();
        y += 54;
        if (!predicting) {
            if (Button({x, y, 170, 42}, "Predict", BTN_SUCCESS, !Busy() && !minus)) {
                predicting = true; std::memset(picked, 0, sizeof(picked));
                message = "Which cells will ReplaceWithMinusOne replace? Click them on the board.";
            }
        } else {
            if (Button({x, y, 170, 42}, "Check", BTN_SUCCESS)) CheckPrediction();
            if (Button({x + 182, y, 110, 42}, "Cancel", BTN_GHOST)) predicting = false;
        }
        if (Button({x + (predicting ? 304 : 182), y, 120, 42}, "Lab1 grid", BTN_GHOST, !Busy())) { ResetLab(); message = "Back to the grid from Lab1's main()."; }
        if (Button({x + (predicting ? 436 : 314), y, 120, 42}, "Random", BTN_GHOST, !Busy())) Randomize();
        RichText({x, y + 60, r.width - 40, 0}, message, 15, col::text);
    }

    void CheckPrediction() {
        int copy[4][6];
        std::memcpy(copy, grid, sizeof(grid));
        int score = ReplaceWithMinusOne(copy);
        bool ok = true;
        int pickedCount = 0;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 6; c++) {
            bool will = copy[r][c] == -1;
            pickedCount += picked[r][c];
            if (will != picked[r][c]) ok = false;
        }
        predicting = false;
        if (score == 0) ok = pickedCount == 0;
        if (ok) {
            if (app.progress.Mark("grid:predict:" + std::to_string(rounds) + ":" + std::to_string(totalScore)))
                app.progress.AddXP(10, "correct prediction");
            message = "**Correct!** Now press Step scan or ReplaceWithMinusOne to apply it.";
        } else {
            message = "Not quite. Press **Step scan** to see the order the function checks the cells.";
            Toast("Not the first cluster", col::danger);
        }
    }

    void CodePanel() {
        Rectangle r = {660, 328, 596, 456};
        CodeView(r, sources::Get(snippet), codeScroll, codeLine, 13.5f, nullptr, true);
    }
};

} // namespace

Scene* CreateGridScene() { return new GridScene(); }
