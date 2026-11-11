// ============================================================================
// login.cpp  -  SegFault OS's sign-in screen, the way a real system does it:
// first the lock screen (the time over the wallpaper), then, after any key or
// click, the student's picture and password. From here a student can also
// create an account or reset a forgotten password with the recovery code
// given when the account was made. Signing in opens the desktop.
// ============================================================================
#include "app/app.h"
#include "app/auth.h"
#include "app/avatar.h"
#include "app/sound.h"
#include "app/theme.h"
#include "app/ui.h"
#include <cmath>
#include <ctime>
#include <vector>

using namespace ui;

namespace {

enum { BOX_USER = 9001, BOX_PASSWORD, BOX_CONFIRM, BOX_CODE };
enum class Mode { LogIn, Register, Forgot };

class LoginScene : public Scene {
public:
    const char* Title() const override { return "Sign in"; }
    bool AiAllowed() const override { return false; }
    void Enter(const std::string& param) override {
        shownCode.clear();
        if (username.empty()) username = app.lastUser;
        switching = username.empty();
        SetMode(Mode::LogIn);
        locked = param != "signin" && param != "register";   // --shot hooks skip the lock screen
        if (param == "register") { switching = true; username.clear(); SetMode(Mode::Register); }
        unlockAt = locked ? -1 : -10;
        t = 0;
        ClearTextFocus();
    }

    void Frame(float dt) override {
        t += dt;
        Wallpaper();
        // ---- the lock screen: the time; any key or click slides it away
        if (locked) {
            if (t > 0.2f && (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) { locked = false; unlockAt = t; }
            LockClock(0, 1);
            return;
        }
        float lift = unlockAt < 0 ? 1 : std::min(1.0f, (t - unlockAt) / 0.35f);   // < 0: it opened unlocked
        lift = 1 - (1 - lift) * (1 - lift);
        if (lift < 1) LockClock(-lift * 260, 1 - lift);
        const bool light = Themes()[CurrentTheme()].light;   // light themes: a softer veil, not grey
        DrawRectangleRec(ScreenRect(), light ? Alpha(col::bg, 0.45f * lift) : Alpha(BLACK, 0.38f * lift));
        if (lift < 0.6f) return;
        if (pendingFocus == 0 && !AnyTextFocused() && shownCode.empty() && t - unlockAt < 0.6f)
            pendingFocus = mode == Mode::LogIn && !switching ? BOX_PASSWORD : BOX_USER;

        const bool modal = !shownCode.empty();
        bool submit = !modal && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
        if (!modal && IsKeyPressed(KEY_TAB)) CycleFocus();
        if (!modal && IsKeyPressed(KEY_ESCAPE) && !AnyTextFocused()) {   // back to the lock screen
            if (mode != Mode::LogIn) SetMode(Mode::LogIn);
            else { locked = true; unlockAt = -1; }
            return;
        }
        if (modal) SetInputEnabled(false);

        if (mode == Mode::LogIn) SignInPanel(submit);
        else FormCard(submit);
        BottomBar();

        if (modal) { SetInputEnabled(true); CodeWindow(); }
        if (quittingAt >= 0) {   // shutting down from here: fade to black while the sound plays
            float k = std::min(1.0f, (t - quittingAt) / 1.1f);
            DrawRectangleRec(ScreenRect(), Alpha(BLACK, k));
            if (t - quittingAt > 1.4f) app.quitRequested = true;
        }
        // Applied after the fields: on the frame of a click, a text box drops
        // the focus if the click was outside it.
        if (pendingFocus) { FocusWidget(pendingFocus); pendingFocus = 0; }
    }

private:
    Mode mode = Mode::LogIn;
    std::string username, password, confirm, code, error;
    int pendingFocus = 0;
    bool locked = true, switching = false;
    float t = 0, unlockAt = -1, quittingAt = -1;
    // The recovery code on show, and who signs in once it has been saved.
    std::string shownCode, shownFor, shownIntro, shownToast;

    // ------------------------------------------------------------ drawing --
    // The desktop's wallpaper, so signing in feels like the same system.
    void Wallpaper() {
        Rectangle scr = ScreenRect();
        DrawWallpaperMark({W / 2, H / 2 - 40}, 173, 0.05f);   // first: the glows tint it like the rest
        SoftGlow({W / 2, H / 2 - 30}, 460, col::accent, 0.14f);
        SoftGlow({W / 2 + 260, H / 2 + 140}, 320, col::accent2, 0.09f);
        DotGrid(scr, 28, Alpha(col::border, 0.35f));
    }

    // The big clock and the date. dy moves it (it slides up when unlocking).
    void LockClock(float dy, float alpha) {
        std::time_t now = std::time(nullptr);
        std::tm tm = *std::localtime(&now);
        char hm[8];
        std::snprintf(hm, sizeof(hm), "%02d:%02d", tm.tm_hour, tm.tm_min);
        static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        static const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                                       "October", "November", "December"};
        std::string date = std::string(days[tm.tm_wday]) + ", " + std::to_string(tm.tm_mday) + " " + months[tm.tm_mon];
        TextCenter(hm, {0, 196 + dy, W, 130}, 116, Alpha(col::heading, alpha), F_DISPLAY);
        TextCenter(date, {0, 334 + dy, W, 34}, 26, Alpha(col::text, alpha), F_MEDIUM);
        // a hint, gently bobbing
        float bob = std::sin(t * 2.4f) * 4;
        Vector2 c = {W / 2, H - 112 + bob + dy};
        DrawLineEx({c.x - 10, c.y + 5}, {c.x, c.y - 5}, 2.4f, Alpha(col::dim, alpha));
        DrawLineEx({c.x, c.y - 5}, {c.x + 10, c.y + 5}, 2.4f, Alpha(col::dim, alpha));
        TextCenter("Press any key or click to sign in", {0, H - 96 + dy, W, 22}, 15, Alpha(col::dim, alpha), F_MEDIUM);
        DrawLogo({W / 2 - 22, 120 + dy, 44, 44}, t, 0.2f);
    }

    // The picture, the name and the password, in the middle of the screen.
    void SignInPanel(bool submit) {
        const float cx = W / 2, w = 340;
        float y = 150;
        // the avatar: the student's picture if they have one, or a person
        Rectangle av = {cx - 64, y, 128, 128};
        SoftGlow({cx, y + 64}, 120, col::accent, 0.15f);
        if (!switching && !username.empty()) avatar::Draw(av, username, 0);
        else {
            RoundRect(av, 64, Mix(col::panel, col::panelHi, 0.6f));
            DrawCircleV({cx, y + 50}, 22, col::dim);
            DrawCircleSector({cx, y + 116}, 40, 180, 360, 32, col::dim);
        }
        RoundRectLines(av, 64, 2, Alpha(col::heading, 0.25f));
        y += 150;

        if (!switching && !username.empty()) {
            TextCenter(username, {0, y, W, 40}, 32, col::heading, F_DISPLAY);
            y += 58;
        } else {
            TextCenter("Sign in", {0, y, W, 40}, 32, col::heading, F_DISPLAY);
            y += 54;
            Text("Username", cx - w / 2, y, 13.5f, col::dim, F_BOLD);
            TextBox({cx - w / 2, y + 20, w, 44}, username, BOX_USER, auth::kMaxUsername, false, "your username");
            y += 76;
        }
        // the password, with the arrow that signs in attached to it
        Rectangle pw = {cx - w / 2, y, w - 50, 46};
        TextBox(pw, password, BOX_PASSWORD, auth::kMaxPassword, false, "Password", true);
        Rectangle go = {pw.x + pw.width + 6, y, 44, 46};
        if (Button(go, "", BTN_PRIMARY, true, 14)) submit = true;
        Vector2 a = {go.x + 22, go.y + 23};
        Color on = OnColor(col::accent);
        DrawLineEx({a.x - 8, a.y}, {a.x + 7, a.y}, 2.4f, on);
        DrawLineEx({a.x + 1, a.y - 6}, {a.x + 7, a.y}, 2.4f, on);
        DrawLineEx({a.x + 1, a.y + 6}, {a.x + 7, a.y}, 2.4f, on);
        y += 58;
        if (!error.empty()) {
            for (const std::string& l : Wrap(error, 14, F_MEDIUM, w)) { TextCenter(l, {0, y, W, 20}, 14, col::danger, F_MEDIUM); y += 20; }
            y += 6;
        }
        // the links
        float lx = cx - w / 2;
        if (Link(lx, y, "Forgot password?", false)) SetMode(Mode::Forgot);
        if (!switching && !username.empty()) {
            if (Link(cx + w / 2, y, "Not you? Switch user", true)) { switching = true; username.clear(); SetMode(Mode::LogIn); }
        } else if (!app.lastUser.empty() && Link(cx + w / 2, y, "Back to " + app.lastUser, true)) {
            switching = false; username = app.lastUser; SetMode(Mode::LogIn);
        }
        y += 44;
        // a new student
        Rectangle make = {cx - w / 2, y, w, 44};
        if (Button(make, "Create an account", BTN_GHOST, true, 14.5f)) { switching = true; username.clear(); SetMode(Mode::Register); }

        if (submit && shownCode.empty()) DoLogin();
    }

    // Create an account / reset the password: a glass card over the wallpaper.
    void FormCard(bool submit) {
        const bool reg = mode == Mode::Register;
        const float w = 440;
        float h = reg ? 516 : 610;
        Rectangle card = {W / 2 - w / 2 - 36, H / 2 - h / 2 - 20, w + 72, h};
        Shadow(card, 22, 2);
        RoundRect(card, 22, Alpha(col::panel, 0.94f));
        RoundRectLines(card, 22, 1, col::border);
        float x = card.x + 36, y = card.y + 30;
        Text(reg ? "Create your account" : "Reset your password", x, y, 28, col::heading, F_DISPLAY);
        y += 40;
        Text(reg ? "Your progress, files and code are saved to it." : "Use the recovery code you saved when you made your account.",
             x, y, 14, col::dim, F_MEDIUM);
        y += 40;
        Field(x, y, w, "Username", username, BOX_USER, auth::kMaxUsername, false, reg ? "3-24 letters, digits, _ or -" : "");
        y += reg ? 96 : 78;
        if (!reg) {
            Field(x, y, w, "Recovery code", code, BOX_CODE, 20, false, "The code you saved, like ABCD-EFGH-JKMN");
            y += 96;
        }
        Field(x, y, w, reg ? "Password" : "New password", password, BOX_PASSWORD, auth::kMaxPassword, true, "At least 8 characters");
        y += 96;
        Field(x, y, w, reg ? "Confirm password" : "Confirm new password", confirm, BOX_CONFIRM, auth::kMaxPassword, true, "");
        y += 78;
        if (!error.empty()) Text(Ellipsize(error, 14, F_MEDIUM, w), x, y, 14, col::danger, F_MEDIUM);
        y += 26;
        if (Button({x, y, w, 46}, reg ? "Create account" : "Reset password", BTN_PRIMARY)) submit = true;
        y += 58;
        if (Link(x, y, "<  Back to sign in", false)) { switching = app.lastUser.empty(); username = app.lastUser; SetMode(Mode::LogIn); }
        if (submit && shownCode.empty()) { if (reg) DoRegister(); else DoReset(); }
    }

    // SegFault OS on the left; theme and power on the right.
    void BottomBar() {
        Rectangle scr = ScreenRect();
        float L = scr.x, R = scr.x + scr.width;
        DrawLogo({L + 24, H - 58, 34, 34}, t, 0);
        Text("SegFault OS", L + 68, H - 50, 15, col::dim, F_BOLD);
        Rectangle power = {R - 58, H - 62, 40, 40}, theme = {R - 108, H - 62, 40, 40};
        bool ph = MouseIn(power), th = MouseIn(theme);
        if (ph || th) WantHand();
        if (ph) RoundRect(power, 10, Alpha(col::danger, 0.18f));
        if (th) RoundRect(theme, 10, Alpha(col::heading, 0.10f));
        Vector2 c = {power.x + 20, power.y + 21};
        DrawRing(c, 7, 9, -50, 230, 24, ph ? col::danger : col::text);   // power symbol, gap at the top
        DrawLineEx({c.x, c.y - 11}, {c.x, c.y - 2}, 2.2f, ph ? col::danger : col::text);
        const Color dots[3] = {col::accent, col::accent2, col::pointer};
        for (int i = 0; i < 3; i++) DrawCircleV({theme.x + 13 + i * 7.0f, theme.y + 20}, 4, dots[i]);
        if (ph && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && quittingAt < 0) { quittingAt = t; snd::Play(snd::SHUTDOWN); }
        if (th && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) app.themePickerOpen = true;
        if (ph) TextRight("Shut down", power.x - 6, power.y + 12, 13, col::text, F_MEDIUM);
        if (th) TextRight("Theme", theme.x - 6, theme.y + 12, 13, col::text, F_MEDIUM);
    }

    // ------------------------------------------------------------ helpers --
    void SetMode(Mode m) {
        mode = m;
        password.clear();
        confirm.clear();
        code.clear();
        error.clear();
        pendingFocus = username.empty() ? BOX_USER : m == Mode::Forgot ? BOX_CODE : BOX_PASSWORD;
        if (m == Mode::LogIn && (switching || username.empty())) pendingFocus = BOX_USER;
    }

    std::vector<int> FocusOrder() const {
        if (mode == Mode::Register) return {BOX_USER, BOX_PASSWORD, BOX_CONFIRM};
        if (mode == Mode::Forgot) return {BOX_USER, BOX_CODE, BOX_PASSWORD, BOX_CONFIRM};
        if (!switching && !username.empty()) return {BOX_PASSWORD};
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
        Text(label, x, y, 13.5f, col::text, F_BOLD);
        TextBox({x, y + 20, w, 44}, value, id, maxLen, false, "", masked);
        if (hint[0]) Text(hint, x, y + 70, 12.5f, col::faint, F_MEDIUM);
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
        password.clear();
        confirm.clear();
        code.clear();
        error.clear();
        ClearTextFocus();
    }

    // The new recovery code, shown once, before the student goes in.
    void CodeWindow() {
        DrawRectangleRec(ScreenRect(), Alpha(BLACK, 0.6f));
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
            username = who;
            switching = false;
            mode = Mode::LogIn;
            snd::Play(snd::LOGON);
            SignIn(who);
            Toast(toast, col::accent2);
        }
    }

    // ---------------------------------------------------------- accounts --
    void DoLogin() {
        if (username.empty() || password.empty()) { error = username.empty() ? "Enter your username" : "Enter your password"; return; }
        auth::UserStore store = Store();
        auth::AuthResult res = store.Login(username, password);
        password.clear();
        if (!res.ok) { error = res.error; pendingFocus = BOX_PASSWORD; snd::Play(snd::ERROR_SOUND); return; }
        // Accounts made before recovery codes existed get one now.
        if (!store.HasRecoveryCode(res.username)) {
            auth::AuthResult made = store.CreateRecoveryCode(res.username);
            if (made.ok) {
                ShowCode(made, "Your account did not have a recovery code yet, so here is one. "
                               "You will need it if you ever forget your password.", "Welcome back, " + res.username);
                return;
            }
        }
        error.clear();
        username = res.username;
        switching = false;
        snd::Play(snd::LOGON);
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
