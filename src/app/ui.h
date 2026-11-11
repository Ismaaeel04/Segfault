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
    // Set by ApplyTheme (theme.h). Read them while drawing, never keep a copy.
    extern Color bg, bg2, panel, panelHi, border, heading, text, dim, faint;
    extern Color accent, accent2, warn, danger, success, pointer, heap, stack;
    extern Color codeBg, codeLine, shadow;
    extern Color synText, synType, synKey, synNum, synStr, synCom, synFn, synPre, synPunct;
}
Color Mix(Color a, Color b, float t);
Color Alpha(Color c, float a);

// ------------------------------------------------------------- frame -----
void SetScale(float s);          // called by main when the window size changes
float Scale();
// On screens wider than 16:10 the canvas grows sideways: ExtraX() more units
// on each side, so x runs from -ExtraX() to W + ExtraX(). Screens keep their
// layout in 0..W (centred); backgrounds and the desktop use the whole width.
void SetExtraX(float units);
float ExtraX();
Rectangle ScreenRect();          // the whole visible canvas
void BeginFrame();               // resets per-frame input state
Vector2 Mouse();                 // mouse in virtual canvas coordinates
bool MouseIn(Rectangle r);
bool Clicked(Rectangle r);       // left click inside r (respects input lock)
bool RightClicked(Rectangle r);
void SetInputEnabled(bool on);   // false while a modal is open
bool InputEnabled();
bool WantsHandCursor();
void WantHand();                 // show the pointing hand this frame
bool WantsIBeam();
void WantIBeam();
// keyboard focus for text boxes and the code editor
void FocusWidget(int id);
bool IsFocused(int id);

// -------------------------------------------------------------- fonts -----
enum FontKind { F_UI, F_MEDIUM, F_BOLD, F_HEAVY, F_DISPLAY, F_MONO, F_MONOBOLD, F_COUNT };
bool InitFonts(const std::string& resourceDir);
void ClearFontCache();           // after a scale change
// Makes one of the usual font sizes ahead of time; main calls it every frame,
// so screens don't stall the first time they use a size. False when all are made.
bool WarmFonts();
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
void Shadow(Rectangle r, float radius, float strength = 1.0f);
// Left-to-right gradient. The colours should be opaque.
void GradientRoundRect(Rectangle r, float radius, Color left, Color right);
// Top-to-bottom gradient; colours may be see-through.
void GradientRoundRectV(Rectangle r, float radius, Color top, Color bottom);
// A wash of colour at the top of a panel that fades out downwards. It stays
// inside the rounded corners (a clipped glow would spill into them).
void PanelTint(Rectangle r, float radius, Color c, float strength);
void SoftGlow(Vector2 center, float radius, Color c, float strength);
// The Segfault logo: {*}, a block of C code with a pointer star inside. The
// star turns slowly; hot (0-1) makes it glow and spin faster.
void DrawLogo(Rectangle r, float time, float hot = 0);
// SegFault AI's logo, a sibling of the one above: { } around an AI sparkle,
// on a dark tile. The sparkle breathes; hot (0-1) makes it glow.
void DrawAiLogo(Rectangle r, float time, float hot = 0);
// The sparkle alone: a four-point star with curved sides.
void Sparkle(Vector2 c, float radius, Color col);
void Backdrop(float time);       // the soft background behind every screen
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
// Ctrl+V pastes; Ctrl+C / Ctrl+X copy or cut the whole text (not when masked).
// masked: draws every character as * (for passwords).
bool TextBox(Rectangle r, std::string& text, int id, int maxLen = 64, bool numericOnly = false, const std::string& placeholder = "", bool masked = false);
void ClearTextFocus();
bool AnyTextFocused();
void ProgressBar(Rectangle r, float t, Color fill, Color back = col::panelHi);

// ---------------------------------------------------------- scrolling -----
struct Scroll {
    float y = 0, target = 0;
    bool dragging = false;     // the scrollbar thumb is being dragged
    float grab = 0;            // where on the thumb it was grabbed
    int lastAuto = -2;         // CodeView: the line it last scrolled to by itself
    void Reset() { y = target = 0; dragging = false; lastAuto = -2; }
    // Handles the wheel and the scrollbar. Call it before drawing the content,
    // so a click on the bar doesn't also click what is underneath it.
    void Update(Rectangle view, float contentHeight);
    void ScrollTo(float value, Rectangle view, float contentHeight);
};
void BeginClip(Rectangle r);
void EndClip();
// Draws the bar; Scroll::Update makes it work.
void ScrollBar(Rectangle view, float contentHeight, const Scroll& s);
// The same bar for widgets that keep their own offsets (the code editor).
// Returns true while it is using the mouse: the widget should ignore that click.
bool ScrollBarInput(Rectangle view, float contentHeight, float& y, float& target, bool& dragging, float& grab);
void DrawScrollBar(Rectangle view, float contentHeight, float y, bool dragging);
bool OverScrollBar(Rectangle view, float contentHeight);

// --------------------------------------------------------------- code -----
float CodeLineHeight(float size);
float CodeCharWidth(float size);
void CodeLine(float x, float y, const std::string& line, float size, float alpha = 1.0f);
// Draws a scrollable code panel, with a Copy button while the mouse is over it. highlight = 0-based line index or -1.
// lineTint (optional): a background colour per line (alpha 0 = none).
void CodeView(Rectangle r, const std::vector<std::string>& lines, Scroll& scroll,
              int highlight = -1, float size = 15, const std::vector<Color>* lineTint = nullptr,
              bool autoScroll = false);
std::vector<std::string> SplitLines(const std::string& s);

// ------------------------------------------------------------- toasts -----
void Toast(const std::string& msg, Color c = col::accent);
void DrawToasts(float dt);

} // namespace ui
