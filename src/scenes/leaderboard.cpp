// ============================================================================
// leaderboard.cpp  -  Every student on this computer, ranked by XP, problems
// solved, quiz accuracy or best mock exam.
// ============================================================================
#include "app/app.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include <algorithm>
#include <cctype>
#include <cmath>

using namespace ui;

namespace {

enum Metric { BY_XP, BY_SOLVED, BY_QUIZ, BY_MOCK, METRIC_COUNT };
const char* kMetricName[METRIC_COUNT] = {"XP", "Problems", "Quiz", "Mock exam"};

int QuizPct(const LeaderRow& r) { return r.answered ? (int)std::lround(100.0 * r.correct / r.answered) : -1; }

std::string Value(const LeaderRow& r, int m) {
    switch (m) {
        case BY_SOLVED: return std::to_string(r.solved) + " solved";
        case BY_QUIZ: return QuizPct(r) >= 0 ? std::to_string(QuizPct(r)) + "% correct" : "no answers yet";
        case BY_MOCK: return r.bestMock >= 0 ? std::to_string(r.bestMock) + "% best" : "not taken yet";
        default: return std::to_string(r.xp) + " XP";
    }
}

class LeaderboardScene : public Scene {
public:
    const char* Title() const override { return "Leaderboard"; }
    void Enter(const std::string&) override {
        rows = LoadLeaderboard();
        scroll.Reset();
        Sort();
    }

    void Frame(float) override {
        Text("Leaderboard", 32, 86, 30, col::heading, F_DISPLAY);
        Text("Everyone who studies on this computer. Keep practising to climb the table.", 34, 126, 16, col::dim, F_MEDIUM);
        float tx = W - 32 - METRIC_COUNT * 112.0f;
        for (int m = 0; m < METRIC_COUNT; m++)
            if (Tab({tx + m * 112.0f, 92, 106, 40}, kMetricName[m], metric == m) && metric != m) { metric = m; Sort(); }

        Podium();
        Table({32, 360, W - 64, H - 360 - 16});
    }

private:
    std::vector<LeaderRow> rows;
    int metric = BY_XP;
    Scroll scroll;

    static int Key(const LeaderRow& r, int m) {
        switch (m) {
            case BY_SOLVED: return r.solved;
            case BY_QUIZ: return QuizPct(r);
            case BY_MOCK: return r.bestMock;
            default: return r.xp;
        }
    }

    void Sort() {
        int m = metric;
        std::stable_sort(rows.begin(), rows.end(), [m](const LeaderRow& a, const LeaderRow& b) {
            if (Key(a, m) != Key(b, m)) return Key(a, m) > Key(b, m);
            if (a.xp != b.xp) return a.xp > b.xp;
            return a.name < b.name;
        });
    }

    // Students level on the chosen measure share a place.
    int Place(size_t i) const {
        while (i > 0 && Key(rows[i - 1], metric) == Key(rows[i], metric)) i--;
        return (int)i + 1;
    }

    static Color Medal(int place) {
        if (place == 1) return Tone({250, 204, 21, 255});
        if (place == 2) return Tone({176, 190, 210, 255});
        if (place == 3) return Tone({224, 132, 60, 255});
        return col::faint;
    }

    void Avatar(Rectangle r, const std::string& name) {
        GradientRoundRect(r, r.width / 2, col::accent, Mix(col::accent, col::accent2, 0.7f));
        std::string initial(1, (char)std::toupper((unsigned char)name[0]));
        TextCenter(initial, r, r.width * 0.45f, OnColor(col::accent), F_DISPLAY);
    }

    void Podium() {
        if (rows.empty()) return;
        // second, first, third: the winner stands in the middle and higher
        const int order[3] = {1, 0, 2};
        const float w = 300, gap = 24, x0 = W / 2 - (w * 3 + gap * 2) / 2;
        for (int k = 0; k < 3; k++) {
            size_t i = order[k];
            if (i >= rows.size()) continue;
            const LeaderRow& r = rows[i];
            int place = Place(i);
            float lift = i == 0 ? 0 : 24;
            Rectangle card = {x0 + k * (w + gap), 156 + lift, w, 188 - lift};
            Color medal = Medal(place);
            Panel(card, r.you ? Mix(col::panel, col::accent, 0.12f) : col::panel, 18);
            BeginClip({card.x + 1, card.y + 1, card.width - 2, card.height - 2});
            SoftGlow({card.x + card.width / 2, card.y}, 170, medal, 0.22f);
            EndClip();
            RoundRect({card.x + card.width / 2 - 32, card.y, 64, 4}, 2, medal);

            Rectangle av = {card.x + card.width / 2 - 30, card.y + 22, 60, 60};
            Avatar(av, r.name);
            Vector2 badge = {av.x + av.width - 4, av.y + av.height - 6};
            DrawCircleV(badge, 15, col::panel);
            DrawCircleV(badge, 12.5f, medal);
            TextCenter(std::to_string(place), {badge.x - 12, badge.y - 12, 24, 24}, 14, OnColor(medal), F_HEAVY);

            std::string name = Ellipsize(r.name, 19, F_HEAVY, card.width - 40);
            TextCenter(name, {card.x, card.y + 92, card.width, 24}, 19, col::heading, F_HEAVY);
            TextCenter(Value(r, metric), {card.x, card.y + 120, card.width, 22}, 15, medal, F_BOLD);
            if (r.you) Badge(card.x + 16, card.y + 16, "You", col::accent, 12);
        }
        if (rows.size() == 1)
            TextCenter("You are the only student here so far. Classmates who make an account on this computer appear here too.",
                       {0, 316, W, 24}, 14.5f, col::dim, F_MEDIUM);
    }

    void Table(Rectangle r) {
        Panel(r, col::panel, 16);
        const float cols[] = {r.x + 24, r.x + 84, r.x + 470, r.x + 580, r.x + 710, r.x + 840, r.x + 970, r.x + 1090};
        const char* heads[] = {"#", "STUDENT", "LEVEL", "XP", "SOLVED", "LESSONS", "QUIZ", "BEST MOCK"};
        for (int c = 0; c < 8; c++) Text(heads[c], cols[c], r.y + 16, 12.5f, col::faint, F_BOLD);
        DrawRectangleRec({r.x + 12, r.y + 42, r.width - 24, 1}, col::border);

        Rectangle view = {r.x + 8, r.y + 48, r.width - 16, r.height - 56};
        const float rh = 48;
        scroll.Update(view, rows.size() * rh + 4);
        BeginClip(view);
        const int problems = (int)Problems().size(), lessons = (int)Lessons().size();
        for (size_t i = 0; i < rows.size(); i++) {
            const LeaderRow& p = rows[i];
            Rectangle row = {view.x, view.y + i * rh - scroll.y, view.width - 12, rh - 4};
            if (row.y + row.height < view.y || row.y > view.y + view.height) continue;
            if (p.you) { RoundRect(row, 10, Alpha(col::accent, 0.14f)); RoundRectLines(row, 10, 1, Alpha(col::accent, 0.4f)); }
            else if (i % 2) RoundRect(row, 10, Alpha(col::bg2, 0.6f));
            float ty = row.y + (row.height - 16) / 2 - 1;
            int place = Place(i);
            if (place <= 3) {
                DrawCircleV({cols[0] + 8, row.y + row.height / 2}, 12, Medal(place));
                TextCenter(std::to_string(place), {cols[0] - 4, row.y + row.height / 2 - 12, 24, 24}, 13, OnColor(Medal(place)), F_HEAVY);
            } else {
                Text(std::to_string(place), cols[0] + 3, ty, 15, col::dim, F_BOLD);
            }
            Avatar({cols[1], row.y + 8, 28, 28}, p.name);
            Text(p.name, cols[1] + 40, ty, 16, col::heading, F_BOLD);
            if (p.you) Badge(cols[1] + 52 + Measure(p.name, 16, F_BOLD).x, ty - 2, "You", col::accent, 11.5f);
            Text(std::to_string(p.level), cols[2], ty, 15, col::warn, F_BOLD);
            Text(std::to_string(p.xp), cols[3], ty, 15, col::text, F_BOLD);
            Text(std::to_string(p.solved) + " / " + std::to_string(problems), cols[4], ty, 15, col::text, F_MEDIUM);
            Text(std::to_string(p.lessons) + " / " + std::to_string(lessons), cols[5], ty, 15, col::text, F_MEDIUM);
            Text(QuizPct(p) >= 0 ? std::to_string(QuizPct(p)) + "%" : "-", cols[6], ty, 15, col::text, F_MEDIUM);
            Text(p.bestMock >= 0 ? std::to_string(p.bestMock) + "%" : "-", cols[7], ty, 15, col::text, F_MEDIUM);
        }
        EndClip();
        ScrollBar(view, rows.size() * rh + 4, scroll);
    }
};

} // namespace

Scene* CreateLeaderboardScene() { return new LeaderboardScene(); }
