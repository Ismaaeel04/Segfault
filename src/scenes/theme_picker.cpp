// ============================================================================
// theme_picker.cpp  -  The window for choosing a colour theme. It opens over any
// screen; clicking a theme applies it straight away.
// ============================================================================
#include "app/app.h"
#include "app/theme.h"
#include "app/ui.h"

using namespace ui;

namespace {

// A small drawing of the app in the theme's own colours.
void Preview(Rectangle r, const Theme& t) {
    RoundRect(r, 8, t.bg);
    BeginClip(r);
    DrawRectangleRec({r.x, r.y, r.width, 18}, t.bg2);
    RoundRect({r.x + 8, r.y + 5, 8, 8}, 2, t.accent);
    RoundRect({r.x + 21, r.y + 7, 42, 4}, 2, t.heading);
    RoundRect({r.x + r.width - 40, r.y + 6, 30, 6}, 3, t.accent2);

    Rectangle left = {r.x + 8, r.y + 26, r.width * 0.42f, r.height - 34};
    RoundRect(left, 5, t.panel);
    RoundRect({left.x + 8, left.y + 9, left.width - 30, 5}, 2, t.heading);
    RoundRect({left.x + 8, left.y + 20, left.width - 16, 3}, 1, t.dim);
    RoundRect({left.x + 8, left.y + 27, left.width - 24, 3}, 1, t.dim);
    RoundRect({left.x + 8, left.y + left.height - 22, left.width - 16, 14}, 4, t.accent);

    Rectangle code = {left.x + left.width + 6, left.y, r.x + r.width - 8 - (left.x + left.width + 6), left.height};
    RoundRect(code, 5, t.codeBg);
    const Color syntax[][2] = {{t.synType, t.synFn}, {t.synKey, t.synNum}, {t.synStr, t.synText}, {t.synCom, t.synCom}, {t.synKey, t.synPre}};
    float y = code.y + 9;
    for (const auto& line : syntax) {
        RoundRect({code.x + 8, y, 22, 3.5f}, 1, line[0]);
        RoundRect({code.x + 34, y, code.width - 56, 3.5f}, 1, line[1]);
        y += 10;
    }
    EndClip();
}

} // namespace

void ThemePicker() {
    SetInputEnabled(true);
    DrawRectangleRec(ScreenRect(), Alpha(BLACK, 0.55f));

    const auto& themes = Themes();
    const float cardW = 224, cardH = 196, gap = 16, pad = 28;
    Rectangle r = {W / 2 - 500, H / 2 - 292, 1000, 584};
    Panel(r, col::panel, 18);
    Text("Choose a theme", r.x + pad, r.y + 24, 26, col::heading, F_DISPLAY);
    Text("It changes the whole app and is saved to your account.", r.x + pad, r.y + 60, 15, col::dim, F_MEDIUM);

    for (int i = 0; i < (int)themes.size(); i++) {
        const Theme& t = themes[i];
        Rectangle card = {r.x + pad + (i % 4) * (cardW + gap), r.y + 96 + (i / 4) * (cardH + gap), cardW, cardH};
        bool selected = CurrentTheme() == i;
        bool hover = MouseIn(card);
        RoundRect(card, 12, hover ? col::panelHi : Mix(col::panel, col::panelHi, 0.4f));
        RoundRectLines(card, 12, selected ? 2.5f : 1, selected ? col::accent : hover ? col::faint : col::border);
        Preview({card.x + 10, card.y + 10, card.width - 20, 116}, t);
        Text(t.name, card.x + 14, card.y + 136, 17, col::heading, F_HEAVY);
        Text(t.tagline, card.x + 14, card.y + 161, 13.5f, col::dim, F_MEDIUM);
        if (selected) {
            Vector2 c = {card.x + card.width - 26, card.y + 146};
            DrawCircleV(c, 11, col::accent);
            Color on = OnColor(col::accent);
            DrawLineEx({c.x - 5, c.y}, {c.x - 1.5f, c.y + 4}, 2.4f, on);
            DrawLineEx({c.x - 1.5f, c.y + 4}, {c.x + 5.5f, c.y - 4}, 2.4f, on);
        }
        if (Clicked(card)) SelectTheme(i);
    }

    auto toggleOption = [&](Rectangle tr, const char* title, const char* sub, bool& flag) {
        bool hover = MouseIn(tr);
        RoundRect(tr, 12, hover ? col::panelHi : Mix(col::panel, col::panelHi, 0.5f));
        Text(title, tr.x + 16, tr.y + 7, 15, col::heading, F_BOLD);
        Text(sub, tr.x + 16, tr.y + 27, 12.5f, col::dim, F_MEDIUM);
        Rectangle sw = {tr.x + tr.width - 58, tr.y + 13, 42, 22};
        RoundRect(sw, 11, flag ? col::accent : col::border);
        DrawCircleV({flag ? sw.x + 31 : sw.x + 11, sw.y + 11}, 8, flag ? OnColor(col::accent) : col::faint);
        if (Clicked(tr)) {
            flag = !flag;
            if (!app.user.empty()) app.progress.Save();
            SaveAppSettings();
        }
    };

    // cursor trail & theme cursor toggles
    float ty = r.y + r.height - 64;
    toggleOption({r.x + pad, ty, 330, 48}, "Cursor trail", "a soft line behind the pointer", app.progress.trail);
    toggleOption({r.x + pad + 330 + gap, ty, 330, 48}, "Theme cursor", "off uses the system pointer", app.progress.customCursor);

    bool close = Button({r.x + r.width - pad - 140, r.y + r.height - 62, 140, 44}, "Done", BTN_PRIMARY);
    if (close || IsKeyPressed(KEY_ESCAPE)) app.themePickerOpen = false;
}
