// ============================================================================
// profile.cpp  -  The student's profile: picture, avatar colour and stats.
// A picture can be chosen in the "open file" window, dropped onto the app, or
// pasted with Ctrl+V.
// ============================================================================
#include "app/app.h"
#include "app/avatar.h"
#include "app/files.h"
#include "app/ui.h"
#include "content/content.h"
#include <cmath>

using namespace ui;

namespace {

class ProfileScene : public Scene {
public:
    const char* Title() const override { return "Profile"; }
    void Enter(const std::string&) override {
        message.clear();
        Refresh();
    }

    void Frame(float) override {
        // a picture dropped onto the window, or pasted
        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0) Result(avatar::SetPictureFromFile(app.user, dropped.paths[0]));
            UnloadDroppedFiles(dropped);
        }
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (InputEnabled() && ctrl && IsKeyPressed(KEY_V)) {
            Image clip = GetClipboardImage();
            if (clip.data) Result(avatar::SetPicture(app.user, clip));
            else { error = true; message = "There is no picture on the clipboard. Copy one first."; }
        }

        Text("Your profile", 32, 86, 30, col::heading, F_DISPLAY);
        Text("How you appear on the home screen and on the leaderboard.", 34, 126, 16, col::dim, F_MEDIUM);
        Picture({32, 160, 540, 624});
        Details({588, 160, 660, 624});
    }

private:
    std::string message;
    bool error = false;
    LeaderRow me;
    int place = 0, students = 0;

    void Refresh() {
        std::vector<LeaderRow> rows = LoadLeaderboard();
        students = (int)rows.size();
        for (const LeaderRow& r : rows) if (r.you) { me = r; place = 1; for (const LeaderRow& o : rows) place += o.xp > r.xp; }
    }

    void Result(const std::string& err) {
        error = !err.empty();
        message = error ? err : "Picture updated";
        if (!error) Toast("Profile picture updated", col::accent2);
    }

    void Picture(Rectangle r) {
        Panel(r, col::panel, 18);
        PanelTint(r, 18, col::accent, 0.10f);
        Rectangle av = {r.x + r.width / 2 - 90, r.y + 40, 180, 180};
        avatar::Draw(av, app.user, app.progress.avatarColour);
        TextCenter(app.user, {r.x, r.y + 236, r.width, 34}, 28, col::heading, F_DISPLAY);
        std::string line = "Level " + std::to_string(app.progress.Level()) + "  -  " + std::to_string(app.progress.xp) + " XP";
        if (place) line += "  -  #" + std::to_string(place) + " of " + std::to_string(students);
        TextCenter(line, {r.x, r.y + 276, r.width, 22}, 15, col::dim, F_MEDIUM);

        bool has = avatar::HasPicture(app.user);
        float bx = r.x + 40, bw = (r.width - 96) / 2, by = r.y + 324;
        if (Button({bx, by, bw, 46}, has ? "Change picture..." : "Choose a picture...", BTN_PRIMARY)) {
            std::string path;
            if (files::PickImage(GetWindowHandle(), path)) Result(avatar::SetPictureFromFile(app.user, path));
        }
        if (Button({bx + bw + 16, by, bw, 46}, "Remove picture", BTN_DANGER, has)) {
            avatar::RemovePicture(app.user);
            error = false;
            message = "Picture removed. Your initial and colour are shown instead.";
        }

        // where pictures can be dropped
        Rectangle drop = {r.x + 40, r.y + 398, r.width - 80, 140};
        RoundRect(drop, 14, Alpha(col::accent, 0.06f));
        for (float x = drop.x + 14; x < drop.x + drop.width - 14; x += 14) {
            DrawRectangleRec({x, drop.y, 7, 1.5f}, Alpha(col::accent, 0.5f));
            DrawRectangleRec({x, drop.y + drop.height - 1.5f, 7, 1.5f}, Alpha(col::accent, 0.5f));
        }
        for (float y = drop.y + 14; y < drop.y + drop.height - 14; y += 14) {
            DrawRectangleRec({drop.x, y, 1.5f, 7}, Alpha(col::accent, 0.5f));
            DrawRectangleRec({drop.x + drop.width - 1.5f, y, 1.5f, 7}, Alpha(col::accent, 0.5f));
        }
        // arrow into a tray
        float cx = drop.x + drop.width / 2, cy = drop.y + 40;
        DrawRectangleRec({cx - 2, cy - 18, 4, 18}, col::accent);
        DrawTriangle({cx - 9, cy - 2}, {cx, cy + 8}, {cx + 9, cy - 2}, col::accent);
        DrawRectangleRec({cx - 16, cy + 12, 32, 3}, col::accent);
        TextCenter("Drop a picture here", {drop.x, drop.y + 64, drop.width, 24}, 17, col::heading, F_BOLD);
        TextCenter("PNG, JPG, BMP or GIF, up to 20 MB  -  or press Ctrl+V to paste one", {drop.x, drop.y + 92, drop.width, 20},
                   13.5f, col::dim, F_MEDIUM);

        if (!message.empty())
            TextCenter(message, {r.x, r.y + 556, r.width, 22}, 14.5f, error ? col::danger : col::success, F_MEDIUM);
        TextCenter("Pictures are cropped to a square from the middle.", {r.x, r.y + 588, r.width, 20}, 13, col::faint, F_MEDIUM);
    }

    void Details(Rectangle r) {
        Panel(r, col::panel, 18);
        float x = r.x + 32, y = r.y + 26;
        Text("Avatar colour", x, y, 20, col::heading, F_HEAVY);
        Text("Shown with your initial when you have no picture.", x, y + 28, 14.5f, col::dim, F_MEDIUM);
        y += 66;
        const float d = 74, gap = (r.width - 64 - 5 * d) / 4;
        for (int i = 0; i < avatar::kColourCount; i++) {
            Rectangle c = {x + (i % 5) * (d + gap), y + (i / 5) * (d + 40), d, d};
            bool sel = app.progress.avatarColour == i || (i == 0 && app.progress.avatarColour < 0);
            bool hover = InputEnabled() && MouseIn(c);
            if (sel || hover) RoundRectLines({c.x - 5, c.y - 5, c.width + 10, c.height + 10}, d / 2 + 5, sel ? 2.5f : 1.5f,
                                             sel ? col::accent : col::faint);
            avatar::DrawInitial(c, app.user, i);
            TextCenter(avatar::ColourName(i), {c.x - 10, c.y + d + 6, d + 20, 18}, 13, sel ? col::heading : col::dim, F_BOLD);
            if (Clicked(c)) {
                app.progress.avatarColour = i;
                app.progress.Save();
            }
        }

        y += 2 * (d + 40) + 14;
        DrawRectangleRec({x, y, r.width - 64, 1}, col::border);
        y += 22;
        Text("Your stats", x, y, 20, col::heading, F_HEAVY);
        y += 40;
        const int problems = (int)Problems().size(), lessons = (int)Lessons().size();
        const std::string stats[6][2] = {
            {std::to_string(me.level), "level"},
            {std::to_string(me.xp), "XP"},
            {std::to_string(me.solved) + " / " + std::to_string(problems), "problems solved"},
            {std::to_string(me.lessons) + " / " + std::to_string(lessons), "lessons done"},
            {me.answered ? std::to_string((int)std::lround(100.0 * me.correct / me.answered)) + "%" : "-", "quiz accuracy"},
            {me.bestMock >= 0 ? std::to_string(me.bestMock) + "%" : "-", "best mock exam"}};
        const float tw = (r.width - 64 - 2 * 16) / 3, th = 94;
        for (int i = 0; i < 6; i++) {
            Rectangle t = {x + (i % 3) * (tw + 16), y + (i / 3) * (th + 14), tw, th};
            RoundRect(t, 12, Mix(col::panel, col::panelHi, 0.6f));
            Text(stats[i][0], t.x + 18, t.y + 16, 26, i == 0 ? col::warn : col::heading, F_DISPLAY);
            Text(stats[i][1], t.x + 18, t.y + 56, 13.5f, col::dim, F_MEDIUM);
        }
    }
};

} // namespace

Scene* CreateProfileScene() { return new ProfileScene(); }
