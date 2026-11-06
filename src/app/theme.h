// ============================================================================
// theme.h  -  The colour themes. Applying one sets the ui::col colours, which
// every widget and scene reads while drawing, so a change shows up instantly.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>
#include <vector>

namespace ui {

struct Theme {
    const char* id;        // saved in the student's progress file (no spaces)
    const char* name;
    const char* tagline;
    bool light;
    Color bg, bg2, panel, panelHi, border;
    Color heading, text, dim, faint;
    Color accent, accent2, warn, danger, success, pointer, heap, stack;
    Color codeBg;
    // syntax highlighting
    Color synText, synType, synKey, synNum, synStr, synCom, synFn, synPre, synPunct;
};

const std::vector<Theme>& Themes();
int ThemeIndex(const std::string& id);    // the default theme (0) if it is unknown
int CurrentTheme();
void ApplyTheme(int index);

// WCAG contrast ratio: 1 (none) to 21 (black on white). Normal text needs 4.5.
float ContrastRatio(Color a, Color b);
// White or near-black, whichever reads better on top of c.
Color OnColor(Color c);
// Bright colours are made for dark backgrounds: darkens them on light themes.
Color Tone(Color c);

} // namespace ui
