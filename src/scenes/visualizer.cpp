// ============================================================================
// visualizer.cpp  -  Memory Visualizer: step through code, see the stack and
// the heap, pointer arrows, dangling pointers, leaks. Prediction questions
// are asked BEFORE a line runs (active recall).
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <random>

using namespace ui;

namespace {

class VisualizerScene : public Scene {
public:
    const char* Title() const override { return "Memory Visualizer"; }

    void Enter(const std::string& param) override {
        prog = 0;
        const auto& ps = MemPrograms();
        for (size_t i = 0; i < ps.size(); i++) if (ps[i].id == param) prog = (int)i;
        Load(prog);
    }

    void Frame(float dt) override {
        flashT += dt;
        List();
        CodePanel();
        NotePanel();
        MemoryPanel();
        Keys();
    }

private:
    int prog = 0;
    int cur = -1;                 // index of the last executed step
    MemState state;
    bool predictMode = true;
    bool asking = false;          // waiting for an answer to steps[cur+1].ask
    std::vector<int> order;       // shuffled choice order for the current question
    int lastAnswer = -1;          // feedback for the step that was just executed
    bool lastCorrect = false;
    float flashT = 10;
    Scroll listScroll, codeScroll;

    const MemProgram& P() const { return MemPrograms()[prog]; }

    void Load(int p) {
        prog = p; cur = -1; asking = false; lastAnswer = -1;
        Rebuild();
        codeScroll.Reset();
    }

    void Rebuild() {
        state = MemState();
        for (int i = 0; i <= cur; i++) {
            for (auto& c : state.cells) c.changed = false;
            P().steps[i].apply(state);
        }
        flashT = 0;
    }

    void Next() {
        if (cur + 1 >= (int)P().steps.size()) return;
        const MemStep& s = P().steps[cur + 1];
        if (predictMode && !s.ask.empty() && !asking) {
            asking = true;
            order = {0, 1, 2, 3};
            order.resize(s.choices.size());
            for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
            std::mt19937 rng((unsigned)(prog * 131 + cur * 17 + 7));
            std::shuffle(order.begin(), order.end(), rng);
            return;
        }
        Execute(-1);
    }

    void Execute(int answer) {
        asking = false;
        lastAnswer = answer;
        lastCorrect = answer == 0;
        if (answer >= 0) {
            std::string key = "predict:" + P().id + ":" + std::to_string(cur + 1);
            if (lastCorrect && app.progress.Mark(key)) app.progress.AddXP(5, "correct prediction");
        }
        cur++;
        Rebuild();
        if (cur == (int)P().steps.size() - 1 && app.progress.Mark("prog:" + P().id))
            app.progress.AddXP(15, "program finished");
    }

    void Prev() {
        if (asking) { asking = false; return; }
        if (cur < 0) return;
        cur--; lastAnswer = -1;
        Rebuild();
    }

    void Keys() {
        if (AnyTextFocused()) return;
        if (asking) {
            for (int k = 0; k < (int)order.size(); k++)
                if (IsKeyPressed(KEY_ONE + k)) Execute(order[k]);
            if (IsKeyPressed(KEY_LEFT)) Prev();
            return;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE)) Next();
        if (IsKeyPressed(KEY_LEFT)) Prev();
        if (IsKeyPressed(KEY_R)) Load(prog);
    }

    // --------------------------------------------------------- left list --
    void List() {
        Rectangle r = {24, 80, 252, 704};
        Panel(r, col::panel, 14);
        Text("PROGRAMS", r.x + 18, r.y + 18, 12, col::dim, F_BOLD);
        Rectangle view = {r.x + 6, r.y + 40, r.width - 12, r.height - 120};
        const auto& ps = MemPrograms();
        float contentH = ps.size() * 62.0f;
        listScroll.Update(view, contentH);
        BeginClip(view);
        for (size_t i = 0; i < ps.size(); i++) {
            Rectangle item = {view.x, view.y + i * 62.0f - listScroll.y, view.width - 6, 58};
            if (ListItem(item, ps[i].title, TopicName(ps[i].topic), (int)i == prog, TopicColor(ps[i].topic),
                         app.progress.Has("prog:" + ps[i].id)) && MouseIn(view))
                Load((int)i);
        }
        EndClip();
        // predict toggle
        Rectangle tg = {r.x + 14, r.y + r.height - 70, r.width - 28, 56};
        RoundRect(tg, 10, col::panelHi);
        Text("Predict mode", tg.x + 14, tg.y + 10, 15, WHITE, F_BOLD);
        Text("answer before each line runs", tg.x + 14, tg.y + 31, 11.5f, col::dim);
        Rectangle sw = {tg.x + tg.width - 52, tg.y + 17, 40, 22};
        RoundRect(sw, 11, predictMode ? col::accent : col::border);
        DrawCircleV({predictMode ? sw.x + 29 : sw.x + 11, sw.y + 11}, 8, WHITE);
        if (Clicked(tg)) { predictMode = !predictMode; asking = false; }
    }

    // --------------------------------------------------------- code ------
    void CodePanel() {
        Rectangle r = {292, 80, 452, 410};
        const auto& steps = P().steps;
        int line = cur >= 0 ? steps[cur].line : -1;
        int nextLine = cur + 1 < (int)steps.size() ? steps[cur + 1].line : -1;
        std::vector<Color> tint(P().code.size(), Color{0, 0, 0, 0});
        if (nextLine >= 0) tint[nextLine] = Alpha(col::warn, 0.10f);
        Text(P().title, r.x + 2, r.y, 20, WHITE, F_DISPLAY);
        Text(P().blurb, r.x + 2, r.y + 28, 13, col::dim);
        CodeView({r.x, r.y + 54, r.width, r.height - 54}, P().code, codeScroll, line, 15.5f, &tint, true);
        // legend
        float ly = r.y + r.height + 8;
        RoundRect({r.x + 4, ly + 3, 10, 10}, 2, col::accent);
        Text("just ran", r.x + 20, ly, 12.5f, col::dim);
        RoundRect({r.x + 90, ly + 3, 10, 10}, 2, Alpha(col::warn, 0.5f));
        Text("runs next", r.x + 106, ly, 12.5f, col::dim);
        TextRight("step " + std::to_string(cur + 1) + " / " + std::to_string(steps.size()), r.x + r.width, ly, 13, col::text, F_BOLD);
    }

    // ------------------------------------------------- note / question ---
    void NotePanel() {
        Rectangle r = {292, 524, 452, 260};
        Panel(r, col::panel, 14);
        const auto& steps = P().steps;
        float x = r.x + 20, w = r.width - 40;
        if (asking) {
            const MemStep& s = steps[cur + 1];
            Text("PREDICT", x, r.y + 16, 12, col::warn, F_BOLD);
            float h = RichText({x, r.y + 36, w, 0}, s.ask, 16, WHITE);
            float y = r.y + 44 + h;
            for (size_t k = 0; k < order.size(); k++) {
                Rectangle b = {x, y, w, 34};
                bool hover = MouseIn(b);
                RoundRect(b, 8, hover ? col::panelHi : col::bg2);
                RoundRectLines(b, 8, 1, hover ? col::warn : col::border);
                Text(std::to_string(k + 1), b.x + 12, b.y + 9, 14, col::warn, F_MONOBOLD);
                Text(Ellipsize(s.choices[order[k]], 14.5f, F_UI, w - 50), b.x + 34, b.y + 8, 14.5f, col::text);
                if (Clicked(b)) { Execute(order[k]); break; }
                y += 40;
            }
            return;
        }
        float y = r.y + 16;
        if (cur < 0) {
            Text("READY", x, y, 12, col::accent2, F_BOLD);
            RichText({x, y + 22, w, 0}, "Press **Next** (or Space) to run the first line. "
                     "Orange arrows are pointers. Watch the addresses: a pointer's value IS an address.", 15.5f, col::text);
        } else {
            const MemStep& s = steps[cur];
            if (lastAnswer >= 0) {
                Color c = lastCorrect ? col::success : col::danger;
                Text(lastCorrect ? "CORRECT" : "NOT QUITE", x, y, 12, c, F_BOLD);
                if (!lastCorrect) Text("answer: " + s.choices[0], x + 100, y, 12.5f, col::dim);
            } else {
                Text("LINE " + std::to_string(s.line + 1), x, y, 12, col::accent2, F_BOLD);
            }
            RichText({x, y + 22, w, 0}, s.note, 15.5f, col::text);
        }
        // controls
        float by = r.y + r.height - 54;
        bool done = cur + 1 >= (int)steps.size();
        if (Button({x, by, 96, 40}, "<  Back", BTN_GHOST, cur >= 0)) Prev();
        if (Button({x + 106, by, 96, 40}, "Reset", BTN_GHOST, cur >= 0)) Load(prog);
        if (done) {
            bool hasNext = prog + 1 < (int)MemPrograms().size();
            if (Button({r.x + r.width - 200, by, 180, 40}, hasNext ? "Next program  >" : "Finished!", BTN_SUCCESS, hasNext))
                Load(prog + 1);
        } else if (Button({r.x + r.width - 160, by, 140, 40}, "Next  >", BTN_PRIMARY)) {
            Next();
        }
    }

    // ------------------------------------------------------------ memory --
    struct Placed { Rectangle box; Rectangle valueBox; const MemCell* cell; };

    bool IsLeaked(const MemCell& c) const {
        if (c.region != Region::Heap || c.state != CellState::Alive) return false;
        // a block is reachable if any alive pointer points at it, or (for
        // arrays) at the first element of its block
        for (const MemCell& p : state.cells) {
            if (p.state != CellState::Alive || p.pointsTo < 0) continue;
            if (p.pointsTo == c.addr) return false;
            if (!c.name.empty() && c.name.find('[') != std::string::npos) {
                // element of a named heap array: reachable if the array base is
                int base = c.addr - 4 * std::max(0, std::atoi(c.name.substr(c.name.find('[') + 1).c_str()));
                if (p.pointsTo == base) return false;
            }
        }
        return true;
    }

    const MemCell* Target(const MemCell& p) const {
        if (p.pointsTo < 0) return nullptr;
        for (int i = (int)state.cells.size() - 1; i >= 0; i--)
            if (state.cells[i].addr == p.pointsTo) return &state.cells[i];
        return nullptr;
    }

    void MemoryPanel() {
        Rectangle r = {760, 80, 496, 704};
        Panel(r, col::panel, 14);
        float stackX = r.x + 20, stackW = 238;
        float heapX = r.x + 300, heapW = 176;
        Text("STACK", stackX, r.y + 16, 13, col::stack, F_BOLD);
        Text("local variables, freed automatically", stackX + 58, r.y + 17, 11.5f, col::faint);
        Text("HEAP", heapX, r.y + 16, 13, col::heap, F_BOLD);
        Text("new / malloc", heapX + 48, r.y + 17, 11.5f, col::faint);
        DrawRectangleRec({heapX - 22, r.y + 14, 1, 520}, Alpha(col::border, 0.6f));

        std::vector<Placed> placed;
        float flash = std::max(0.0f, 1.0f - flashT / 1.2f);
        // stack cells grouped by frame
        float y = r.y + 46;
        std::string frame;
        for (const MemCell& c : state.cells) {
            if (c.region != Region::Stack) continue;
            if (c.frame != frame) {
                frame = c.frame;
                Text(frame + "()", stackX, y + 4, 12.5f, col::dim, F_MONOBOLD);
                DrawRectangleRec({stackX + 8 + Measure(frame + "()", 12.5f, F_MONOBOLD).x, y + 11, stackW - 16 - Measure(frame + "()", 12.5f, F_MONOBOLD).x, 1}, Alpha(col::border, 0.6f));
                y += 24;
            }
            Rectangle box = {stackX, y, stackW, 40};
            placed.push_back({box, {box.x + 140, box.y + 5, 92, 30}, &c});
            y += 46;
        }
        if (y == r.y + 46) Text("(empty)", stackX, y + 4, 13, col::faint);
        float hy = r.y + 46;
        for (const MemCell& c : state.cells) {
            if (c.region != Region::Heap) continue;
            Rectangle box = {heapX, hy, heapW, 40};
            placed.push_back({box, {box.x + 84, box.y + 5, 86, 30}, &c});
            hy += 46;
        }
        if (hy == r.y + 46) Text("(nothing allocated)", heapX, hy + 4, 13, col::faint);

        // cells
        for (const Placed& pl : placed) DrawCell(pl, flash);
        // arrows on top
        for (const Placed& pl : placed) {
            const MemCell& c = *pl.cell;
            if (c.pointsTo < 0 || c.state != CellState::Alive) continue;
            const MemCell* t = Target(c);
            if (!t) continue;
            const Placed* tp = nullptr;
            for (const Placed& q : placed) if (q.cell == t) tp = &q;
            if (!tp) continue;
            bool dangling = t->state != CellState::Alive;
            Color ac = dangling ? col::danger : col::pointer;
            Vector2 from = {pl.valueBox.x + pl.valueBox.width - 6, pl.valueBox.y + pl.valueBox.height / 2};
            if (t->region == Region::Stack && c.region == Region::Stack) {
                Vector2 to = {tp->box.x + tp->box.width + 2, tp->box.y + tp->box.height / 2};
                float bend = 30 + std::fabs(to.y - from.y) * 0.18f;
                CurvedArrow(from, {stackX + stackW + bend, (from.y + to.y) / 2}, to, ac, 2.2f, 9);
            } else {
                Vector2 to = {tp->box.x - 2, tp->box.y + tp->box.height / 2};
                CurvedArrow(from, {(from.x + to.x) / 2, from.y + (to.y - from.y) * 0.2f - 10}, to, ac, 2.2f, 9);
            }
        }

        // console output
        Rectangle con = {r.x + 16, r.y + r.height - 150, r.width - 32, 134};
        RoundRect(con, 10, col::codeBg);
        Text("OUTPUT", con.x + 14, con.y + 10, 11.5f, col::dim, F_BOLD);
        float oy = con.y + 32;
        int start = std::max(0, (int)state.output.size() - 5);
        for (int i = start; i < (int)state.output.size(); i++) {
            Text("> " + state.output[i], con.x + 14, oy, 14, col::text, F_MONO);
            oy += 19;
        }
        // legend
        float lx = r.x + 20, ly = r.y + r.height - 182;
        auto leg = [&](Color c, const char* s) {
            RoundRect({lx, ly + 2, 12, 12}, 3, c);
            Text(s, lx + 18, ly, 12, col::dim);
            lx += Measure(s, 12).x + 38;
        };
        leg(col::pointer, "pointer");
        leg(col::danger, "dangling / leaked");
        leg(col::faint, "out of scope / freed");
        leg(col::accent2, "changed");
    }

    void DrawCell(const Placed& pl, float flash) {
        const MemCell& c = *pl.cell;
        bool dead = c.state != CellState::Alive;
        bool leaked = IsLeaked(c);
        bool isPtr = c.type.find('*') != std::string::npos;
        const MemCell* t = Target(c);
        bool dangling = !dead && isPtr && t && t->state != CellState::Alive;
        Color base = c.region == Region::Heap ? col::heap : col::stack;
        Rectangle b = pl.box;
        RoundRect(b, 8, dead ? Alpha(col::panelHi, 0.4f) : col::panelHi);
        Color line = leaked || dangling ? col::danger : Alpha(base, dead ? 0.25f : 0.55f);
        RoundRectLines(b, 8, leaked || dangling ? 2.0f : 1.0f, line);
        if (c.changed && flash > 0 && !dead) RoundRectLines({b.x - 2, b.y - 2, b.width + 4, b.height + 4}, 9, 2, Alpha(col::accent2, flash));
        float a = dead ? 0.4f : 1.0f;
        // address + name
        Text(std::to_string(c.addr), b.x + 8, b.y + 3, 10.5f, Alpha(col::dim, a), F_MONO);
        std::string label = c.name.empty() ? c.type : c.name;
        if (c.region == Region::Stack) label = c.name;
        Text(Ellipsize(label, 14, F_MONOBOLD, c.region == Region::Heap ? 74 : 86), b.x + 8, b.y + 18, 14, Alpha(WHITE, a), F_MONOBOLD);
        if (c.region == Region::Stack) Text(c.type, b.x + 98, b.y + 21, 11, Alpha(isPtr ? col::pointer : col::dim, a), F_MONO);
        // value box
        Rectangle v = pl.valueBox;
        RoundRect(v, 6, Alpha(col::codeBg, dead ? 0.5f : 1));
        std::string val = c.value;
        Color vc = isPtr ? col::pointer : col::text;
        if (val == "?") vc = col::faint;
        TextCenter(Ellipsize(val, 14, F_MONO, v.width - 8), v, 14, Alpha(vc, a), F_MONO);
        // state tags
        std::string tag;
        Color tc = col::faint;
        if (c.state == CellState::OutOfScope) tag = "out of scope";
        if (c.state == CellState::Freed) tag = "freed";
        if (leaked) { tag = "LEAKED"; tc = col::danger; }
        if (dangling) { tag = "dangling"; tc = col::danger; }
        if (!tag.empty()) {
            float tw = Measure(tag, 10.5f, F_BOLD).x + 10;
            RoundRect({b.x + b.width - tw - 4, b.y - 8, tw, 15}, 7, Mix(col::panel, tc, 0.25f));
            Text(tag, b.x + b.width - tw + 1, b.y - 7, 10.5f, tc, F_BOLD);
        }
        if (dead) DrawLineEx({b.x + 8, b.y + 26}, {b.x + 8 + Measure(label, 14, F_MONOBOLD).x, b.y + 26}, 1.5f, Alpha(col::dim, 0.8f));
    }
};

} // namespace

Scene* CreateVisualizerScene() { return new VisualizerScene(); }
