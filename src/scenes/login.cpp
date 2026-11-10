// ============================================================================
// login.cpp  -  The first screen: log in, create a student account, or reset
// a forgotten password with the recovery code given when the account was made.
// ============================================================================
#include "app/app.h"
#include "app/auth.h"
#include "app/ui.h"
#include <vector>

using namespace ui;

namespace {

enum { BOX_USER = 9001, BOX_PASSWORD, BOX_CONFIRM, BOX_CODE };
enum class Mode { LogIn, Register, Forgot };

class LoginScene : public Scene {
public:
    const char* Title() const override { return "Log in"; }
    void Enter(const std::string&) override { shownCode.clear(); SetMode(Mode::LogIn); }

    void Frame(float) override {
        const bool modal = !shownCode.empty();
        bool submit = !modal && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
        if (!modal && IsKeyPressed(KEY_TAB)) CycleFocus();
        if (modal) SetInputEnabled(false);

        Rectangle outer = {140, 92, 1000, 668};
        Panel(outer, col::panel, 20);
        Brand({outer.x, outer.y, 420, outer.height});

        const bool reg = mode == Mode::Register, forgot = mode == Mode::Forgot;
        float x = outer.x + 480, w = 460;
        float y = outer.y + (outer.height - (mode == Mode::LogIn ? 436.0f : 556.0f)) / 2;
        Text(reg ? "Create your account" : forgot ? "Reset your password" : "Welcome back", x, y, 30, col::heading, F_DISPLAY);
        Text(reg ? "Your progress and code are saved to your account."
                 : forgot ? "Use the recovery code you saved when you made your account."
                          : "Log in to continue where you left off.",
             x, y + 44, 15, col::dim, F_MEDIUM);
        y += 84;

        if (!forgot) {
            if (Tab({x, y, w / 2 - 4, 42}, "Log in", !reg) && reg) SetMode(Mode::LogIn);
            if (Tab({x + w / 2 + 4, y, w / 2 - 4, 42}, "Create account", reg) && !reg) SetMode(Mode::Register);
            y += 62;
        }

        Field(x, y, w, "Username", username, BOX_USER, auth::kMaxUsername, false, reg ? "3-24 letters, digits, _ or -" : "");
        y += reg ? 100 : 80;
        if (forgot) {
            Field(x, y, w, "Recovery code", code, BOX_CODE, 20, false, "The code you saved, like ABCD-EFGH-JKMN");
            y += 100;
        }
        Field(x, y, w, forgot ? "New password" : "Password", password, BOX_PASSWORD, auth::kMaxPassword, true,
              reg || forgot ? "At least 8 characters" : "");
        if (mode == Mode::LogIn && Link(x + w, y - 1, "Forgot password?", true)) SetMode(Mode::Forgot);
        y += reg || forgot ? 100 : 80;
        if (reg || forgot) {
            Field(x, y, w, forgot ? "Confirm new password" : "Confirm password", confirm, BOX_CONFIRM, auth::kMaxPassword, true, "");
            y += 80;
        }

        if (!error.empty()) Text(Ellipsize(error, 14.5f, F_MEDIUM, w), x, y, 14.5f, col::danger, F_MEDIUM);
        y += 28;
        if (Button({x, y, w, 50}, reg ? "Create account" : forgot ? "Reset password" : "Log in", BTN_PRIMARY)) submit = true;
        y += 64;
        if (forgot) { if (Link(x, y, "<  Back to log in", false)) SetMode(Mode::LogIn); }
        else Text("Accounts are stored on this computer.", x, y, 13.5f, col::faint, F_MEDIUM);

        if (modal) { SetInputEnabled(true); CodeWindow(); }
        else if (submit) { if (reg) DoRegister(); else if (forgot) DoReset(); else DoLogin(); }
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

    Mode mode = Mode::LogIn;
    std::string username, password, confirm, code, error;
    int pendingFocus = 0;
    // The recovery code on show, and who signs in once it has been saved.
    std::string shownCode, shownFor, shownIntro, shownToast;

    void SetMode(Mode m) {
        mode = m;
        password.clear();
        confirm.clear();
        code.clear();
        error.clear();
        pendingFocus = username.empty() ? BOX_USER : m == Mode::Forgot ? BOX_CODE : BOX_PASSWORD;
    }

    std::vector<int> FocusOrder() const {
        if (mode == Mode::Register) return {BOX_USER, BOX_PASSWORD, BOX_CONFIRM};
        if (mode == Mode::Forgot) return {BOX_USER, BOX_CODE, BOX_PASSWORD, BOX_CONFIRM};
        return {BOX_USER, BOX_PASSWORD};
    }

    void CycleFocus() {
        std::vector<int> order = FocusOrder();
        int n = (int)order.size();
        bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int at = -1;
        for (int i = 0; i < n; i++) if (IsFocused(order[i])) at = i;
        if (at < 0) { FocusWidget(back ? order[n - 1] : order[0]); return; }
        FocusWidget(order[(at + (back ? n - 1 : 1)) % n]);
    }

    void Field(float x, float y, float w, const char* label, std::string& value, int id, int maxLen, bool masked, const char* hint) {
        Text(label, x, y, 14, col::text, F_BOLD);
        TextBox({x, y + 22, w, 44}, value, id, maxLen, false, "", masked);
        if (hint[0]) Text(hint, x, y + 72, 13, col::faint, F_MEDIUM);
    }

    // A text link. rightAlign: x is where the text ends.
    bool Link(float x, float y, const std::string& label, bool rightAlign) {
        Vector2 m = Measure(label, 14, F_BOLD);
        Rectangle r = {rightAlign ? x - m.x : x, y, m.x, m.y + 4};
        bool hover = InputEnabled() && MouseIn(r);
        Text(label, r.x, r.y, 14, hover ? col::heading : col::accent, F_BOLD);
        if (hover) DrawRectangleRec({r.x, r.y + m.y + 1, m.x, 1.5f}, col::heading);
        return Clicked(r);
    }

    auth::UserStore Store() const { return auth::UserStore(app.dataDir + "segfault_users"); }

    void ShowCode(const auth::AuthResult& res, const std::string& intro, const std::string& toast) {
        shownCode = res.recoveryCode;
        shownFor = res.username;
        shownIntro = intro;
        shownToast = toast;
        username.clear();
        password.clear();
        confirm.clear();
        code.clear();
        error.clear();
        ClearTextFocus();
    }

    // The new recovery code, shown once, before the student goes in.
    void CodeWindow() {
        DrawRectangleRec({0, 0, W, H}, Alpha(BLACK, 0.6f));
        Rectangle r = {W / 2 - 300, H / 2 - 180, 600, 360};
        Panel(r, col::panel, 18);
        float x = r.x + 32, w = r.width - 64, y = r.y + 28;
        Text("Save your recovery code", x, y, 26, col::heading, F_DISPLAY);
        y += 46;
        for (const std::string& l : Wrap(shownIntro, 15, F_MEDIUM, w)) { Text(l, x, y, 15, col::dim, F_MEDIUM); y += 22; }
        y += 12;
        Rectangle box = {x, y, w, 76};
        RoundRect(box, 12, col::codeBg);
        RoundRectLines(box, 12, 1.5f, col::accent);
        TextCenter(shownCode, box, 34, col::heading, F_MONOBOLD);
        y += 92;
        const char* note = "Write it down or keep it somewhere safe. If you forget your password, this code is the "
                           "only way back into your account, and it will not be shown again.";
        for (const std::string& l : Wrap(note, 14, F_MEDIUM, w)) { Text(l, x, y, 14, col::text, F_MEDIUM); y += 21; }

        float by = r.y + r.height - 76;
        if (Button({x, by, 180, 46}, "Copy code")) { SetClipboardText(shownCode.c_str()); Toast("Recovery code copied", col::accent2); }
        if (Button({x + w - 240, by, 240, 46}, "I have saved it", BTN_PRIMARY)) {
            std::string who = shownFor, toast = shownToast;
            shownCode.clear();
            SignIn(who);
            Toast(toast, col::accent2);
        }
    }

    void DoLogin() {
        if (username.empty() || password.empty()) { error = "Enter your username and password"; return; }
        auth::UserStore store = Store();
        auth::AuthResult res = store.Login(username, password);
        password.clear();
        if (!res.ok) { error = res.error; pendingFocus = BOX_PASSWORD; return; }
        // Accounts made before recovery codes existed get one now.
        if (!store.HasRecoveryCode(res.username)) {
            auth::AuthResult made = store.CreateRecoveryCode(res.username);
            if (made.ok) {
                ShowCode(made, "Your account did not have a recovery code yet, so here is one. "
                               "You will need it if you ever forget your password.", "Welcome back, " + res.username);
                return;
            }
        }
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
        // Progress saved before accounts existed goes to the first account made.
        if (res.firstAccount) store.AdoptLegacyData(res.username, app.dataDir);
        ShowCode(res, "Your account is ready. This is your recovery code.", "Account created. Welcome, " + res.username);
    }

    void DoReset() {
        if (username.empty()) { error = "Enter your username"; pendingFocus = BOX_USER; return; }
        if (code.empty()) { error = "Enter your recovery code"; pendingFocus = BOX_CODE; return; }
        error = auth::ValidatePassword(password);
        if (error.empty() && password != confirm) error = "The passwords do not match";
        if (!error.empty()) return;
        auth::AuthResult res = Store().ResetPassword(username, code, password);
        password.clear();
        confirm.clear();
        if (!res.ok) { error = res.error; pendingFocus = BOX_CODE; return; }
        ShowCode(res, "Your password has been changed. Your old recovery code no longer works, so here is a new one.",
                 "Password changed. Welcome back, " + res.username);
    }
};

} // namespace

Scene* CreateLoginScene() { return new LoginScene(); }
