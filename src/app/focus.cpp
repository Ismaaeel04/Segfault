// ============================================================================
// focus.cpp  -  The Focus timer (see focus.h).
// ============================================================================
#include "focus.h"
#include "app.h"
#include "files.h"
#include "streak.h"
#include "ui.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace ui;

namespace focus {
namespace {

Phase phase = WORK;
bool running = false;
double endTime = 0;          // GetTime() when the running phase ends
float remaining = -1;        // seconds left while paused (-1 = a full phase)
int sessions = 0;
bool alertOpen = false;
Phase alertFrom = WORK;      // the phase that just ended
float alertAt = 0;
Sound chime = {};
bool chimeLoaded = false;

// SEGFAULT_FOCUS_FAST=<seconds> makes each minute that long (for trying it out)
float Unit() {
    static float unit = -1;
    if (unit < 0) {
        const char* v = std::getenv("SEGFAULT_FOCUS_FAST");
        unit = v ? (float)std::atof(v) : 60.0f;
        if (unit <= 0) unit = 1;
    }
    return unit;
}

float LengthOf(Phase p) {
    int m = p == WORK ? app.focusWork : p == SHORT_BREAK ? app.focusBreak : 15;
    return std::max(1, m) * Unit();
}

std::string Today() {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    return streak::Format(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// A soft two-note bell, made once (no sound files to ship).
void LoadChime() {
    if (chimeLoaded || std::getenv("SEGFAULT_MUTE")) return;   // MUTE: tests stay silent
    if (!IsAudioDeviceReady()) InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    const int rate = 44100;
    const float len = 1.6f;
    const int n = (int)(rate * len);
    std::vector<short> data(n);
    const float notes[2] = {880.0f, 1318.5f};   // A5, then E6
    const float starts[2] = {0.0f, 0.18f};
    for (int i = 0; i < n; i++) {
        float t = (float)i / rate, v = 0;
        for (int k = 0; k < 2; k++) {
            float u = t - starts[k];
            if (u < 0) continue;
            float env = std::exp(-u * 3.2f) * std::min(1.0f, u * 300);   // quick attack, long ring
            v += env * (std::sin(6.2831853f * notes[k] * u) * 0.7f + std::sin(6.2831853f * notes[k] * 2.01f * u) * 0.18f);
        }
        data[i] = (short)(std::max(-1.0f, std::min(1.0f, v * 0.42f)) * 32767);
    }
    Wave w = {};
    w.frameCount = (unsigned)n;
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data.data();
    chime = LoadSoundFromWave(w);   // copies the samples
    chimeLoaded = true;
}

void Finish() {
    running = false;
    alertFrom = phase;
    if (phase == WORK) {
        sessions++;
        if (!app.user.empty()) {
            app.progress.focusMinutes[Today()] += app.focusWork;
            app.progress.Save();
        }
        phase = sessions >= 4 ? LONG_BREAK : SHORT_BREAK;
    } else {
        if (phase == LONG_BREAK) sessions = 0;
        phase = WORK;
    }
    remaining = -1;
    alertOpen = true;
    alertAt = (float)GetTime();
    LoadChime();
    if (chimeLoaded) PlaySound(chime);
    if (!IsWindowFocused()) files::FlashWindow(GetWindowHandle());
}

} // namespace

void Update() {
    if (running && GetTime() >= endTime) Finish();
}

void Start() {
    if (running) return;
    LoadChime();   // opening the audio device takes a moment: do it now, not when the time is up
    float left = remaining > 0 ? remaining : LengthOf(phase);
    endTime = GetTime() + left;
    running = true;
}

void Pause() {
    if (!running) return;
    remaining = std::max(0.0f, (float)(endTime - GetTime()));
    running = false;
}

void Reset() {
    running = false;
    phase = WORK;
    remaining = -1;
    sessions = 0;
}

void Skip() {
    // ending a break early goes back to work; ending work early is a break without the credit
    running = false;
    if (phase == WORK) phase = sessions >= 3 ? LONG_BREAK : SHORT_BREAK;
    else { if (phase == LONG_BREAK) sessions = 0; phase = WORK; }
    remaining = -1;
}

bool Running() { return running; }
bool Active() { return running || remaining > 0; }
Phase CurrentPhase() { return phase; }
float Length() { return LengthOf(phase); }
float Remaining() {
    if (running) return std::max(0.0f, (float)(endTime - GetTime()));
    return remaining > 0 ? remaining : LengthOf(phase);
}
int SessionsDone() { return sessions; }
int TodayMinutes() {
    auto it = app.progress.focusMinutes.find(Today());
    return it == app.progress.focusMinutes.end() ? 0 : it->second;
}
const char* PhaseName(Phase p) { return p == WORK ? "Focus" : p == SHORT_BREAK ? "Short break" : "Long break"; }

std::string Clock(float s) {
    int total = (int)std::ceil(s);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    return buf;
}

bool AlertOpen() { return alertOpen; }

void DrawAlert() {
    if (!alertOpen) return;
    float t = (float)GetTime() - alertAt;
    float k = std::min(1.0f, t / 0.25f);
    float e = 1 - (1 - k) * (1 - k) * (1 - k);
    DrawRectangleRec(ScreenRect(), Alpha(BLACK, 0.45f * e));

    const bool toBreak = alertFrom == WORK;
    const Color c = toBreak ? col::success : col::accent;
    Rectangle card = {W / 2 - 230, H / 2 - 150 + (1 - e) * 24, 460, 300};
    Shadow(card, 18, 2.5f);
    RoundRect(card, 18, col::panel);
    PanelTint(card, 18, c, 0.16f);
    RoundRectLines(card, 18, 1, col::border);

    // a ring that breathes, with a cup (break) or the pointer star (work)
    Vector2 m = {card.x + card.width / 2, card.y + 68};
    float pulse = 0.5f + 0.5f * std::sin(t * 3.2f);
    SoftGlow(m, 70, c, 0.18f + 0.1f * pulse);
    DrawRing(m, 31, 35, 0, 360, 48, Alpha(c, 0.9f));
    if (toBreak) {   // a cup with steam
        Rectangle cup = {m.x - 12, m.y - 4, 20, 16};
        RoundRect(cup, 4, c);
        DrawRing({m.x + 10, m.y + 4}, 3.5f, 6, -90, 90, 12, c);
        for (int i = 0; i < 2; i++) {
            float sx = m.x - 6 + i * 8, wob = std::sin(t * 4 + i) * 1.5f;
            DrawLineEx({sx + wob, m.y - 8}, {sx - wob, m.y - 15}, 2, Alpha(c, 0.7f));
        }
    } else {
        DrawLogo({m.x - 16, m.y - 16, 32, 32}, t, 0.6f);
    }

    std::string title, body, primary, secondary;
    if (toBreak) {
        bool longOne = phase == LONG_BREAK;
        title = longOne ? "Four sessions done!" : "Time for a break";
        body = "You focused for " + std::to_string(app.focusWork) + " minutes. Stand up, drink some water and look away from the screen.";
        primary = longOne ? "Start 15 min break" : "Start " + std::to_string(app.focusBreak) + " min break";
        secondary = "Keep working";
    } else {
        title = "Break's over";
        body = "Ready for another " + std::to_string(app.focusWork) + " minutes? Session " + std::to_string(sessions + 1) + " of 4.";
        primary = "Start focusing";
        secondary = "Not now";
    }
    TextCenter(title, {card.x, card.y + 116, card.width, 30}, 23, col::heading, F_HEAVY);
    float y = card.y + 154;
    for (const std::string& l : Wrap(body, 14.5f, F_MEDIUM, card.width - 70)) {
        TextCenter(l, {card.x, y, card.width, 20}, 14.5f, col::dim, F_MEDIUM);
        y += 21;
    }
    Rectangle b1 = {card.x + card.width / 2 + 6, card.y + card.height - 66, 196, 42};
    Rectangle b2 = {card.x + card.width / 2 - 202, card.y + card.height - 66, 196, 42};
    bool go = Button(b1, primary, BTN_PRIMARY, true, 14.5f) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    bool other = Button(b2, secondary, BTN_GHOST, true, 14.5f) || IsKeyPressed(KEY_ESCAPE);
    if (t < 0.3f) return;   // a click meant for something else doesn't close it straight away
    if (go) { alertOpen = false; Start(); }
    else if (other) {
        alertOpen = false;
        if (toBreak) {   // keep working
            if (phase == LONG_BREAK) sessions = 0;
            phase = WORK; remaining = -1; Start();
        }
    }
}

} // namespace focus
