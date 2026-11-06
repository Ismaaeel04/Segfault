// ============================================================================
// login.cpp  -  The first screen: log in or create a student account.
// ============================================================================
#include "app/app.h"
#include "app/auth.h"
#include "app/ui.h"

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

        Rectangle r = {W / 2 - 240, 96, 480, registering ? 592.0f : 472.0f};
        Panel(r, col::panel, 18);
        float x = r.x + 36, w = r.width - 72, y = r.y + 30;
        Text(registering ? "Create your account" : "Welcome back", x, y, 28, WHITE, F_DISPLAY);
        Text(registering ? "Your progress and code are saved to your account." : "Log in to continue where you left off.",
             x, y + 42, 15, col::dim, F_MEDIUM);

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
