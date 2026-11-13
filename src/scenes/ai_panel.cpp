// ============================================================================
// ai_panel.cpp  -  The SegFault AI side panel: opens from the top bar on any
// screen, sees what that screen shows (Scene::AiInfo) and answers as the reply
// streams in. Code in answers is drawn like the editor, with a Copy button.
// ============================================================================
#include "app/app.h"
#include "app/assistant.h"
#include "app/runner.h"
#include "app/theme.h"
#include "app/ui.h"
#include <algorithm>
#include <cmath>
#include <map>

using namespace ui;

namespace {

float gSlide = 0;          // 0 closed .. 1 open
std::string gInput;
std::string gKeyInput;
std::string gKeyErr;
bool gKeyShow = false;
bool gKeySetupOpen = false;
Scroll gScroll;
float gLastContentH = 0;
size_t gLastTextLen = 0;
bool gStickToBottom = true;

struct Segment { bool code; std::string text; std::string lang; };

// Splits an answer into prose and ``` code blocks; tidies markdown the panel doesn't draw.
std::vector<Segment> Segments(const std::string& text) {
    std::vector<Segment> out;
    Segment cur{false, "", ""};
    for (std::string line : SplitLines(text)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t lead = line.find_first_not_of(' ');
        if (lead != std::string::npos && line.compare(lead, 3, "```") == 0) {
            if (cur.code) { out.push_back(cur); cur = {false, "", ""}; }
            else {
                if (!cur.text.empty()) out.push_back(cur);
                cur = {true, "", line.substr(lead + 3)};
            }
            continue;
        }
        if (!cur.code) {
            size_t h = line.find_first_not_of('#');
            if (h > 0 && h != std::string::npos && line[h] == ' ') line = "**" + line.substr(h + 1) + "**";   // headings
            else if (line.rfind("* ", 0) == 0) line = "- " + line.substr(2);
            else if (line == "---" || line == "***") continue;
        }
        cur.text += line + "\n";
    }
    if (cur.code || !cur.text.empty()) out.push_back(cur);
    return out;
}

// Draws (or measures) an answer; returns its height.
float Answer(Rectangle r, const std::string& text, bool draw, bool input) {
    const float fs = 14.5f, cfs = 13.5f, lh = CodeLineHeight(cfs), cw = CodeCharWidth(cfs);
    float y = r.y;
    for (const Segment& s : Segments(text)) {
        if (!s.code) {
            std::string t = s.text;
            while (!t.empty() && t.back() == '\n') t.pop_back();
            if (t.empty()) continue;
            y += RichText({r.x, y, r.width, 10000}, t, fs, col::text, draw) + 8;
            continue;
        }
        // code: long lines wrap at the panel's edge
        int maxChars = std::max(10, (int)((r.width - 24) / cw));
        std::vector<std::string> lines;
        for (const std::string& l : SplitLines(s.text)) {
            std::string rest = l;
            do { lines.push_back(rest.substr(0, maxChars)); rest = rest.size() > (size_t)maxChars ? rest.substr(maxChars) : ""; }
            while (!rest.empty());
        }
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
        float h = lines.size() * lh + 34;
        if (draw && y + h > 0 && y < H) {
            Rectangle box = {r.x, y, r.width, h};
            RoundRect(box, 9, col::codeBg);
            RoundRectLines(box, 9, 1, Alpha(col::border, 0.7f));
            Text(s.lang.empty() ? "code" : s.lang, box.x + 12, box.y + 7, 11.5f, col::faint, F_MONOBOLD);
            Rectangle copy = {box.x + box.width - 58, box.y + 4, 52, 20};
            bool hover = input && MouseIn(box);
            if (hover) {
                bool ch = MouseIn(copy);
                RoundRect(copy, 5, ch ? col::panelHi : Alpha(col::panelHi, 0.5f));
                TextCenter("Copy", copy, 11.5f, ch ? col::heading : col::dim, F_BOLD);
                if (ch) WantHand();
                if (ch && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    std::string code = s.text;
                    while (!code.empty() && code.back() == '\n') code.pop_back();
                    SetClipboardText(code.c_str());
                    Toast("Code copied", col::success);
                }
            }
            for (size_t i = 0; i < lines.size(); i++) CodeLine(box.x + 12, box.y + 26 + i * lh, lines[i], cfs);
        }
        y += h + 10;
    }
    return y - r.y;
}

float UserBubble(Rectangle r, const std::string& text, bool draw) {
    const float fs = 14.5f, maxW = r.width * 0.84f;
    std::vector<std::string> lines;
    for (const std::string& para : SplitLines(text))
        for (const std::string& l : Wrap(para.empty() ? " " : para, fs, F_MEDIUM, maxW - 28)) lines.push_back(l);
    float w = 0;
    for (const std::string& l : lines) w = std::max(w, Measure(l, fs, F_MEDIUM).x);
    float h = lines.size() * fs * 1.45f + 20;
    if (draw) {
        Rectangle b = {r.x + r.width - w - 28, r.y, w + 28, h};
        RoundRect(b, 14, Mix(col::panel, col::accent, 0.22f));
        for (size_t i = 0; i < lines.size(); i++) Text(lines[i], b.x + 14, b.y + 10 + i * fs * 1.45f, fs, col::heading, F_MEDIUM);
    }
    return h;
}

struct Chip { std::string label, prompt; };

std::vector<Chip> Chips(const ai::Screen& s) {
    std::vector<Chip> c;
    switch (s.status) {
        case RunReport::CompileError: c.push_back({"Explain this error", "Explain the compile error in my code: what it means and where."}); break;
        case RunReport::RuntimeError: c.push_back({"Why did it crash?", "My program crashed. Why, and on which line?"}); break;
        case RunReport::WrongAnswer: c.push_back({"Why does a test fail?", "Why does my code fail the failing test? Don't give me the full solution."}); break;
        case RunReport::TimeLimit: c.push_back({"Why is it stuck?", "My program ran out of time. Where could it loop forever or wait for input?"}); break;
        case RunReport::RuleBroken: c.push_back({"What rule did I break?", "Which exam rule does my code break, and how can I avoid it?"}); break;
        default: break;
    }
    if (!s.context.empty() && !s.isEditor) c.push_back({"Give me a hint", "Give me a small hint for this problem, without the solution."});
    if (!s.context.empty()) c.push_back({s.isEditor ? "Explain my code" : "Review my code", s.isEditor ? "Explain what my code does, line by line where it matters." : "Review my code: is the approach right, and what would fail?"});
    if (s.context.empty()) {
        c.push_back({"Pointers, simply", "Explain pointers simply, with a tiny C example."});
        c.push_back({"Quiz me", "Ask me one exam-style question about pointers or dynamic memory, then wait for my answer."});
    }
    return c;
}

} // namespace

// The header and the chat inside r. closable: the panel's X.
static void Contents(Rectangle P, const ai::Screen& screen, bool input, bool closable) {
    // header
    const float hx = P.x + 18;
    DrawAiLogo({hx - 2, P.y + 13, 34, 34}, (float)GetTime(), ai::Busy() ? 1.0f : 0.0f);   // glows while it answers
    Text("SegFault AI", hx + 42, P.y + 14, 18, col::heading, F_HEAVY);
    std::string sub = screen.context.empty() ? std::string("ask anything about C, C++ or the exam") : "sees: " + screen.label;
    Text(Ellipsize(sub, 12, F_MEDIUM, P.width - 236), hx + 43, P.y + 37, 12, col::faint, F_MEDIUM);
    float right = P.x + P.width - 14;
    if (closable) {
        Rectangle close = {right - 32, P.y + 14, 32, 32};
        if (Button(close, "", BTN_GHOST, input, 14)) app.aiOpen = false;
        Vector2 cc = {close.x + 16, close.y + 16};
        DrawLineEx({cc.x - 5, cc.y - 5}, {cc.x + 5, cc.y + 5}, 2, col::text);
        DrawLineEx({cc.x + 5, cc.y - 5}, {cc.x - 5, cc.y + 5}, 2, col::text);
        right -= 38;
    }
    if (ai::HasKey()) {
        std::string keyLabel = gKeySetupOpen ? "Done" : std::string(ai::ProviderName(ai::ActiveProvider())) + " key";
        float kw = std::max(54.0f, Measure(keyLabel, 12.5f, F_BOLD).x + 22);
        if (Button({right - kw, P.y + 14, kw, 32}, keyLabel, gKeySetupOpen ? BTN_PRIMARY : BTN_GHOST, input, 12.5f)) {
            gKeySetupOpen = !gKeySetupOpen;
            gKeyErr.clear();
            if (gKeySetupOpen) FocusWidget(9103);
        }
        right -= kw + 6;
    }
    if (!gKeySetupOpen && !ai::Turns().empty() && Button({right - 58, P.y + 14, 58, 32}, "New", BTN_GHOST, input, 12.5f)) {
        ai::NewChat();
        gScroll.Reset();
    }
    DrawRectangleRec({P.x + 1, P.y + 60, P.width - 2, 1}, col::border);

    const Rectangle body = {P.x + 18, P.y + 72, P.width - 36, P.height - 72};
    if (!ai::HasKey() || gKeySetupOpen) {
        float maxW = std::min(body.width, 460.0f);
        Rectangle card = {body.x + (body.width - maxW) / 2, body.y + 18, maxW, 0};
        float y = card.y;

        DrawAiLogo({card.x + card.width / 2 - 28, y, 56, 56}, (float)GetTime(), ai::HasKey() ? 0.35f : 0.0f);
        y += 68;
        TextCenter(ai::HasKey() ? "SegFault AI Key Settings" : "Set up SegFault AI",
                   {card.x, y, card.width, 22}, 17, col::heading, F_HEAVY);
        y += 26;
        TextCenter("Paste a Gemini (AIza... / AQ....) or Claude (sk-ant-...) API key.",
                   {card.x, y, card.width, 18}, 13, col::dim, F_MEDIUM);
        y += 28;

        // Provider selector / status pills
        float pillW = (card.width - 10) / 2;
        for (int p = 0; p < 2; p++) {
            ai::Provider prov = p == 0 ? ai::GEMINI : ai::CLAUDE;
            bool active = ai::ActiveProvider() == prov;
            bool has = ai::HasKey(prov);
            Rectangle pr = {card.x + p * (pillW + 10), y, pillW, 44};
            bool hover = input && MouseIn(pr);
            RoundRect(pr, 9, active ? Alpha(col::accent, 0.18f) : (hover ? col::panelHi : col::panel));
            RoundRectLines(pr, 9, active ? 1.5f : 1.0f, active ? col::accent : col::border);
            std::string title = prov == ai::GEMINI ? "Gemini (Google)" : "Claude (Anthropic)";
            std::string status = has ? (active ? "Active - key saved" : "Key saved - click to use")
                                     : (prov == ai::GEMINI ? "Free tier available" : "Paid API key");
            Text(title, pr.x + 12, pr.y + 6, 13, active ? col::heading : col::text, F_BOLD);
            Text(status, pr.x + 12, pr.y + 24, 11.5f, has ? col::success : col::faint, F_MEDIUM);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ai::UseProvider(prov);
                gKeyErr.clear();
            }
        }
        y += 58;

        // API key input row + Paste + Show/Hide
        Text("API key", card.x, y, 12.5f, col::dim, F_BOLD);
        Rectangle showBtn = {card.x + card.width - 52, y - 3, 52, 22};
        if (Button(showBtn, gKeyShow ? "Hide" : "Show", BTN_GHOST, input, 11.5f)) gKeyShow = !gKeyShow;
        Rectangle pasteBtn = {showBtn.x - 60, y - 3, 54, 22};
        if (Button(pasteBtn, "Paste", BTN_GHOST, input, 11.5f)) {
            const char* clip = GetClipboardText();
            if (clip && *clip) {
                gKeyInput = clip;
                while (!gKeyInput.empty() && (gKeyInput.back() == '\n' || gKeyInput.back() == '\r' || gKeyInput.back() == ' '))
                    gKeyInput.pop_back();
                gKeyErr.clear();
            }
        }
        y += 22;

        Rectangle keyBox = {card.x, y, card.width, 40};
        bool enterSave = TextBox(keyBox, gKeyInput, 9103, 256, false, "AIza... or sk-ant-...", !gKeyShow);
        y += 50;

        auto saveNow = [&]() {
            std::string err = ai::SaveKey(gKeyInput);
            if (!err.empty()) {
                gKeyErr = err;
            } else {
                gKeyInput.clear();
                gKeyErr.clear();
                gKeySetupOpen = false;
                Toast(std::string(ai::ProviderName(ai::ActiveProvider())) + " key saved", col::success);
                FocusWidget(9102);
            }
        };

        bool hasActiveKey = ai::HasKey();
        float saveW = hasActiveKey ? (card.width - 10) * 0.62f : card.width;
        if ((Button({card.x, y, saveW, 40}, "Save key", BTN_PRIMARY, input && !gKeyInput.empty(), 14) || enterSave) && !gKeyInput.empty()) {
            saveNow();
        }
        if (hasActiveKey) {
            Rectangle remBtn = {card.x + saveW + 10, y, card.width - saveW - 10, 40};
            if (Button(remBtn, "Remove key", BTN_DANGER, input && !ai::KeyFromEnvironment(), 13)) {
                ai::RemoveKey();
                gKeyErr.clear();
                Toast("API key removed", col::warn);
            }
        }
        y += 48;

        if (!gKeyErr.empty()) {
            y += RichText({card.x, y, card.width, 80}, gKeyErr, 12.5f, col::danger) + 6;
        }

        // Quick instructions box
        Rectangle helpBox = {card.x, y + 4, card.width, 108};
        RoundRect(helpBox, 9, Alpha(col::panel, 0.75f));
        RoundRectLines(helpBox, 9, 1, Alpha(col::border, 0.6f));
        RichText({helpBox.x + 12, helpBox.y + 10, helpBox.width - 24, helpBox.height - 16},
                 "**Where to get a key:**\n"
                 "- **Gemini (free):** create a key in Google AI Studio (`aistudio.google.com/apikey`).\n"
                 "- **Claude:** create a key in Anthropic Console (`console.anthropic.com`).\n"
                 "- Saved locally on this PC inside your SegFault data folder.",
                 12.0f, col::dim);
        return;
    }
    // ---- the conversation
    const float inputH = 62;
    const std::vector<Chip> chips = ai::Busy() ? std::vector<Chip>() : Chips(screen);
    const float chipsH = chips.empty() ? 0 : 40;
    Rectangle view = {body.x - 6, body.y, body.width + 12, body.y + body.height - inputH - chipsH - body.y - 6};
    const std::vector<ai::Turn>& turns = ai::Turns();
    const float gap = 14;

    if (turns.empty()) {
        float y = view.y + 40;
        DrawAiLogo({view.x + view.width / 2 - 30, y - 4, 60, 60}, (float)GetTime(), 0.3f);
        y += 76;
        TextCenter("Stuck? Ask me.", {view.x, y, view.width, 26}, 20, col::heading, F_HEAVY);
        y += 34;
        RichText({view.x + 30, y, view.width - 60, 200},
                 screen.context.empty()
                     ? "Ask about pointers, memory, an exam question or any C or C++ doubt. Open me from the top bar "
                       "while you solve a problem and I'll see your code and your last run too."
                     : "I can see the problem and the code on this screen, and what happened the last time you ran it. "
                       "I'll give you hints before answers.",
                 14, col::dim);
    } else {
        // measure, then follow the end of the conversation while it grows
        std::vector<float> hs;
        float total = 8;
        for (const ai::Turn& t : turns) {
            float h = t.user ? UserBubble({view.x + 6, 0, view.width - 22, 0}, t.text, false)
                             : (t.text.empty() ? 30 : Answer({view.x + 6, 0, view.width - 22, 0}, t.text, false, false)) + (t.failed ? 16 : 0);
            hs.push_back(h);
            total += h + gap;
        }
        size_t len = turns.back().text.size() + turns.size();
        float maxScroll = std::max(0.0f, total - view.height);
        if (len != gLastTextLen || total != gLastContentH) {
            if (gStickToBottom) gScroll.ScrollTo(maxScroll, view, total);
            gLastTextLen = len;
            gLastContentH = total;
        }
        if (input) gScroll.Update(view, total);
        gStickToBottom = gScroll.target >= maxScroll - 40;
        BeginClip(view);
        float y = view.y + 8 - gScroll.y;
        for (size_t i = 0; i < turns.size(); i++) {
            const ai::Turn& t = turns[i];
            if (y + hs[i] >= view.y - 40 && y <= view.y + view.height + 40) {
                if (t.user) UserBubble({view.x + 6, y, view.width - 22, 0}, t.text, true);
                else if (t.text.empty() && t.streaming) {
                    Sparkle({view.x + 16, y + 12}, 8 + std::sin((float)GetTime() * 6) * 1.5f, col::accent);
                    static const char* dots[] = {"Thinking", "Thinking.", "Thinking..", "Thinking..."};
                    Text(dots[(int)(GetTime() * 3) % 4], view.x + 32, y + 4, 14, col::dim, F_MEDIUM);
                } else {
                    if (t.failed) RoundRect({view.x + 2, y - 6, view.width - 14, hs[i] - 2}, 10, Alpha(col::danger, 0.10f));
                    Answer({view.x + 6 + (t.failed ? 6 : 0), y + (t.failed ? 2 : 0), view.width - 22 - (t.failed ? 12 : 0), 0}, t.text, true,
                           input && MouseIn(view));
                }
            }
            y += hs[i] + gap;
        }
        EndClip();
        ScrollBar(view, total, gScroll);
    }

    // ---- suggestions
    float cy = view.y + view.height + 6;
    if (!chips.empty()) {
        float cx = body.x;
        for (const Chip& c : chips) {
            float w = Measure(c.label, 12.5f, F_BOLD).x + 24;
            if (cx + w > body.x + body.width) break;
            Rectangle b = {cx, cy, w, 30};
            bool hover = input && MouseIn(b);
            RoundRect(b, 15, hover ? Alpha(col::accent, 0.24f) : Alpha(col::accent, 0.12f));
            RoundRectLines(b, 15, 1, Alpha(col::accent, 0.35f));
            TextCenter(c.label, b, 12.5f, hover ? col::heading : col::text, F_BOLD);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { ai::Ask(c.prompt, screen); gStickToBottom = true; }
            cx += w + 8;
        }
    }

    // ---- the question box
    Rectangle box = {body.x, body.y + body.height - inputH + 6, body.width - 92, 44};
    bool submit = TextBox(box, gInput, 9102, 2000, false, "Ask SegFault AI...");
    Rectangle send = {box.x + box.width + 10, box.y, 82, 44};
    if (ai::Busy()) {
        if (Button(send, "Stop", BTN_DANGER, input, 14)) ai::Stop();
    } else if ((Button(send, "Send", BTN_PRIMARY, input && !gInput.empty(), 14) || submit) && !gInput.empty()) {
        ai::Ask(gInput, screen);
        gInput.clear();
        gStickToBottom = true;
        FocusWidget(9102);
    }
}

void AiPanel(Scene* scene, float dt) {
    ai::Poll();
    float target = app.aiOpen ? 1.0f : 0.0f;
    gSlide += (target - gSlide) * std::min(1.0f, dt * 14);
    if (std::fabs(gSlide - target) < 0.002f) gSlide = target;
    if (gSlide <= 0) return;

    const float e = 1 - (1 - gSlide) * (1 - gSlide);
    const Rectangle P = {W + ExtraX() - kAiPanelWidth * e, 64, kAiPanelWidth, H - 64};
    DrawRectangleGradientH((int)(P.x - 24), (int)P.y, 24, (int)P.height, Alpha(col::shadow, 0), Alpha(col::shadow, 0.22f));
    DrawRectangleRec(P, Mix(col::bg2, col::panel, 0.6f));
    DrawRectangleRec({P.x, P.y, 1, P.height}, col::border);
    Contents(P, scene->AiInfo(), InputEnabled() && app.aiOpen, true);
}

// SegFault AI as an app of its own (opened from the desktop): the same chat, full screen.
void AiFullScreen(Rectangle r) {
    ai::Poll();
    Panel(r, Mix(col::bg2, col::panel, 0.6f), 14);
    ai::Screen none;
    none.label = "SegFault AI";
    Contents(r, none, InputEnabled(), false);
}
