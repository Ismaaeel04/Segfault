// ============================================================================
// editor.h  -  A small code editor widget (raylib, immediate mode).
// Syntax highlighting, line numbers, selection, clipboard, undo/redo,
// auto-indent, Tab / Shift+Tab, Ctrl+/ comments, error markers.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class CodeEditor {
public:
    void SetText(const std::string& text);
    std::string Text() const;
    void SetErrorLines(const std::vector<int>& lines) { errorLines = lines; }   // 1-based
    void Frame(Rectangle r, int id, bool readOnly = false);
    bool TakeChanged() { bool c = changed; changed = false; return c; }
    void Focus(int id);
    float fontSize = 16;

private:
    struct Snapshot { std::vector<std::string> lines; int cy, cx; };
    std::vector<std::string> lines{""};
    int cy = 0, cx = 0;          // cursor
    int ay = 0, ax = 0;          // selection anchor
    bool hasSel = false;
    float scrollY = 0, scrollX = 0, targetY = 0;
    std::vector<Snapshot> undo, redo;
    double lastEdit = 0;
    int lastEditKind = 0;
    bool dragging = false;
    double lastClick = 0;
    int lastClickY = -1, lastClickX = -1;
    float blink = 0;
    bool changed = false;
    std::vector<int> errorLines;
    int preferredX = -1;

    void PushUndo(int kind);
    void Restore(Snapshot& s);
    bool SelRange(int& y0, int& x0, int& y1, int& x1) const;
    std::string SelectedText() const;
    void DeleteSelection();
    void InsertText(const std::string& t);
    void InsertChar(char c);
    void Newline();
    void Backspace();
    void DeleteForward();
    void MoveTo(int y, int x, bool select);
    void IndentLines(bool out);
    void ToggleComment();
    void Clamp();
    void HandleKeys(bool readOnly, float viewH);
    void EnsureVisible(Rectangle text, float lineH, float charW);
};
