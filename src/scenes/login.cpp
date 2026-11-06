// ============================================================================
// login.cpp  -  The first screen: log in or create a student account.
// ============================================================================
#include "app/app.h"
#include "app/auth.h"
#include "app/ui.h"
#include <vector>

using namespace ui;

namespace {

enum { BOX_USER = 9001, BOX_PASSWORD, BOX_CONFIRM };

class LoginScene : public Scene {
public:
    const char* Title() const override { return "Log in"; }
    void Enter(const std::string&) override { Switch(false); }

    void Frame(float) override {
        bool submit = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        if (IsKeyPressed(KEY_TAB)) CycleFocus();

        Rectangle outer = {140, 92, 1000, 668};
        Panel(outer, col::panel, 20);
        Brand({outer.x, outer.y, 420, outer.height});

        float x = outer.x + 480, w = 460;
        float y = outer.y + (outer.height - (registering ? 556.0f : 436.0f)) / 2;
        Text(registering ? "Create your account" : "Welcome back", x, y, 30, col::heading, F_DISPLAY);
        Text(registering ? "Your progress and code are saved to your account." : "Log in to continue where you left off.",
             x, y + 44, 15, col::dim, F_MEDIUM);

        y += 84;
        if (Tab({x, y, w / 2 - 4, 42}, "Log in", !registering) && registering) Switch(false);
        if (Tab({x + w / 2 + 4, y, w / 2 - 4, 42}, "Create account", registering) && !registering) Switch(true);

        y += 62;
        Field(x, y, w, "Username", username, BOX_USER, auth::kMaxUsername, false,
              registering ? "3-24 letters, digits, _ or -" : "");
        y += registering ? 100 : 80;
        Field(x, y, w, "Password", password, BOX_PASSWORD, auth::kMaxPassword, true,
              registering ? "At least 8 characters" : "");
        y += registering ? 100 : 80;
        if (registering) {
            Field(x, y, w, "Confirm password", confirm, BOX_CONFIRM, auth::kMaxPassword, true, "");
            y += 80;
        }

        if (!error.empty()) Text(Ellipsize(error, 14.5f, F_MEDIUM, w), x, y, 14.5f, col::danger, F_MEDIUM);
        y += 28;
        if (Button({x, y, w, 50}, registering ? "Create account" : "Log in", BTN_PRIMARY)) submit = true;
        y += 64;
        Text("Accounts are stored on this computer.", x, y, 13.5f, col::faint, F_MEDIUM);

        if (submit) { if (registering) DoRegister(); else DoLogin(); }
        // Applied after the fields: on the frame of a click, a text box drops
        // the focus if the click was outside it.
        if (pendingFocus) { FocusWidget(pendingFocus); pendingFocus = 0; }
    }

private:
    // The coloured panel on the left. Deep enough in every theme for white text.
    void Brand(Rectangle r) {
        const Color left = Mix(col::accent, BLACK, 0.5f);
        const Color right = Mix(Mix(col::accent, col::accent2, 0.6f), BLACK, 0.62f);
        GradientRoundRect(r, 20, left, right);
        BeginClip(r);
        SoftGlow({r.x + 360, r.y + 40}, 240, WHITE, 0.14f);
        SoftGlow({r.x + 40, r.y + 640}, 260, BLACK, 0.28f);
        DotGrid({r.x + 250, r.y + 20, 150, 110}, 18, Alpha(WHITE, 0.22f));
        EndClip();

        float x = r.x + 40, y = r.y + 44;
        RoundRect({x, y, 56, 56}, 15, WHITE);
        RoundRect({x + 15, y + 15, 26, 26}, 5, left);
        DrawCircleV({x + 28, y + 28}, 6, col::pointer);
        Text("Segfault", x, y + 78, 42, WHITE, F_DISPLAY);
        Text("Learn C & C++ by writing it.", x, y + 132, 17, Alpha(WHITE, 0.85f), F_MEDIUM);

        y += 190;
        const char* points[3][2] = {{"Study", "Lessons with code you can step through"},
                                    {"Practice", "50 problems, compiled and tested for real"},
                                    {"Mock exams", "Timed papers marked by hidden tests"}};
        for (const auto& p : points) {
            DrawCircleV({x + 6, y + 10}, 5, WHITE);
            Text(p[0], x + 24, y, 17, WHITE, F_BOLD);
            Text(p[1], x + 24, y + 23, 13.5f, Alpha(WHITE, 0.82f), F_MEDIUM);
            y += 58;
        }

        // a small code card, always in dark colours so it reads on any theme
        Rectangle card = {x, r.y + r.height - 128, r.width - 80, 92};
        RoundRect(card, 12, Alpha(BLACK, 0.42f));
        RoundRectLines(card, 12, 1, Alpha(WHITE, 0.16f));
        const float fs = 14.5f, cw = CodeCharWidth(fs);
        struct Span { const char* s; Color c; };
        const Color key = {196, 167, 255, 255}, num = {253, 186, 116, 255}, com = {170, 178, 200, 255}, txt = {232, 236, 246, 255};
        const std::vector<std::vector<Span>> code = {
            {{"int", key}, {" value = ", txt}, {"7", num}, {";", txt}},
            {{"int", key}, {"* ptr = &value;", txt}},
            {{"*ptr = ", txt}, {"42", num}, {";  ", txt}, {"// value is now 42", com}}};
        float cy = card.y + 14;
        for (const auto& line : code) {
            float cx = card.x + 16;
            for (const Span& sp : line) {
                std::string part = sp.s;
                Text(part, cx, cy, fs, sp.c, F_MONO);
                cx += part.size() * cw;
            }
            cy += 22;
        }
    }

    bool registering = false;
    std::string username, password, confirm, error;
    int pendingFocus = 0;

    void Switch(bool toRegister) {
        registering = toRegister;
        password.clear();
        confirm.clear();
        error.clear();
        pendingFocus = username.empty() ? BOX_USER : BOX_PASSWORD;
    }

    void CycleFocus() {
        int count = registering ? 3 : 2;
        bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (!AnyTextFocused()) { FocusWidget(back ? BOX_USER + count - 1 : BOX_USER); return; }
        int at = IsFocused(BOX_PASSWORD) ? 1 : IsFocused(BOX_CONFIRM) ? 2 : 0;
        FocusWidget(BOX_USER + (at + (back ? count - 1 : 1)) % count);
    }

    void Field(float x, float y, float w, const char* label, std::string& value, int id, int maxLen, bool masked, const char* hint) {
        Text(label, x, y, 14, col::text, F_BOLD);
        TextBox({x, y + 22, w, 44}, value, id, maxLen, false, "", masked);
        if (hint[0]) Text(hint, x, y + 72, 13, col::faint, F_MEDIUM);
    }

    auth::UserStore Store() const { return auth::UserStore(app.dataDir + "segfault_users"); }

    void DoLogin() {
        if (username.empty() || password.empty()) { error = "Enter your username and password"; return; }
        auth::AuthResult res = Store().Login(username, password);
        password.clear();
        if (!res.ok) { error = res.error; pendingFocus = BOX_PASSWORD; return; }
        username.clear();
        error.clear();
        SignIn(res.username);
        Toast("Welcome back, " + res.username, col::accent2);
    }

    void DoRegister() {
        error = auth::ValidateUsername(username);
        if (error.empty()) error = auth::ValidatePassword(password);
        if (error.empty() && password != confirm) error = "The passwords do not match";
        if (!error.empty()) return;
        auth::UserStore store = Store();
        auth::AuthResult res = store.Register(username, password);
        if (!res.ok) { error = res.error; return; }
        username.clear();
        password.clear();
        confirm.clear();
        // Progress saved before accounts existed goes to the first account made.
        if (res.firstAccount) store.AdoptLegacyData(res.username, app.dataDir);
        SignIn(res.username);
        Toast("Account created. Welcome, " + res.username, col::accent2);
    }
};

} // namespace

Scene* CreateLoginScene() { return new LoginScene(); }
