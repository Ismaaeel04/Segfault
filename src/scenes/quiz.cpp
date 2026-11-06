// ============================================================================
// quiz.cpp  -  Quick multiple-choice revision with instant explanations.
// Wrong answers go to a "mistakes" list you can review later.
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace ui;

namespace {

bool LooksLikeCode(const std::string& s) {
    return s.find_first_of(";{}*&[]") != std::string::npos || s.find("->") != std::string::npos ||
           s.find("()") != std::string::npos || s.find(" = ") != std::string::npos;
}

void Check(Vector2 c, float s, Color col) {
    DrawLineEx({c.x - s, c.y}, {c.x - s * 0.3f, c.y + s * 0.8f}, 2.6f, col);
    DrawLineEx({c.x - s * 0.3f, c.y + s * 0.8f}, {c.x + s, c.y - s * 0.8f}, 2.6f, col);
}
void Cross(Vector2 c, float s, Color col) {
    DrawLineEx({c.x - s, c.y - s}, {c.x + s, c.y + s}, 2.6f, col);
    DrawLineEx({c.x - s, c.y + s}, {c.x + s, c.y - s}, 2.6f, col);
}

class QuizScene : public Scene {
public:
    const char* Title() const override { return mode == M_REVIEW ? "Quiz - mistakes" : "Quick Quiz"; }

    void Enter(const std::string& param) override {
        if (param.rfind("section:", 0) == 0) Start(M_SECTION, std::atoi(param.c_str() + 8));
        else if (param == "quick") Start(M_QUICK, -1);
        else if (param == "review") Start(M_REVIEW, -1);
        else state = S_SETUP;
    }

    void Frame(float) override {
        if (state == S_SETUP) Setup();
        else if (state == S_RUN) Run();
        else Results();
    }

private:
    enum Mode { M_SECTION, M_QUICK, M_REVIEW } mode = M_QUICK;
    enum State { S_SETUP, S_RUN, S_RESULTS } state = S_SETUP;
    int section = -1;
    std::vector<int> qs, answers;
    int qi = 0, chosen = -1, streak = 0, bestStreak = 0;
    std::vector<int> order;
    std::mt19937 rng{std::random_device{}()};
    Scroll codeScroll, resultScroll;

    void Start(Mode m, int s) {
        mode = m; section = s;
        std::vector<int> pool;
        for (const Question& q : Questions()) {
            if (m == M_SECTION && q.section != s) continue;
            if (m == M_REVIEW && !app.progress.mistakes.count(q.id)) continue;
            pool.push_back(q.id);
        }
        if (pool.empty()) {
            Toast(m == M_REVIEW ? "No mistakes to review - nice!" : "No questions here yet", col::success);
            state = S_SETUP;
            return;
        }
        std::shuffle(pool.begin(), pool.end(), rng);
        if (m != M_REVIEW && pool.size() > 10) pool.resize(10);
        qs = pool;
        answers.assign(qs.size(), -1);
        qi = 0; streak = 0; bestStreak = 0;
        state = S_RUN;
        NewQuestion();
    }

    void NewQuestion() {
        const Question* q = QuestionById(qs[qi]);
        order.resize(q->options.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
        std::shuffle(order.begin(), order.end(), rng);
        chosen = -1;
        codeScroll.Reset();
    }

    void Answer(int original) {
        const Question* q = QuestionById(qs[qi]);
        answers[qi] = original;
        chosen = original;
        Progress& p = app.progress;
        p.quizAnswered++;
        if (original == 0) {
            p.quizCorrect++;
            streak++;
            bestStreak = std::max(bestStreak, streak);
            bool first = p.Mark("q:" + std::to_string(q->id));
            if (mode == M_REVIEW) p.mistakes.erase(q->id);
            p.AddXP(first ? 5 : 1, streak >= 3 ? "streak x" + std::to_string(streak) : "correct");
        } else {
            streak = 0;
            p.mistakes.insert(q->id);
            p.Save();
        }
    }

    void Advance() {
        if (qi + 1 < (int)qs.size()) { qi++; NewQuestion(); }
        else { state = S_RESULTS; resultScroll.Reset(); }
    }

    // ------------------------------------------------------------- setup --
    void Setup() {
        Text("Quick Quiz", 32, 86, 30, col::heading, F_DISPLAY);
        Text("Ten quick questions with an explanation after each one. Good for revising on the bus.", 34, 126, 16, col::dim, F_MEDIUM);
        for (int s = 0; s < S_COUNT; s++) {
            Rectangle r = {32 + (s % 3) * 300.0f, 170 + (s / 3) * 202.0f, 286, 188};
            bool hover = InputEnabled() && MouseIn(r);
            Panel(r, hover ? col::panelHi : col::panel, 16);
            Color sc = SectionColor(s);
            if (hover) RoundRectLines(r, 16, 1.5f, sc);
            int n = 0, done = 0;
            for (const Question& q : Questions()) if (q.section == s) { n++; done += app.progress.Has("q:" + std::to_string(q.id)); }
            char num[8]; snprintf(num, sizeof(num), "%02d", s + 1);
            Text(num, r.x + 22, r.y + 22, 14, sc, F_HEAVY);
            Text(SectionName(s), r.x + 22, r.y + 48, 19, col::heading, F_HEAVY);
            float y = r.y + 80;
            for (const std::string& l : Wrap(SectionShort(s), 14, F_UI, r.width - 44)) { Text(l, r.x + 22, y, 14, col::dim); y += 20; }
            Text(std::to_string(done) + " / " + std::to_string(n) + " mastered", r.x + 22, r.y + r.height - 44, 13.5f, col::text, F_BOLD);
            ProgressBar({r.x + 22, r.y + r.height - 20, r.width - 44, 6}, n ? (float)done / n : 0, sc);
            if (Clicked(r) && n > 0) Start(M_SECTION, s);
        }
        Rectangle side = {944, 170, 312, 594};
        Panel(side, col::panel, 16);
        float y = side.y + 26;
        Text("Mixed", side.x + 26, y, 21, col::heading, F_HEAVY);
        y += 36;
        RichText({side.x + 26, y, side.width - 52, 0}, "Questions from every section, in random order.", 15, col::dim);
        y += 56;
        if (Button({side.x + 26, y, side.width - 52, 50}, "10 random questions", BTN_PRIMARY)) Start(M_QUICK, -1);
        y += 62;
        int m = (int)app.progress.mistakes.size();
        if (Button({side.x + 26, y, side.width - 52, 50}, "Review mistakes (" + std::to_string(m) + ")", BTN_DEFAULT, m > 0)) Start(M_REVIEW, -1);
        y += 84;
        DrawRectangleRec({side.x + 26, y, side.width - 52, 1}, col::border);
        y += 24;
        const Progress& p = app.progress;
        int acc = p.quizAnswered ? (int)std::round(100.0 * p.quizCorrect / p.quizAnswered) : 0;
        Text(std::to_string(p.quizAnswered), side.x + 26, y, 30, col::heading, F_DISPLAY);
        Text("answers", side.x + 26, y + 38, 13.5f, col::dim, F_MEDIUM);
        Text(p.quizAnswered ? std::to_string(acc) + "%" : "-", side.x + 166, y, 30, col::heading, F_DISPLAY);
        Text("accuracy", side.x + 166, y + 38, 13.5f, col::dim, F_MEDIUM);
        y += 90;
        RichText({side.x + 26, y, side.width - 52, 0},
                 "Remember: the real exam is about **writing code**. Use the quiz for revision, and **Practice** for exam preparation.",
                 14.5f, col::faint);
    }

    // --------------------------------------------------------------- run --
    void Run() {
        const Question* q = QuestionById(qs[qi]);
        const float x = 190, w = 900;
        Color sc = SectionColor(q->section);
        Badge(x, 84, SectionName(q->section), sc, 13);
        std::string counter = "Question " + std::to_string(qi + 1) + " of " + std::to_string(qs.size());
        TextRight(counter, x + w, 88, 14.5f, col::dim, F_BOLD);
        if (streak >= 2) {
            std::string st = "streak " + std::to_string(streak);
            float sw = Measure(st, 13, F_BOLD).x + 20;
            float cx = x + w - Measure(counter, 14.5f, F_BOLD).x - sw - 16;
            Badge(cx, 84, st, col::warn, 13);
        }
        // segmented progress
        float segW = (w - (qs.size() - 1) * 4) / qs.size();
        for (size_t i = 0; i < qs.size(); i++) {
            Color c = col::panelHi;
            if (answers[i] >= 0) c = answers[i] == 0 ? col::success : col::danger;
            else if ((int)i == qi) c = Alpha(sc, 0.6f);
            RoundRect({x + i * (segW + 4), 118, segW, 6}, 3, c);
        }

        float y = 146;
        y += RichText({x, y, w, 0}, q->prompt, 21, col::heading) + 14;
        if (!q->code.empty()) {
            auto lines = SplitLines(q->code);
            float h = std::min(230.0f, lines.size() * CodeLineHeight(15.5f) + 24);
            CodeView({x, y, w, h}, lines, codeScroll, -1, 15.5f);
            y += h + 16;
        }
        bool answered = chosen >= 0;
        for (size_t k = 0; k < order.size(); k++) {
            int o = order[k];
            Rectangle b = {x, y, w, 54};
            bool hover = !answered && InputEnabled() && MouseIn(b);
            Color fill = hover ? col::panelHi : col::panel, line = hover ? sc : col::border;
            Color badge = col::bg2, letter = col::dim;
            if (answered && o == 0) { fill = Alpha(col::success, 0.13f); line = col::success; badge = col::success; letter = col::bg; }
            else if (answered && o == chosen) { fill = Alpha(col::danger, 0.13f); line = col::danger; badge = col::danger; letter = col::bg; }
            else if (answered) { fill = Alpha(col::panel, 0.6f); }
            RoundRect(b, 12, fill);
            RoundRectLines(b, 12, answered && (o == 0 || o == chosen) ? 2.0f : 1.0f, line);
            Rectangle key = {b.x + 12, b.y + 11, 32, 32};
            RoundRect(key, 9, hover ? sc : badge);
            if (answered && o == 0) Check({key.x + 16, key.y + 16}, 7, letter);
            else if (answered && o == chosen) Cross({key.x + 16, key.y + 16}, 6, letter);
            else TextCenter(std::string(1, (char)('A' + k)), key, 15, hover ? col::bg : letter, F_HEAVY);
            const std::string& txt = q->options[o];
            bool code = LooksLikeCode(txt);
            float fs = code ? 15.5f : 17;
            Color tc = answered && o != 0 && o != chosen ? col::dim : col::text;
            Text(Ellipsize(txt, fs, code ? F_MONO : F_MEDIUM, w - 84), b.x + 60, b.y + (54 - fs) / 2 - 1, fs, tc, code ? F_MONO : F_MEDIUM);
            if (!answered && Clicked(b)) { Answer(o); return; }
            y += 62;
        }
        if (!answered && !AnyTextFocused()) {
            for (size_t k = 0; k < order.size(); k++)
                if (IsKeyPressed(KEY_ONE + (int)k) || IsKeyPressed(KEY_A + (int)k)) { Answer(order[k]); return; }
        }
        if (answered) {
            bool ok = chosen == 0;
            Color c = ok ? col::success : col::danger;
            float th = RichText({0, 0, w - 260, 0}, q->explain, 16, col::text, false);
            Rectangle fb = {x, y + 6, w, std::max(100.0f, th + 60)};
            RoundRect(fb, 14, Alpha(c, 0.09f));
            RoundRectLines(fb, 14, 1, Alpha(c, 0.4f));
            Text(ok ? "Correct!" : "Not quite", fb.x + 24, fb.y + 18, 19, c, F_HEAVY);
            RichText({fb.x + 24, fb.y + 48, w - 260, 0}, q->explain, 16, col::text);
            bool last = qi + 1 == (int)qs.size();
            if (Button({fb.x + fb.width - 200, fb.y + fb.height / 2 - 24, 176, 48}, last ? "See results" : "Continue  >", BTN_PRIMARY) ||
                ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && !AnyTextFocused()))
                Advance();
        }
    }

    // ----------------------------------------------------------- results --
    void Results() {
        int correct = 0;
        for (size_t i = 0; i < qs.size(); i++) correct += answers[i] == 0;
        int pct = qs.empty() ? 0 : (int)std::round(100.0 * correct / qs.size());
        Rectangle left = {24, 80, 400, 704};
        Panel(left, col::panel, 16);
        Text("Round complete", left.x + 28, left.y + 26, 22, col::heading, F_HEAVY);
        Color pc = pct >= 70 ? col::success : pct >= 40 ? col::warn : col::danger;
        Vector2 c = {left.x + left.width / 2, left.y + 190};
        DrawRing(c, 76, 90, -90, 270, 64, col::panelHi);
        if (pct > 0) DrawRing(c, 76, 90, -90, -90 + 3.6f * pct, 64, pc);
        TextCenter(std::to_string(pct) + "%", {c.x - 80, c.y - 30, 160, 44}, 42, col::heading, F_DISPLAY);
        TextCenter(std::to_string(correct) + " of " + std::to_string(qs.size()) + " correct", {c.x - 90, c.y + 20, 180, 20}, 14.5f, col::dim, F_BOLD);
        const char* msg = pct >= 90 ? "Excellent!" : pct >= 70 ? "Well done." : pct >= 40 ? "Getting there." : "Time to study this section again.";
        TextCenter(msg, {left.x, left.y + 300, left.width, 24}, 17, pc, F_BOLD);
        if (bestStreak >= 3) TextCenter("Best streak: " + std::to_string(bestStreak), {left.x, left.y + 330, left.width, 20}, 14, col::warn, F_BOLD);
        float by = left.y + left.height - 228;
        if (Button({left.x + 28, by, left.width - 56, 48}, "Play again", BTN_PRIMARY)) Start(mode, section);
        int m = (int)app.progress.mistakes.size();
        if (Button({left.x + 28, by + 58, left.width - 56, 48}, "Review mistakes (" + std::to_string(m) + ")", BTN_DEFAULT, m > 0)) Start(M_REVIEW, -1);
        if (mode == M_SECTION && Button({left.x + 28, by + 116, left.width - 56, 48}, "Study this section", BTN_GHOST))
            app.Go(SceneId::Study, "section:" + std::to_string(section));
        else if (mode != M_SECTION && Button({left.x + 28, by + 116, left.width - 56, 48}, "Choose another quiz", BTN_GHOST)) state = S_SETUP;

        Rectangle right = {440, 80, 816, 704};
        Panel(right, col::panel, 16);
        Text("Your answers", right.x + 28, right.y + 24, 19, col::heading, F_HEAVY);
        Rectangle view = {right.x + 16, right.y + 64, right.width - 32, right.height - 80};
        std::vector<float> hs;
        float contentH = 0;
        for (size_t i = 0; i < qs.size(); i++) {
            const Question* q = QuestionById(qs[i]);
            float h = 40 + RichText({0, 0, view.width - 60, 0}, q->prompt, 16, col::text, false) + 30;
            if (answers[i] != 0) h += RichText({0, 0, view.width - 60, 0}, q->explain, 14.5f, col::dim, false) + 8;
            hs.push_back(h + 10);
            contentH += h + 22;
        }
        resultScroll.Update(view, contentH);
        BeginClip(view);
        float y = view.y - resultScroll.y;
        for (size_t i = 0; i < qs.size(); i++) {
            const Question* q = QuestionById(qs[i]);
            bool ok = answers[i] == 0;
            Rectangle card = {view.x, y, view.width - 10, hs[i]};
            if (card.y + card.height > view.y && card.y < view.y + view.height) {
                RoundRect(card, 12, col::bg2);
                Color c = ok ? col::success : col::danger;
                DrawCircleV({card.x + 26, card.y + 26}, 11, Alpha(c, 0.2f));
                if (ok) Check({card.x + 26, card.y + 26}, 5, c); else Cross({card.x + 26, card.y + 26}, 4.5f, c);
                Text(SectionName(q->section), card.x + 48, card.y + 17, 13, SectionColor(q->section), F_BOLD);
                float h = RichText({card.x + 24, card.y + 44, card.width - 50, 0}, q->prompt, 16, col::text);
                float cy = card.y + 50 + h;
                Text("Answer:  " + q->options[0], card.x + 24, cy, 14.5f, col::success, LooksLikeCode(q->options[0]) ? F_MONO : F_BOLD);
                if (!ok) RichText({card.x + 24, cy + 26, card.width - 50, 0}, q->explain, 14.5f, col::dim);
            }
            y += hs[i] + 12;
        }
        EndClip();
        ScrollBar(view, contentH, resultScroll);
    }
};

} // namespace

Scene* CreateQuizScene() { return new QuizScene(); }
