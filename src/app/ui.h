// ============================================================================
// ui.h  -  Small immediate-mode UI toolkit on top of raylib.
// Everything is laid out on a fixed 1280x800 "virtual canvas" which main.cpp
// scales to the real window, so the app looks the same on any screen.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>
#include <vector>

namespace ui {

constexpr float W = 1280.0f;
constexpr float H = 800.0f;

// ------------------------------------------------------------ colours -----
namespace col {
    extern const Color bg, bg2, panel, panelHi, border, text, dim, faint;
    extern const Color accent, accent2, warn, danger, success, pointer, heap, stack;
    extern const Color codeBg, codeLine;
}
Color Mix(Color a, Color b, float t);
Color Alpha(Color c, float a);

// ------------------------------------------------------------- frame -----
void SetScale(float s);          // called by main when the window size changes
float Scale();
void BeginFrame();               // resets per-frame input state
Vector2 Mouse();                 // mouse in virtual canvas coordinates
bool MouseIn(Rectangle r);
bool Clicked(Rectangle r);       // left click inside r (respects input lock)
bool RightClicked(Rectangle r);
void SetInputEnabled(bool on);   // false while a modal is open
bool InputEnabled();
bool WantsHandCursor();
bool WantsIBeam();
void WantIBeam();
// keyboard focus for text boxes and the code editor
void FocusWidget(int id);
bool IsFocused(int id);

// -------------------------------------------------------------- fonts -----
enum FontKind { F_UI, F_MEDIUM, F_BOLD, F_HEAVY, F_DISPLAY, F_MONO, F_MONOBOLD, F_COUNT };
bool InitFonts(const std::string& resourceDir);
void ClearFontCache();           // after a scale change
void UnloadFonts();
Font GetFont(FontKind kind, float size);

void Text(const std::string& s, float x, float y, float size, Color c, FontKind k = F_UI);
Vector2 Measure(const std::string& s, float size, FontKind k = F_UI);
void TextCenter(const std::string& s, Rectangle r, float size, Color c, FontKind k = F_UI);
void TextRight(const std::string& s, float rightX, float y, float size, Color c, FontKind k = F_UI);
std::vector<std::string> Wrap(const std::string& s, float size, FontKind k, float maxWidth);
std::string Ellipsize(const std::string& s, float size, FontKind k, float maxWidth);

// Rich text: '\n' new line, blank line = paragraph, "- " bullet,
// `code` and **bold** spans. Returns the height used.
float RichText(Rectangle r, const std::string& text, float size, Color c, bool draw = true);

// ------------------------------------------------------------- shapes -----
void RoundRect(Rectangle r, float radius, Color c);
void RoundRectLines(Rectangle r, float radius, float thick, Color c);
void Panel(Rectangle r, Color fill = col::panel, float radius = 12);
void Arrow(Vector2 from, Vector2 to, Color c, float thick = 2.0f, float head = 10.0f);
void CurvedArrow(Vector2 from, Vector2 ctrl, Vector2 to, Color c, float thick = 2.0f, float head = 10.0f);
void DotGrid(Rectangle r, float gap, Color c);
float Badge(float x, float y, const std::string& s, Color c, float size = 13);  // returns width

// ------------------------------------------------------------ widgets -----
enum ButtonKind { BTN_DEFAULT, BTN_PRIMARY, BTN_GHOST, BTN_DANGER, BTN_SUCCESS };
bool Button(Rectangle r, const std::string& label, ButtonKind kind = BTN_DEFAULT, bool enabled = true, float size = 16);
bool IconButton(Rectangle r, const std::string& glyph, bool enabled = true);
bool ListItem(Rectangle r, const std::string& title, const std::string& sub, bool selected, Color accent, bool done);
bool Tab(Rectangle r, const std::string& label, bool active);
// Returns true when Enter is pressed. `id` must be unique per text box.
bool TextBox(Rectangle r, std::string& text, int id, int maxLen = 64, bool numericOnly = false, const std::string& placeholder = "");
void ClearTextFocus();
bool AnyTextFocused();
void ProgressBar(Rectangle r, float t, Color fill, Color back = col::panelHi);

// ---------------------------------------------------------- scrolling -----
struct Scroll {
    float y = 0, target = 0;
    void Reset() { y = target = 0; }
    // Handles the wheel while the mouse is over `view`.
    void Update(Rectangle view, float contentHeight);
    void ScrollTo(float value, Rectangle view, float contentHeight);
};
void BeginClip(Rectangle r);
void EndClip();
void ScrollBar(Rectangle view, float contentHeight, const Scroll& s);

// --------------------------------------------------------------- code -----
float CodeLineHeight(float size);
float CodeCharWidth(float size);
void CodeLine(float x, float y, const std::string& line, float size, float alpha = 1.0f);
// Draws a scrollable code panel. highlight = 0-based line index or -1.
// lineTint (optional): a background colour per line (alpha 0 = none).
void CodeView(Rectangle r, const std::vector<std::string>& lines, Scroll& scroll,
              int highlight = -1, float size = 15, const std::vector<Color>* lineTint = nullptr,
              bool autoScroll = false);
std::vector<std::string> SplitLines(const std::string& s);

// ------------------------------------------------------------- toasts -----
void Toast(const std::string& msg, Color c = col::accent);
void DrawToasts(float dt);

} // namespace ui
