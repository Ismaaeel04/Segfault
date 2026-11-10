// ============================================================================
// home.cpp  -  Three main paths (Study, Practice, Mock Exam), the quiz, and
// progress per section.
// ============================================================================
#include "app/app.h"
#include "app/runner.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cctype>
#include <cmath>

using namespace ui;

namespace {

void IconBook(Rectangle r, Color c) {
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    RoundRect({cx - 17, cy - 12, 16, 24}, 3, c);
    RoundRect({cx + 1, cy - 12, 16, 24}, 3, Alpha(c, 0.65f));
    for (int i = 0; i < 3; i++) DrawRectangleRec({cx - 13, cy - 6 + i * 5.5f, 9, 2}, col::panel);
}
void IconCode(Rectangle r, Color c) {
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    DrawLineEx({cx - 8, cy - 9}, {cx - 16, cy}, 3, c);
    DrawLineEx({cx - 16, cy}, {cx - 8, cy + 9}, 3, c);
    DrawLineEx({cx + 8, cy - 9}, {cx + 16, cy}, 3, c);
    DrawLineEx({cx + 16, cy}, {cx + 8, cy + 9}, 3, c);
    DrawLineEx({cx + 3, cy - 12}, {cx - 3, cy + 12}, 3, c);
}
void IconClock(Rectangle r, Color c, float t) {
    Vector2 m = {r.x + r.width / 2, r.y + r.height / 2 + 2};
    DrawRing(m, 12, 15, 0, 360, 36, c);
    DrawRectangleRec({m.x - 3, m.y - 20, 6, 4}, c);
    DrawLineEx(m, {m.x + std::cos(t - 1.57f) * 8, m.y + std::sin(t - 1.57f) * 8}, 2.5f, c);
}
void IconQuiz(Rectangle r, Color c) {
    Vector2 m = {r.x + r.width / 2, r.y + r.height / 2};
    DrawRing(m, 13, 16, 0, 360, 36, c);
    TextCenter("?", {m.x - 10, m.y - 12, 20, 24}, 19, c, F_DISPLAY);
}

class HomeScene : public Scene {
public:
    const char* Title() const override { return "Home"; }
    void Enter(const std::string&) override {
        confirmReset = false;
        for (float& l : lift) l = 0;
        // your place by XP, for the leaderboard button
        place = 0;
        std::vector<LeaderRow> rows = LoadLeaderboard();
        students = (int)rows.size();
        for (const LeaderRow& r : rows) if (r.you) { place = 1; for (const LeaderRow& o : rows) place += o.xp > r.xp; }
    }

    void Frame(float dt) override {
        t += dt;
        if (confirmReset) SetInputEnabled(false);
        Hero();
        MainCards();
        QuizCard();
        SectionsCard();
        if (confirmReset) ResetModal();
    }

private:
    float t = 0;
    bool confirmReset = false;
    float lift[3] = {0, 0, 0};            // how far each card has risen under the mouse
    int place = 0, students = 0;          // your place on the leaderboard (0 = not logged in)

    void HeroStat(float x, const std::string& value, const std::string& label, Color c) {
        Text(value, x, 92, 22, c, F_DISPLAY);
        Text(label, x, 121, 12.5f, col::dim, F_MEDIUM);
    }

    void Hero() {
        Rectangle r = {32, 78, 1226, 74};
        Shadow(r, 16);
        GradientRoundRect(r, 16, Mix(col::panel, col::accent, 0.24f), Mix(col::panel, col::accent2, 0.10f));
        RoundRectLines(r, 16, 1, Alpha(col::accent, 0.3f));
        // avatar with the student's initial
        Rectangle av = {r.x + 16, r.y + 13, 48, 48};
        GradientRoundRect(av, 24, col::accent, Mix(col::accent, col::accent2, 0.7f));
        std::string who = app.user.empty() ? "S" : std::string(1, (char)std::toupper((unsigned char)app.user[0]));
        TextCenter(who, av, 22, OnColor(col::accent), F_DISPLAY);

        Text(app.user.empty() ? "Ready for your coding exams?" : "Welcome back, " + app.user, r.x + 80, r.y + 13, 23, col::heading, F_DISPLAY);
        Text("Study a topic, write real code, then test yourself under exam conditions.", r.x + 81, r.y + 44, 14.5f, col::dim, F_MEDIUM);

        const Progress& p = app.progress;
        int solved = 0, lessons = 0;
        for (const Problem& pr : Problems()) solved += p.Has("solved:" + pr.id);
        for (const Lesson& l : Lessons()) lessons += p.Has("lesson:" + l.id);
        float x = r.x + r.width - 372;
        std::string label = place ? "Leaderboard   #" + std::to_string(place) + " of " + std::to_string(students) : "Leaderboard";
        float lw = Measure(label, 14, F_BOLD).x + 64;
        Rectangle lb = {x - 44 - lw, r.y + 17, lw, 40};
        if (Button(lb, label, BTN_DEFAULT, true, 14)) app.Go(SceneId::Leaderboard, "");
        // a little trophy
        Color gold = Tone({250, 204, 21, 255});
        float ix = lb.x + 20, iy = lb.y + 12;
        RoundRect({ix - 6, iy, 12, 9}, 3, gold);
        DrawRectangleRec({ix - 1.5f, iy + 8, 3, 5}, gold);
        RoundRect({ix - 5, iy + 13, 10, 3}, 1.5f, gold);
        DrawRectangleRec({x - 22, r.y + 14, 1, r.height - 28}, Alpha(col::border, 0.8f));
        HeroStat(x, std::to_string(p.Level()), "level", col::warn);
        HeroStat(x + 92, std::to_string(solved) + "/" + std::to_string(Problems().size()), "solved", col::heading);
        HeroStat(x + 192, std::to_string(lessons) + "/" + std::to_string(Lessons().size()), "lessons", col::heading);
        HeroStat(x + 292, p.quizAnswered ? std::to_string((int)std::round(100.0 * p.quizCorrect / p.quizAnswered)) + "%" : "-", "quiz", col::heading);
    }

    struct CardInfo {
        const char* tag; const char* title; const char* desc; Color color;
        std::string stat; float progress; std::string cta; int icon;
    };

    bool Card(int index, Rectangle r, const CardInfo& c) {
        bool hover = InputEnabled() && MouseIn(r);
        float& up = lift[index];
        up += ((hover ? 4.0f : 0.0f) - up) * std::min(1.0f, GetFrameTime() * 16);
        Rectangle rr = r;
        rr.y -= up;
        if (hover) Shadow(rr, 18, 1.5f);
        Panel(rr, hover ? col::panelHi : col::panel, 18);
        // soft glow of the card's colour in its corner
        BeginClip({rr.x + 1, rr.y + 1, rr.width - 2, rr.height - 2});
        SoftGlow({rr.x + rr.width - 20, rr.y + 16}, 190, c.color, hover ? 0.24f : 0.15f);
        EndClip();
        if (hover) RoundRectLines(rr, 18, 1.5f, Alpha(c.color, 0.9f));
        RoundRect({rr.x + 24, rr.y, 64, 4}, 2, c.color);
        Rectangle ic = {rr.x + 24, rr.y + 28, 58, 58};
        RoundRect(ic, 14, Alpha(c.color, 0.16f));
        RoundRectLines(ic, 14, 1, Alpha(c.color, 0.3f));
        if (c.icon == 0) IconBook(ic, c.color);
        else if (c.icon == 1) IconCode(ic, c.color);
        else IconClock(ic, c.color, t);
        Text(c.tag, rr.x + rr.width - 24 - Measure(c.tag, 12.5f, F_HEAVY).x, rr.y + 34, 12.5f, c.color, F_HEAVY);
        Text(c.title, rr.x + 24, rr.y + 106, 27, col::heading, F_DISPLAY);
        float y = rr.y + 148;
        for (const std::string& l : Wrap(c.desc, 15.5f, F_UI, rr.width - 48)) { Text(l, rr.x + 24, y, 15.5f, col::dim, F_UI); y += 23; }
        float sy = rr.y + rr.height - 118;
        Text(c.stat, rr.x + 24, sy, 14.5f, col::text, F_BOLD);
        ProgressBar({rr.x + 24, sy + 26, rr.width - 48, 7}, c.progress, c.color);
        Rectangle b = {rr.x + 24, rr.y + rr.height - 66, rr.width - 48, 46};
        if (hover) GradientRoundRect(b, 11, c.color, Mix(c.color, col::accent2, 0.25f));
        else RoundRect(b, 11, Alpha(c.color, 0.16f));
        TextCenter(Ellipsize(c.cta, 15.5f, F_BOLD, b.width - 30), b, 15.5f, hover ? OnColor(c.color) : c.color, F_BOLD);
        return Clicked(r);
    }

    void MainCards() {
        const auto& ls = Lessons();
        const auto& ps = Problems();
        int lessonsDone = 0, solved = 0;
        std::string nextLesson, nextLessonTitle, nextProblem, nextProblemTitle;
        for (const Lesson& l : ls) {
            if (app.progress.Has("lesson:" + l.id)) lessonsDone++;
            else if (nextLesson.empty()) { nextLesson = l.id; nextLessonTitle = l.title; }
        }
        for (const Problem& p : ps) {
            if (app.progress.Has("solved:" + p.id)) solved++;
            else if (nextProblem.empty()) { nextProblem = p.id; nextProblemTitle = p.title; }
        }
        int b1 = app.progress.Best("mock:1"), b2 = app.progress.Best("mock:2");
        std::string mockStat = "Best:  Paper 1 " + (b1 >= 0 ? std::to_string(b1) + "%" : std::string("-")) +
                               "    Paper 2 " + (b2 >= 0 ? std::to_string(b2) + "%" : std::string("-"));
        CardInfo cards[3] = {
            {"01  STUDY", "Study", "Lessons by section with step-by-step code walkthroughs, quick checks and interactive memory demos.",
             col::accent, std::to_string(lessonsDone) + " / " + std::to_string(ls.size()) + " lessons completed",
             ls.empty() ? 0 : (float)lessonsDone / ls.size(),
             nextLesson.empty() ? "Review the lessons" : "Continue: " + nextLessonTitle, 0},
            {"02  PRACTICE", "Practice", "Problems like LeetCode: write the function in the editor, run it, and pass every test - hidden edge cases included.",
             col::success, std::to_string(solved) + " / " + std::to_string(ps.size()) + " problems solved",
             ps.empty() ? 0 : (float)solved / ps.size(),
             nextProblem.empty() ? "All solved - browse problems" : "Next: " + nextProblemTitle, 1},
            {"03  MOCK EXAM", "Mock Exam", "A timed coding exam like the real one. Example tests while you work, hidden tests mark your answers.",
             col::pointer, mockStat, (std::max(b1, 0) + std::max(b2, 0)) / 200.0f, "Start a mock exam", 2},
        };
        for (int i = 0; i < 3; i++) {
            Rectangle r = {32 + i * 414.0f, 166, 398, 340};
            if (Card(i, r, cards[i])) {
                if (i == 0) app.Go(SceneId::Study, nextLesson);
                else if (i == 1) app.Go(SceneId::Practice, "");
                else app.Go(SceneId::Mock, "");
            }
        }
    }

    void QuizCard() {
        Rectangle r = {32, 524, 500, 252};
        Panel(r, col::panel, 18);
        Color c = col::stack;
        Rectangle ic = {r.x + 24, r.y + 24, 50, 50};
        RoundRect(ic, 12, Alpha(c, 0.16f));
        IconQuiz(ic, c);
        Text("Quick Quiz", r.x + 90, r.y + 26, 22, col::heading, F_HEAVY);
        Text("Multiple choice, for quick revision", r.x + 90, r.y + 56, 14.5f, col::dim, F_MEDIUM);
        const Progress& p = app.progress;
        int acc = p.quizAnswered ? (int)std::round(100.0 * p.quizCorrect / p.quizAnswered) : -1;
        Text(std::to_string(p.quizAnswered), r.x + 24, r.y + 104, 26, col::heading, F_DISPLAY);
        Text("answered", r.x + 24, r.y + 138, 13, col::dim, F_MEDIUM);
        Text(acc >= 0 ? std::to_string(acc) + "%" : "-", r.x + 170, r.y + 104, 26, col::heading, F_DISPLAY);
        Text("accuracy", r.x + 170, r.y + 138, 13, col::dim, F_MEDIUM);
        Text(std::to_string(p.mistakes.size()), r.x + 316, r.y + 104, 26, p.mistakes.empty() ? col::heading : col::warn, F_DISPLAY);
        Text("to review", r.x + 316, r.y + 138, 13, col::dim, F_MEDIUM);
        if (Button({r.x + 24, r.y + 180, 214, 46}, "Start a quiz", BTN_PRIMARY)) app.Go(SceneId::Quiz, "");
        if (Button({r.x + 250, r.y + 180, 226, 46}, "Review mistakes", BTN_GHOST, !p.mistakes.empty())) app.Go(SceneId::Quiz, "review");
    }

    void SectionsCard() {
        Rectangle r = {548, 524, 708, 252};
        Panel(r, col::panel, 18);
        Text("Progress by section", r.x + 24, r.y + 22, 18, col::heading, F_HEAVY);
        if (Button({r.x + r.width - 104, r.y + 16, 84, 32}, "Reset", BTN_GHOST, true, 13)) confirmReset = true;
        for (int s = 0; s < S_COUNT; s++) {
            int colIdx = s < 5 ? 0 : 1, rowIdx = s < 5 ? s : s - 5;
            float x = r.x + 24 + colIdx * 340, y = r.y + 62 + rowIdx * 36;
            Rectangle row = {x - 8, y - 6, 324, 32};
            bool hover = InputEnabled() && MouseIn(row);
            if (hover) RoundRect(row, 8, col::panelHi);
            float m = SectionMastery(s);
            Text(SectionName(s), x, y, 14.5f, hover ? col::heading : col::text, F_MEDIUM);
            ProgressBar({x + 170, y + 6, 100, 6}, m, SectionColor(s));
            TextRight(std::to_string((int)std::round(m * 100)) + "%", x + 312, y, 14, SectionColor(s), F_BOLD);
            if (Clicked(row)) app.Go(SceneId::Study, "section:" + std::to_string(s));
        }
    }

    void ResetModal() {
        SetInputEnabled(true);
        DrawRectangleRec({0, 0, W, H}, Alpha(BLACK, 0.6f));
        Rectangle r = {W / 2 - 230, H / 2 - 100, 460, 200};
        Panel(r, col::panel, 16);
        Text("Reset all progress?", r.x + 28, r.y + 26, 22, col::heading, F_HEAVY);
        Text("XP, lessons, solved problems and quiz mistakes will be cleared.", r.x + 28, r.y + 66, 14.5f, col::dim, F_MEDIUM);
        Text("Your saved code is kept.", r.x + 28, r.y + 88, 14.5f, col::dim, F_MEDIUM);
        if (Button({r.x + 28, r.y + 130, 190, 44}, "Cancel")) confirmReset = false;
        if (Button({r.x + 242, r.y + 130, 190, 44}, "Reset", BTN_DANGER)) {
            app.progress.Reset();
            confirmReset = false;
            Toast("Progress reset", col::danger);
        }
    }
};

} // namespace

Scene* CreateHomeScene() { return new HomeScene(); }
