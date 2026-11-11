// ============================================================================
// assistant.h  -  SegFault AI: a tutor for the exam, built on the Claude API.
// It sees what the student is working on (the problem, their code, the last
// run), and answers in a side panel (scenes/ai_panel.cpp) as the reply
// streams in. The conversation runs on a worker thread; Poll() brings the new
// text to the UI thread every frame.
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace ai {

// What the current screen shows SegFault AI (Scene::AiInfo).
struct Screen {
    std::string label;     // "Practice - Two Sum", shown in the panel
    std::string context;   // the problem, the code, the last result (sent to the model)
    int status = -1;       // RunReport::Status of the last run, -1 = none
    bool isEditor = false; // SegFault Editor (free code) rather than a problem
};

struct Turn {
    bool user = false;
    std::string text;      // what the panel shows
    bool failed = false;   // an error or a refusal: shown, never sent again
    bool streaming = false;
};

// Who answers: Claude (Anthropic, paid) or Gemini (Google, has a free plan).
enum Provider { CLAUDE = 0, GEMINI = 1 };
Provider ActiveProvider();
void UseProvider(Provider p);           // starts a new chat
const char* ProviderName(Provider p);

// Keys: ANTHROPIC_API_KEY / GEMINI_API_KEY, or the ones saved on this computer.
bool HasKey();                          // for the active provider
bool HasKey(Provider p);
bool KeyFromEnvironment();
// Saves a key; its kind is told by its start (sk-ant-... Claude, AIza... or AQ.... Gemini)
// and that provider becomes the active one. Returns an error, or "".
std::string SaveKey(const std::string& key);
void RemoveKey();                       // the active provider's
std::string KeyFile();                  // where the active provider's key is kept

void Ask(const std::string& question, const Screen& screen);
void Stop();                    // stops the answer being written
void NewChat();
void Poll();                    // every frame: moves streamed text into Turns()
bool Busy();
const std::vector<Turn>& Turns();

// Line-numbered code for the context ("  3 | int *p = 0;").
std::string NumberLines(const std::string& code);

// A made-up exchange for screenshots (--shot with SEGFAULT_AI_DEMO); never sent.
void LoadDemo();

} // namespace ai
