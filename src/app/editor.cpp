#include "editor.h"
#include "ui.h"
#include <algorithm>
#include <cmath>

using namespace ui;

static int LeadingSpaces(const std::string& s) {
    int n = 0;
    while (n < (int)s.size() && s[n] == ' ') n++;
    return n;
}
static bool IsWordChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

void CodeEditor::SetText(const std::string& text) {
    lines.clear();
    std::string cur;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else if (c == '\t') cur += "    ";
        else cur += c;
    }
    lines.push_back(cur);
    cy = cx = ay = ax = 0;
    hasSel = false;
    scrollY = targetY = scrollX = 0;
    undo.clear(); redo.clear();
    errorLines.clear();
    changed = false;
}

std::string CodeEditor::Text() const {
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) { s += lines[i]; if (i + 1 < lines.size()) s += '\n'; }
    return s;
}

void CodeEditor::Focus(int id) { FocusWidget(id); }

void CodeEditor::Clamp() {
    if (lines.empty()) lines.push_back("");
    cy = std::clamp(cy, 0, (int)lines.size() - 1);
    cx = std::clamp(cx, 0, (int)lines[cy].size());
    ay = std::clamp(ay, 0, (int)lines.size() - 1);
    ax = std::clamp(ax, 0, (int)lines[ay].size());
}

void CodeEditor::PushUndo(int kind) {
    double now = GetTime();
    // group consecutive typing into one undo step
    if (kind != 0 && kind == lastEditKind && now - lastEdit < 1.0) { lastEdit = now; return; }
    undo.push_back({lines, cy, cx});
    if (undo.size() > 300) undo.erase(undo.begin());
    redo.clear();
    lastEdit = now;
    lastEditKind = kind;
    changed = true;
    errorLines.clear();
}

void CodeEditor::Restore(Snapshot& s) {
    lines = s.lines; cy = s.cy; cx = s.cx; hasSel = false;
    Clamp();
    changed = true;
}

bool CodeEditor::SelRange(int& y0, int& x0, int& y1, int& x1) const {
    if (!hasSel || (ay == cy && ax == cx)) return false;
    if (ay < cy || (ay == cy && ax < cx)) { y0 = ay; x0 = ax; y1 = cy; x1 = cx; }
    else { y0 = cy; x0 = cx; y1 = ay; x1 = ax; }
    return true;
}

std::string CodeEditor::SelectedText() const {
    int y0, x0, y1, x1;
    if (!SelRange(y0, x0, y1, x1)) return "";
    if (y0 == y1) return lines[y0].substr(x0, x1 - x0);
    std::string s = lines[y0].substr(x0) + "\n";
    for (int y = y0 + 1; y < y1; y++) s += lines[y] + "\n";
    return s + lines[y1].substr(0, x1);
}

void CodeEditor::DeleteSelection() {
    int y0, x0, y1, x1;
    if (!SelRange(y0, x0, y1, x1)) { hasSel = false; return; }
    std::string tail = lines[y1].substr(x1);
    lines[y0] = lines[y0].substr(0, x0) + tail;
    lines.erase(lines.begin() + y0 + 1, lines.begin() + y1 + 1);
    cy = y0; cx = x0;
    hasSel = false;
}

void CodeEditor::InsertText(const std::string& raw) {
    DeleteSelection();
    std::string t;
    for (char c : raw) { if (c == '\r') continue; if (c == '\t') t += "    "; else if (c == '\n' || (c >= 32 && c < 127)) t += c; }
    std::string tail = lines[cy].substr(cx);
    lines[cy] = lines[cy].substr(0, cx);
    size_t start = 0;
    while (true) {
        size_t nl = t.find('\n', start);
        std::string part = t.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        lines[cy] += part;
        cx = (int)lines[cy].size();
        if (nl == std::string::npos) break;
        lines.insert(lines.begin() + cy + 1, "");
        cy++;
        start = nl + 1;
    }
    lines[cy] += tail;
}

void CodeEditor::InsertChar(char c) {
    PushUndo(1);
    DeleteSelection();
    std::string& l = lines[cy];
    // typing '}' on an indent-only line: dedent one level
    if (c == '}' && LeadingSpaces(l) == (int)l.size() && cx == (int)l.size() && cx >= 4) {
        l.erase(0, 4);
        cx -= 4;
    }
    l.insert(l.begin() + cx, c);
    cx++;
}

void CodeEditor::Newline() {
    PushUndo(2);
    DeleteSelection();
    std::string& l = lines[cy];
    int indent = LeadingSpaces(l);
    std::string before = l.substr(0, cx), after = l.substr(cx);
    std::string trimmed = before;
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    bool opens = !trimmed.empty() && trimmed.back() == '{';
    size_t a0 = after.find_first_not_of(' ');
    bool closesNext = a0 != std::string::npos && after[a0] == '}';
    if (opens) indent += 4;
    if (a0 != std::string::npos) after = after.substr(a0); else after.clear();
    l = before;
    if (opens && closesNext) {
        // "{|}" -> put the brace on its own line below
        lines.insert(lines.begin() + cy + 1, std::string(indent, ' '));
        lines.insert(lines.begin() + cy + 2, std::string(std::max(0, indent - 4), ' ') + after);
        cy++;
        cx = indent;
        return;
    }
    lines.insert(lines.begin() + cy + 1, std::string(indent, ' ') + after);
    cy++;
    cx = indent;
}

void CodeEditor::Backspace() {
    PushUndo(3);
    if (hasSel && (ay != cy || ax != cx)) { DeleteSelection(); return; }
    hasSel = false;
    std::string& l = lines[cy];
    if (cx > 0) {
        int lead = LeadingSpaces(l);
        int n = 1;
        if (cx <= lead && cx % 4 == 0) n = 4;     // remove one indent level
        else if (cx <= lead) n = cx % 4;
        n = std::max(1, std::min(n, cx));
        l.erase(cx - n, n);
        cx -= n;
    } else if (cy > 0) {
        cx = (int)lines[cy - 1].size();
        lines[cy - 1] += l;
        lines.erase(lines.begin() + cy);
        cy--;
    }
}

void CodeEditor::DeleteForward() {
    PushUndo(4);
    if (hasSel && (ay != cy || ax != cx)) { DeleteSelection(); return; }
    hasSel = false;
    std::string& l = lines[cy];
    if (cx < (int)l.size()) l.erase(cx, 1);
    else if (cy + 1 < (int)lines.size()) { l += lines[cy + 1]; lines.erase(lines.begin() + cy + 1); }
}

void CodeEditor::MoveTo(int y, int x, bool select) {
    if (select) { if (!hasSel) { ay = cy; ax = cx; hasSel = true; } }
    else hasSel = false;
    cy = y; cx = x;
    Clamp();
    blink = 0;
}

void CodeEditor::IndentLines(bool out) {
    PushUndo(0);
    int y0 = cy, y1 = cy, x0, x1;
    if (hasSel) SelRange(y0, x0, y1, x1);
    for (int y = y0; y <= y1; y++) {
        if (out) {
            int n = std::min(4, LeadingSpaces(lines[y]));
            lines[y].erase(0, n);
            if (y == cy) cx = std::max(0, cx - n);
            if (y == ay) ax = std::max(0, ax - n);
        } else {
            lines[y].insert(0, "    ");
            if (y == cy) cx += 4;
            if (y == ay && hasSel) ax += 4;
        }
    }
    Clamp();
}

void CodeEditor::ToggleComment() {
    PushUndo(0);
    int y0 = cy, y1 = cy, x0, x1;
    if (hasSel) SelRange(y0, x0, y1, x1);
    bool allCommented = true;
    for (int y = y0; y <= y1; y++) {
        int lead = LeadingSpaces(lines[y]);
        if (lead < (int)lines[y].size() && lines[y].compare(lead, 2, "//") != 0) allCommented = false;
    }
    for (int y = y0; y <= y1; y++) {
        int lead = LeadingSpaces(lines[y]);
        if (lead == (int)lines[y].size()) continue;
        if (allCommented) {
            int n = lines[y].compare(lead, 3, "// ") == 0 ? 3 : 2;
            lines[y].erase(lead, n);
        } else lines[y].insert(lead, "// ");
    }
    Clamp();
}

void CodeEditor::HandleKeys(bool readOnly, float viewH) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    auto pressed = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };
    int pageLines = std::max(1, (int)(viewH / (fontSize * 1.6f)) - 2);

    // ---- navigation -------------------------------------------------------
    if (pressed(KEY_LEFT)) {
        if (hasSel && !shift) { int y0, x0, y1, x1; if (SelRange(y0, x0, y1, x1)) MoveTo(y0, x0, false); else hasSel = false; }
        else if (ctrl) {
            int x = cx;
            while (x > 0 && !IsWordChar(lines[cy][x - 1])) x--;
            while (x > 0 && IsWordChar(lines[cy][x - 1])) x--;
            if (x == cx && cy > 0) MoveTo(cy - 1, (int)lines[cy - 1].size(), shift); else MoveTo(cy, x, shift);
        }
        else if (cx > 0) MoveTo(cy, cx - 1, shift);
        else if (cy > 0) MoveTo(cy - 1, (int)lines[cy - 1].size(), shift);
        preferredX = -1;
    }
    if (pressed(KEY_RIGHT)) {
        if (hasSel && !shift) { int y0, x0, y1, x1; if (SelRange(y0, x0, y1, x1)) MoveTo(y1, x1, false); else hasSel = false; }
        else if (ctrl) {
            int x = cx, n = (int)lines[cy].size();
            while (x < n && !IsWordChar(lines[cy][x])) x++;
            while (x < n && IsWordChar(lines[cy][x])) x++;
            if (x == cx && cy + 1 < (int)lines.size()) MoveTo(cy + 1, 0, shift); else MoveTo(cy, x, shift);
        }
        else if (cx < (int)lines[cy].size()) MoveTo(cy, cx + 1, shift);
        else if (cy + 1 < (int)lines.size()) MoveTo(cy + 1, 0, shift);
        preferredX = -1;
    }
    auto vertical = [&](int dy) {
        if (preferredX < 0) preferredX = cx;
        int ny = std::clamp(cy + dy, 0, (int)lines.size() - 1);
        int nx = std::min(preferredX, (int)lines[ny].size());
        if (dy < 0 && cy == 0) nx = 0;
        if (dy > 0 && cy == (int)lines.size() - 1) nx = (int)lines[ny].size();
        MoveTo(ny, nx, shift);
    };
    if (pressed(KEY_UP)) vertical(-1);
    if (pressed(KEY_DOWN)) vertical(1);
    if (pressed(KEY_PAGE_UP)) vertical(-pageLines);
    if (pressed(KEY_PAGE_DOWN)) vertical(pageLines);
    if (pressed(KEY_HOME)) {
        int lead = LeadingSpaces(lines[cy]);
        if (ctrl) MoveTo(0, 0, shift); else MoveTo(cy, cx == lead ? 0 : lead, shift);
        preferredX = -1;
    }
    if (pressed(KEY_END)) {
        if (ctrl) MoveTo((int)lines.size() - 1, (int)lines.back().size(), shift); else MoveTo(cy, (int)lines[cy].size(), shift);
        preferredX = -1;
    }

    // ---- shortcuts ----------------------------------------------------------
    if (ctrl && IsKeyPressed(KEY_A)) { ay = 0; ax = 0; cy = (int)lines.size() - 1; cx = (int)lines.back().size(); hasSel = true; }
    if (ctrl && IsKeyPressed(KEY_C)) { std::string s = SelectedText(); if (s.empty()) s = lines[cy] + "\n"; SetClipboardText(s.c_str()); }
    if (readOnly) { while (GetCharPressed() > 0) {} return; }
    if (ctrl && IsKeyPressed(KEY_X)) {
        std::string s = SelectedText();
        PushUndo(0);
        if (s.empty()) {   // cut the whole line
            s = lines[cy] + "\n";
            if (lines.size() > 1) lines.erase(lines.begin() + cy); else lines[0].clear();
            cx = 0; Clamp();
        } else DeleteSelection();
        SetClipboardText(s.c_str());
    }
    if (ctrl && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if (clip && *clip) { PushUndo(0); InsertText(clip); }
    }
    if (ctrl && IsKeyPressed(KEY_Z) && !shift) { if (!undo.empty()) { redo.push_back({lines, cy, cx}); Restore(undo.back()); undo.pop_back(); lastEditKind = 0; } }
    if (ctrl && (IsKeyPressed(KEY_Y) || (IsKeyPressed(KEY_Z) && shift))) { if (!redo.empty()) { undo.push_back({lines, cy, cx}); Restore(redo.back()); redo.pop_back(); lastEditKind = 0; } }
    if (ctrl && IsKeyPressed(KEY_SLASH)) ToggleComment();
    if (ctrl && IsKeyPressed(KEY_D)) {   // duplicate line
        PushUndo(0);
        lines.insert(lines.begin() + cy + 1, lines[cy]);
        cy++;
    }

    // ---- editing --------------------------------------------------------------
    if (IsKeyPressed(KEY_TAB)) {
        int y0, x0, y1, x1;
        if (shift) IndentLines(true);
        else if (SelRange(y0, x0, y1, x1) && y0 != y1) IndentLines(false);
        else { PushUndo(1); DeleteSelection(); int n = 4 - cx % 4; lines[cy].insert(cx, std::string(n, ' ')); cx += n; }
    }
    if ((pressed(KEY_ENTER) || pressed(KEY_KP_ENTER)) && !ctrl) Newline();   // Ctrl+Enter = run
    if (pressed(KEY_BACKSPACE)) Backspace();
    if (pressed(KEY_DELETE)) DeleteForward();
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch < 127 && !ctrl) InsertChar((char)ch);
        ch = GetCharPressed();
    }
    Clamp();
}

void CodeEditor::EnsureVisible(Rectangle text, float lineH, float charW) {
    float cyPix = cy * lineH;
    if (cyPix < targetY) targetY = cyPix;
    if (cyPix + lineH > targetY + text.height) targetY = cyPix + lineH - text.height;
    float cxPix = cx * charW;
    if (cxPix < scrollX) scrollX = std::max(0.0f, cxPix - charW * 4);
    if (cxPix > scrollX + text.width - charW * 2) scrollX = cxPix - text.width + charW * 6;
}

void CodeEditor::Frame(Rectangle r, int id, bool readOnly) {
    const float fs = fontSize;
    const float lineH = std::round(fs * 1.6f);
    const float charW = CodeCharWidth(fs);
    const float gutterW = CodeCharWidth(fs * 0.82f) * 3 + 26;
    Rectangle text = {r.x + gutterW + 8, r.y + 10, r.width - gutterW - 20, r.height - 20};
    Clamp();

    bool focused = IsFocused(id);
    bool hover = InputEnabled() && MouseIn(r);
    const float contentH = lines.size() * lineH + lineH * 2;
    const Rectangle barView = {r.x, text.y, r.width - 1, text.height};
    if (hover && !OverScrollBar(barView, contentH)) WantIBeam();

    // ---- scrollbar: comes first, so its clicks don't move the cursor ---------
    const bool onBar = ScrollBarInput(barView, contentH, scrollY, targetY, barDragging, barGrab);

    // ---- mouse -------------------------------------------------------------
    auto posFromMouse = [&](int& y, int& x) {
        Vector2 m = Mouse();
        y = (int)std::floor((m.y - text.y + scrollY) / lineH);
        y = std::clamp(y, 0, (int)lines.size() - 1);
        x = (int)std::round((m.x - text.x + scrollX) / charW);
        x = std::clamp(x, 0, (int)lines[y].size());
    };
    if (!onBar && InputEnabled() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!hover && focused) { ClearTextFocus(); focused = false; }
        if (hover) {
            FocusWidget(id);
            focused = true;
            int y, x;
            posFromMouse(y, x);
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            double now = GetTime();
            if (now - lastClick < 0.35 && y == lastClickY && std::abs(x - lastClickX) <= 1) {
                // double click: select the word
                int a = x, b = x;
                while (a > 0 && IsWordChar(lines[y][a - 1])) a--;
                while (b < (int)lines[y].size() && IsWordChar(lines[y][b])) b++;
                ay = y; ax = a; cy = y; cx = b; hasSel = b > a;
                dragging = false;
            } else {
                MoveTo(y, x, shift);
                if (!shift) { ay = cy; ax = cx; hasSel = true; }
                dragging = true;
            }
            lastClick = now; lastClickY = y; lastClickX = x;
            preferredX = -1;
        }
    }
    if (dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int y, x;
            posFromMouse(y, x);
            cy = y; cx = x;
            Vector2 m = Mouse();
            if (m.y < text.y) targetY -= lineH * 0.5f;
            if (m.y > text.y + text.height) targetY += lineH * 0.5f;
        } else dragging = false;
    }
    if (hover && InputEnabled()) targetY -= GetMouseWheelMove() * lineH * 3;

    // ---- keyboard ----------------------------------------------------------------
    if (focused && InputEnabled()) {
        int oy = cy, ox = cx;
        size_t oldCount = lines.size();
        std::string oldLine = lines[cy];
        HandleKeys(readOnly, text.height);
        if (cy != oy || cx != ox || lines.size() != oldCount || lines[cy] != oldLine) { blink = 0; EnsureVisible(text, lineH, charW); }
    }
    float maxY = std::max(0.0f, lines.size() * lineH - text.height + lineH * 2);
    targetY = std::clamp(targetY, 0.0f, maxY);
    scrollY += (targetY - scrollY) * std::min(1.0f, GetFrameTime() * 18);
    if (std::fabs(targetY - scrollY) < 0.5f) scrollY = targetY;
    blink += GetFrameTime();

    // ---- draw ------------------------------------------------------------------
    RoundRect(r, 12, col::codeBg);
    RoundRectLines(r, 12, focused ? 1.5f : 1.0f, focused ? Alpha(col::accent, 0.7f) : Alpha(col::border, 0.7f));
    BeginClip({r.x + 1, r.y + 1, r.width - 2, r.height - 2});
    int first = std::max(0, (int)(scrollY / lineH) - 1);
    int last = std::min((int)lines.size() - 1, (int)((scrollY + text.height) / lineH) + 1);
    int sy0, sx0, sy1, sx1;
    bool sel = SelRange(sy0, sx0, sy1, sx1);
    for (int i = first; i <= last; i++) {
        float y = text.y + i * lineH - scrollY;
        bool err = std::find(errorLines.begin(), errorLines.end(), i + 1) != errorLines.end();
        if (err) DrawRectangleRec({r.x + 1, y, r.width - 2, lineH}, Alpha(col::danger, 0.13f));
        else if (i == cy && focused && !sel) DrawRectangleRec({r.x + 1, y, r.width - 2, lineH}, Alpha(col::panelHi, 0.6f));
        // selection
        if (sel && i >= sy0 && i <= sy1) {
            int a = i == sy0 ? sx0 : 0;
            int b = i == sy1 ? sx1 : (int)lines[i].size() + 1;
            DrawRectangleRec({text.x + a * charW - scrollX, y, std::max(charW * 0.5f, (b - a) * charW), lineH}, Alpha(col::accent, 0.35f));
        }
        // gutter
        std::string num = std::to_string(i + 1);
        TextRight(num, r.x + gutterW - 6, y + (lineH - fs * 0.82f) / 2, fs * 0.82f, i == cy && focused ? col::text : col::faint, F_MONO);
        if (err) DrawCircleV({r.x + 12, y + lineH / 2}, 4, col::danger);
    }
    BeginClip(text.width > 0 ? Rectangle{text.x - 4, r.y + 1, text.width + 12, r.height - 2} : text);
    for (int i = first; i <= last; i++) {
        float y = text.y + i * lineH - scrollY;
        CodeLine(text.x - scrollX, y + (lineH - fs) / 2, lines[i], fs);
    }
    if (focused && !readOnly && std::fmod(blink, 1.0f) < 0.6f) {
        float y = text.y + cy * lineH - scrollY;
        DrawRectangleRec({text.x + cx * charW - scrollX - 1, y + 3, 2, lineH - 6}, col::accent);
    }
    EndClip();
    EndClip();
    // gutter separator + scrollbar
    DrawRectangleRec({r.x + gutterW, r.y + 8, 1, r.height - 16}, Alpha(col::border, 0.5f));
    DrawScrollBar(barView, contentH, scrollY, barDragging);
}
