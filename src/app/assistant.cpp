// ============================================================================
// assistant.cpp  -  SegFault AI (see assistant.h): the conversation, the
// request to Claude (Anthropic) or Gemini (Google), and the streamed answer.
// ============================================================================
#include "assistant.h"
#include "ai_stream.h"
#include "app.h"
#include "http.h"
#include "json.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace ai {
namespace {

// Claude: the cheapest model, about $0.10 / $0.50 per million tokens in / out.
const char* kClaudeModel = "claude-haiku-5-5";

// Gemini: model names change often, so these are tried in order and the first
// one this key can use is kept. Flash-Lite first: its free daily limit is the
// highest. SEGFAULT_GEMINI_MODEL picks one by hand.
const char* kGeminiModels[] = {
    "gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemini-3.8-flash", "gemini-3.6-flash",
    "gemini-3-flash", "gemini-2.5-flash-lite", "gemini-2.5-flash",
};
const int kGeminiModelCount = (int)(sizeof(kGeminiModels) / sizeof(kGeminiModels[0]));
std::atomic<int> gGeminiPick{0};

// Stable text first, so a prompt cache can reuse it on every turn.
const char* kSystem =
    "You are SegFault AI, the tutor inside SegFault, an app where university students prepare for "
    "a C and C++ programming exam (Advanced Programming). The exam covers numbers and loops, arrays "
    "and 2D grids, strings, pointers and pointer arithmetic, dynamic memory (malloc/free, new/delete) "
    "and linked lists. Students write functions that are checked by hidden tests, sometimes under "
    "rules such as \"no [ ] indexing\".\n\n"
    "A message may start with a <screen> block: what the student is looking at right now - the "
    "problem statement, their current code with line numbers, and the result of their last run. Use "
    "it, and refer to their code by line number; don't ask them to paste what you can already see. "
    "A <screen> block stays valid until a newer one arrives.\n\n"
    "How to help:\n"
    "- Teach rather than solve. On a practice problem, start with a hint or a question that points "
    "at the idea or the line that matters. Give a complete solution only when the student asks for "
    "one outright; then give it and explain it.\n"
    "- Compile errors: say what the compiler means in plain words and where. Crashes (segmentation "
    "fault, access violation, stack overflow): name the likely cause in their code - a NULL or "
    "dangling pointer, an index out of bounds, a missing base case, a freed block used again - and "
    "how to check it.\n"
    "- Keep answers short and concrete: a few sentences, and a small code snippet when it helps, in "
    "a fenced block with its language (```c or ```cpp). The answer is read in a narrow side panel: "
    "no tables, no big headings.\n"
    "- Always reply in English by default. Only switch to another language if the student clearly "
    "writes their latest message in that language.\n"
    "- Write maths as plain text (sqrt(n), n^2, i <= n), never LaTeX or $...$: the panel can't show it.\n"
    "- For questions far from C, C++ or studying, answer briefly and bring them back to their work.";

struct Entry {
    Turn turn;
    std::string content;   // user: the text sent (with any <screen> block)
    Provider from = CLAUDE;
    json::Value native;    // assistant: Claude's content blocks or Gemini's parts, as received
    bool send = false;
};

struct Live {
    std::mutex m;
    std::string text;
    bool done = false;
    bool refused = false;
    bool cutShort = false;   // hit the length limit
    std::string error;
    json::Value native;
    std::atomic<bool> cancel{false};
};

std::vector<Entry> gEntries;
std::vector<Turn> gTurns;           // a copy for the UI, rebuilt when something changes
std::shared_ptr<Live> gLive;
std::string gLastContext;

struct KeySlot { std::string key; bool fromEnv = false; };
KeySlot gKeys[2];
Provider gActive = CLAUDE;
bool gLoaded = false;

std::string KeyPath(Provider p) { return app.dataDir + (p == CLAUDE ? "segfault_ai_key.txt" : "segfault_gemini_key.txt"); }
std::string ChoicePath() { return app.dataDir + "segfault_ai_provider.txt"; }

std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r' || s.back() == '\t')) s.pop_back();
    while (!s.empty() && (s[0] == ' ' || s[0] == '\t')) s.erase(0, 1);
    return s;
}

void Load() {
    if (gLoaded) return;
    gLoaded = true;
    const char* envNames[2] = {"ANTHROPIC_API_KEY", "GEMINI_API_KEY"};
    for (int p = 0; p < 2; p++) {
        const char* k = std::getenv(envNames[p]);
        if (k && *k) { gKeys[p].key = k; gKeys[p].fromEnv = true; continue; }
        std::ifstream in(KeyPath((Provider)p));
        std::getline(in, gKeys[p].key);
        gKeys[p].key = Trim(gKeys[p].key);
    }
    std::string choice;
    std::ifstream(ChoicePath()) >> choice;
    if (choice == "gemini") gActive = GEMINI;
    else if (choice == "claude") gActive = CLAUDE;
    else gActive = !gKeys[GEMINI].key.empty() && gKeys[CLAUDE].key.empty() ? GEMINI : CLAUDE;
}

void Rebuild() {
    gTurns.clear();
    for (const Entry& e : gEntries) gTurns.push_back(e.turn);
}

std::string ErrorDetail(const std::string& body) {
    json::Value v;
    if (json::Parse(body, v)) {
        if (const json::Value* e = v.Get("error")) return e->Text("message");
        if (v.type == json::Value::Array && !v.items.empty())   // Gemini sometimes wraps it in a list
            if (const json::Value* e = v.items[0].Get("error")) return e->Text("message");
    }
    return "";
}

// A friendly line for an HTTP failure.
std::string Explain(Provider p, int status, const std::string& body, const std::string& netError) {
    if (status == 0) return netError.empty() ? "No answer from the server." : netError;
    std::string detail = ErrorDetail(body);
    if (p == GEMINI) {
        if (status == 400 && detail.find("API key") != std::string::npos)
            return "SegFault AI can't connect: its key isn't valid.";
        if (status == 429) return "SegFault AI has answered a lot today. Wait a minute and try again, or come back tomorrow.";
        if (status == 403) return "SegFault AI can't connect right now." + (detail.empty() ? "" : " (" + detail + ")");
        if (status == 503) return "SegFault AI is busy right now. Try again in a minute.";
    } else {
        switch (status) {
            case 401: return "SegFault AI can't connect: its key isn't valid.";
            case 403: return "SegFault AI can't connect right now." + (detail.empty() ? "" : " (" + detail + ")");
            case 429: return "Too many requests right now (rate limit). Wait a moment and try again.";
            case 529: return "SegFault AI is busy right now. Try again in a minute.";
        }
    }
    if (status >= 500) return "The server had a problem (" + std::to_string(status) + "). Try again.";
    return "Request failed (" + std::to_string(status) + ")" + (detail.empty() ? "." : ": " + detail);
}

// ---- Claude -----------------------------------------------------------------
std::string ClaudeBody() {
    json::Value req = json::Value::Obj();
    req.Set("model", json::Value::Str(kClaudeModel));
    req.Set("max_tokens", json::Value::Num(16000));
    req.Set("stream", json::Value::Boolean(true));
    json::Value cache = json::Value::Obj();
    cache.Set("type", json::Value::Str("ephemeral"));
    req.Set("cache_control", cache);   // caches the conversation so far for the next turn
    json::Value sys = json::Value::Arr();
    json::Value sysText = json::Value::Obj();
    sysText.Set("type", json::Value::Str("text"));
    sysText.Set("text", json::Value::Str(kSystem));
    sys.Push(sysText);
    req.Set("system", sys);
    json::Value thinking = json::Value::Obj();
    thinking.Set("type", json::Value::Str("adaptive"));
    req.Set("thinking", thinking);
    json::Value oc = json::Value::Obj();
    oc.Set("effort", json::Value::Str("low"));   // a quick tutor in a chat; thinking still adapts
    req.Set("output_config", oc);
    json::Value& msgs = req.Set("messages", json::Value::Arr());
    for (const Entry& e : gEntries) {
        if (!e.send) continue;
        json::Value m = json::Value::Obj();
        m.Set("role", json::Value::Str(e.turn.user ? "user" : "assistant"));
        if (e.turn.user) m.Set("content", json::Value::Str(e.content));
        else if (e.from == CLAUDE) m.Set("content", e.native);   // thinking blocks go back unchanged
        else m.Set("content", json::Value::Str(e.turn.text));    // a Gemini answer: its text
        msgs.Push(m);
    }
    return json::Dump(req);
}

void ClaudeWorker(std::shared_ptr<Live> live, std::string body, std::string key) {
    StreamParser parser;
    std::vector<std::string> headers = {
        "content-type: application/json",
        "x-api-key: " + key,
        "anthropic-version: 2026-10-21",
    };
    HttpResult r = HttpPost("api.anthropic.com", "/v1/messages", headers, body, [&](const char* d, size_t n) {
        parser.Feed(d, n);
        std::lock_guard<std::mutex> lock(live->m);
        live->text = parser.Text();
        return !live->cancel.load();
    });
    std::lock_guard<std::mutex> lock(live->m);
    live->text = parser.Text();
    live->native = parser.EchoContent();
    live->refused = parser.stopReason == "refusal";
    live->cutShort = parser.stopReason == "max_tokens";
    if (live->cancel) live->error = "Stopped.";
    else if (r.status != 200) live->error = Explain(CLAUDE, r.status, r.body, r.error);
    else if (!parser.error.empty()) live->error = parser.error;
    else if (!r.error.empty()) live->error = r.error;
    else if (!parser.finished) live->error = "The answer was cut off. Try again.";
    live->done = true;
}

// ---- Gemini -----------------------------------------------------------------
std::string GeminiBody() {
    json::Value req = json::Value::Obj();
    json::Value sys = json::Value::Obj();
    json::Value sysParts = json::Value::Arr();
    json::Value sysText = json::Value::Obj();
    sysText.Set("text", json::Value::Str(kSystem));
    sysParts.Push(sysText);
    sys.Set("parts", sysParts);
    req.Set("systemInstruction", sys);
    json::Value& contents = req.Set("contents", json::Value::Arr());
    for (const Entry& e : gEntries) {
        if (!e.send) continue;
        json::Value c = json::Value::Obj();
        c.Set("role", json::Value::Str(e.turn.user ? "user" : "model"));
        if (!e.turn.user && e.from == GEMINI && !e.native.items.empty()) c.Set("parts", e.native);   // signatures go back unchanged
        else {
            json::Value parts = json::Value::Arr();
            json::Value t = json::Value::Obj();
            t.Set("text", json::Value::Str(e.turn.user ? e.content : e.turn.text));
            parts.Push(t);
            c.Set("parts", parts);
        }
        contents.Push(c);
    }
    json::Value gen = json::Value::Obj();
    gen.Set("maxOutputTokens", json::Value::Num(8192));
    req.Set("generationConfig", gen);
    return json::Dump(req);
}

// "this model doesn't exist / isn't available on this key": try the next one
bool ModelUnavailable(const HttpResult& r) {
    if (r.status == 404) return true;
    std::string d = ErrorDetail(r.body);
    if (r.status == 400 && (d.find("not found") != std::string::npos || d.find("not supported") != std::string::npos)) return true;
    if (r.status == 429 && r.body.find("limit: 0") != std::string::npos) return true;   // not in this key's free plan
    return false;
}

// The best model this key can use, from Google's own list: a Flash-Lite, else a Flash.
std::string DiscoverGeminiModel(const std::string& key) {
    HttpResult r = HttpGet("generativelanguage.googleapis.com", "/v1beta/models?pageSize=200", {"x-goog-api-key: " + key});
    json::Value v;
    if (r.status != 200 || !json::Parse(r.body, v)) return "";
    const json::Value* list = v.Get("models");
    if (!list) return "";
    std::string best;
    int bestScore = -1;
    for (const json::Value& m : list->items) {
        std::string name = m.Text("name");                       // "models/gemini-3.5-flash-lite"
        if (name.rfind("models/", 0) == 0) name = name.substr(7);
        bool canChat = false;
        if (const json::Value* g = m.Get("supportedGenerationMethods"))
            for (const json::Value& x : g->items) if (x.s == "generateContent") canChat = true;
        if (!canChat || name.find("gemini") == std::string::npos) continue;
        if (name.find("image") != std::string::npos || name.find("tts") != std::string::npos ||
            name.find("audio") != std::string::npos || name.find("live") != std::string::npos) continue;
        int score = name.find("flash-lite") != std::string::npos ? 3 : name.find("flash") != std::string::npos ? 2 : 0;
        if (score == 0) continue;
        if (name.find("preview") != std::string::npos || name.find("exp") != std::string::npos) score -= 1;
        if (score > bestScore || (score == bestScore && name > best)) { best = name; bestScore = score; }   // newer names sort later
    }
    return best;
}

std::mutex gDiscoveredMutex;
std::string gDiscovered;   // the model Google's list gave, once the names above failed

void GeminiWorker(std::shared_ptr<Live> live, std::string body, std::string key) {
    std::vector<std::string> models;
    if (const char* m = std::getenv("SEGFAULT_GEMINI_MODEL")) if (*m) models.push_back(m);
    {
        std::lock_guard<std::mutex> lock(gDiscoveredMutex);
        if (!gDiscovered.empty()) models.push_back(gDiscovered);
    }
    for (int i = gGeminiPick.load(); i < kGeminiModelCount; i++) models.push_back(kGeminiModels[i]);
    for (int i = 0; i < gGeminiPick.load(); i++) models.push_back(kGeminiModels[i]);   // the rest, last

    GeminiParser parser;
    HttpResult r;
    for (const std::string& model : models) {
        parser = GeminiParser();
        std::vector<std::string> headers = {"content-type: application/json", "x-goog-api-key: " + key};
        r = HttpPost("generativelanguage.googleapis.com", "/v1beta/models/" + model + ":streamGenerateContent?alt=sse",
                     headers, body, [&](const char* d, size_t n) {
                         parser.Feed(d, n);
                         std::lock_guard<std::mutex> lock(live->m);
                         live->text = parser.Text();
                         return !live->cancel.load();
                     });
        if (live->cancel || !ModelUnavailable(r)) {
            for (int i = 0; i < kGeminiModelCount; i++)
                if (model == kGeminiModels[i] && r.status == 200) gGeminiPick = i;   // remember the one that works
            break;
        }
    }
    bool known;
    { std::lock_guard<std::mutex> lock(gDiscoveredMutex); known = !gDiscovered.empty(); }
    if (!live->cancel && ModelUnavailable(r) && !known) {   // none of the names worked: ask Google
        std::string m = DiscoverGeminiModel(key);
        if (!m.empty()) {
            { std::lock_guard<std::mutex> lock(gDiscoveredMutex); gDiscovered = m; }
            parser = GeminiParser();
            std::vector<std::string> headers = {"content-type: application/json", "x-goog-api-key: " + key};
            r = HttpPost("generativelanguage.googleapis.com", "/v1beta/models/" + m + ":streamGenerateContent?alt=sse",
                         headers, body, [&](const char* d, size_t n) {
                             parser.Feed(d, n);
                             std::lock_guard<std::mutex> lock(live->m);
                             live->text = parser.Text();
                             return !live->cancel.load();
                         });
        }
    }
    std::lock_guard<std::mutex> lock(live->m);
    live->text = parser.Text();
    live->native = parser.EchoParts();
    live->refused = parser.Refused();
    live->cutShort = parser.finishReason == "MAX_TOKENS";
    if (live->cancel) live->error = "Stopped.";
    else if (r.status != 200) live->error = Explain(GEMINI, r.status, r.body, r.error);
    else if (!parser.error.empty()) live->error = parser.error;
    else if (!r.error.empty()) live->error = r.error;
    else if (live->text.empty() && !live->refused) live->error = "SegFault AI sent an empty answer. Try again.";
    live->done = true;
}

} // namespace

// ---- keys -------------------------------------------------------------------
Provider ActiveProvider() { Load(); return gActive; }
const char* ProviderName(Provider p) { return p == CLAUDE ? "Claude" : "Gemini"; }
bool HasKey() { Load(); return !gKeys[gActive].key.empty(); }
bool HasKey(Provider p) { Load(); return !gKeys[p].key.empty(); }
bool KeyFromEnvironment() { Load(); return gKeys[gActive].fromEnv; }
std::string KeyFile() { Load(); return KeyPath(gActive); }

void UseProvider(Provider p) {
    Load();
    if (p == gActive) return;
    gActive = p;
    std::ofstream(ChoicePath()) << (p == GEMINI ? "gemini" : "claude") << "\n";
    NewChat();
}

std::string SaveKey(const std::string& key) {
    Load();
    std::string k = Trim(key);
    Provider p;
    if (k.rfind("sk-ant-", 0) == 0) p = CLAUDE;
    else if (k.rfind("AIza", 0) == 0 || k.rfind("AQ.", 0) == 0) p = GEMINI;   // Google's older and newer key formats
    else return "That doesn't look like a Claude key (sk-ant-...) or a Gemini key (AIza... or AQ....).";
    gKeys[p].key = k;
    gKeys[p].fromEnv = false;
    std::ofstream(KeyPath(p)) << k << "\n";
    UseProvider(p);
    return "";
}

void RemoveKey() {
    Load();
    gKeys[gActive].key.clear();
    gKeys[gActive].fromEnv = false;
    std::remove(KeyPath(gActive).c_str());
}

std::string NumberLines(const std::string& code) {
    std::string out;
    std::istringstream in(code);
    std::string line;
    int n = 1;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        char num[16];
        std::snprintf(num, sizeof(num), "%3d | ", n++);
        out += num + line + "\n";
    }
    return out;
}

// ---- the conversation ---------------------------------------------------------
void Ask(const std::string& question, const Screen& screen) {
    if (Busy() || question.empty() || !HasKey()) return;
    std::string content;
    if (!screen.context.empty() && screen.context != gLastContext) {
        content = "<screen>\n" + screen.context + "\n</screen>\n\n";
        gLastContext = screen.context;
    }
    content += question;

    Entry u;
    u.turn.user = true;
    u.turn.text = question;
    u.content = content;
    u.send = true;
    gEntries.push_back(u);

    const Provider p = gActive;
    std::string body = p == CLAUDE ? ClaudeBody() : GeminiBody();
    Entry a;
    a.turn.streaming = true;
    a.from = p;
    gEntries.push_back(a);
    Rebuild();

    gLive = std::make_shared<Live>();
    if (p == CLAUDE) std::thread(ClaudeWorker, gLive, body, gKeys[CLAUDE].key).detach();
    else std::thread(GeminiWorker, gLive, body, gKeys[GEMINI].key).detach();
}

void LoadDemo() {
    gEntries.clear();
    Entry u;
    u.turn.user = true;
    u.turn.text = "Why does my program crash on line 3?";
    gEntries.push_back(u);
    Entry a;
    a.turn.text =
        "On **line 3** you write through `p`, but `p` is still `NULL` - nothing was ever allocated for it, "
        "so the write lands on address 0 and Windows stops the program (access violation).\n\n"
        "- Give `p` some memory first, or point it at a real variable.\n"
        "- Check what `malloc` returns before using it.\n\n"
        "```c\nint *p = malloc(sizeof(int));\nif (p == NULL) return 1;\n*p = 42;\nfree(p);   // don't forget to free it!\n```\n\n"
        "Can you see which of the two fixes fits your exercise?";
    gEntries.push_back(a);
    Entry u2;
    u2.turn.user = true;
    u2.turn.text = "What if I use new instead of malloc?";
    gEntries.push_back(u2);
    Entry a2;
    a2.turn.text =
        "Yes, `new` works cleanly in C++: `int *p = new int(42);` and `delete p;` when you're done.\n\n"
        "Just make sure you never mix `malloc` with `delete` or `new` with `free` — that is undefined "
        "behaviour, even if it seems to work at first.";
    gEntries.push_back(a2);
    Rebuild();
}

void Stop() { if (gLive) gLive->cancel = true; }

void NewChat() {
    Stop();
    gLive.reset();   // a running answer finishes on its own and is dropped
    gEntries.clear();
    gLastContext.clear();
    Rebuild();
}

bool Busy() { return gLive != nullptr; }
const std::vector<Turn>& Turns() { return gTurns; }

void Poll() {
    if (!gLive || gEntries.size() < 2) return;
    Entry& a = gEntries.back();
    Entry& u = gEntries[gEntries.size() - 2];
    bool done;
    {
        std::lock_guard<std::mutex> lock(gLive->m);
        a.turn.text = gLive->text;
        done = gLive->done;
        if (done) {
            a.turn.streaming = false;
            if (!gLive->error.empty() || gLive->refused) {
                // keep what was written, but never send this exchange again
                a.turn.failed = true;
                u.send = false;
                if (gLive->refused) a.turn.text = "SegFault AI can't help with that one. Try asking it another way.";
                else a.turn.text += (a.turn.text.empty() ? "" : "\n\n") + gLive->error;
                if (u.content.rfind("<screen>", 0) == 0) gLastContext.clear();
            } else {
                a.native = gLive->native;
                a.send = true;
                if (gLive->cutShort) a.turn.text += "\n\n(The answer hit its length limit.)";
            }
        }
    }
    Rebuild();
    if (done) gLive.reset();
}

} // namespace ai
