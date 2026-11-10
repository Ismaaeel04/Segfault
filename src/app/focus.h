// ============================================================================
// focus.h  -  The Focus timer (Pomodoro): 25 minutes of work, a 5 minute
// break, and a longer 15 minute break after every fourth session.
// It lives outside the scenes so it keeps running while you study anywhere
// in the app; main.cpp ticks it every frame and shows its pill in the top bar.
// When a session ends it plays a chime, flashes the taskbar button and opens a
// small window that says what comes next.
// ============================================================================
#pragma once
#include <string>

namespace focus {

enum Phase { WORK, SHORT_BREAK, LONG_BREAK };

void Update();                 // every frame (uses the wall clock, not dt)
void Start();                  // starts or resumes the current phase
void Pause();
void Reset();                  // back to a fresh work session
void Skip();                   // ends this phase now (a skipped session isn't counted)

bool Running();
bool Active();                 // running, or paused part way through
Phase CurrentPhase();
float Remaining();             // seconds
float Length();                // seconds of the current phase
int SessionsDone();            // finished work sessions in this cycle (0-3)
int TodayMinutes();            // finished work minutes today (this student)
const char* PhaseName(Phase p);
std::string Clock(float seconds);   // "24:59"

// The "time for a break" window. main draws it over everything; while it is
// open the scene underneath gets no input.
bool AlertOpen();
void DrawAlert();

} // namespace focus
