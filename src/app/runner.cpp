// ============================================================================
// runner.cpp  -  compiler detection, test harness, compile + run + parse.
// ============================================================================
#include "runner.h"
#include "process.h"
#include "content/content.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

// ======================================================== detection ======
namespace runner {
static std::vector<Compiler> gCompilers;
static std::atomic<bool> gDone{false};
static std::atomic<int> gActive{0};
static std::mutex gMutex;
static std::thread gDetectThread;

static std::string FirstLine(const std::string& s) {
    size_t e = s.find('\n');
    std::string l = s.substr(0, e);
    if (!l.empty() && l.back() == '\r') l.pop_back();
    return l;
}

static void Detect(const std::string& resourceDir) {
    std::vector<Compiler> found;
    // 1) g++ / clang++ in PATH
    struct Cand { const char* exe; Backend kind; const char* label; };
    const Cand cands[] = {{"g++", Backend::GCC, "g++"}, {"clang++", Backend::Clang, "clang++"}};
    for (const Cand& c : cands) {
        ProcResult r = RunProcess({c.exe, "--version"}, "", 8000);
        if (r.started && r.exitCode == 0 && !r.output.empty()) {
            std::string v = FirstLine(r.output);
            std::smatch m;
            std::string ver;
            if (std::regex_search(v, m, std::regex("([0-9]+\\.[0-9]+(\\.[0-9]+)?)"))) ver = " " + m[1].str();
            bool isClang = v.find("clang") != std::string::npos;
            Compiler comp{isClang ? Backend::Clang : c.kind, (isClang ? std::string("clang") : std::string(c.label)) + ver, c.exe, true, {}, false};
            bool dup = false;
            for (auto& f : found) if (f.kind == comp.kind) dup = true;
            if (!dup) found.push_back(comp);
        }
    }
#ifdef _WIN32
    // 2) Visual Studio's cl.exe (through vswhere + vcvars64.bat)
    std::string pf = GetEnvVar("ProgramFiles(x86)");
    if (pf.empty()) pf = "C:\\Program Files (x86)";
    std::string vswhere = pf + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    std::error_code ec;
    if (fs::exists(vswhere, ec)) {
        ProcResult r = RunProcess({vswhere, "-latest", "-products", "*", "-requires",
                                   "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"}, "", 15000);
        std::string path = FirstLine(r.output);
        std::string vcvars = path + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
        if (!path.empty() && fs::exists(vcvars, ec)) {
            std::vector<std::string> env = CaptureBatchEnvironment(vcvars);
            std::string pathVar;
            for (auto& e : env) {
                std::string key = e.substr(0, e.find('='));
                for (auto& ch : key) ch = (char)toupper((unsigned char)ch);
                if (key == "PATH") pathVar = e.substr(e.find('=') + 1);
            }
            std::string cl = FindInPath("cl.exe", pathVar);
            if (!cl.empty()) found.push_back({Backend::MSVC, "Visual Studio (cl)", cl, true, env, true});
        }
    }
#endif
    // 3) bundled TinyCC (C only)
#ifndef _WIN32
    std::error_code ec;
#endif
    std::string tcc = GetEnvVar("PQ_TCC");
#ifdef _WIN32
    if (tcc.empty()) tcc = resourceDir + "/tcc/tcc.exe";
#else
    if (tcc.empty()) tcc = resourceDir + "/tcc/tcc";
#endif
    if (fs::exists(tcc, ec)) found.push_back({Backend::TCC, "TinyCC (built-in, C only)", tcc, false, {}, false});

    std::lock_guard<std::mutex> lock(gMutex);
    gCompilers = found;
    gActive = 0;
    gDone = true;
}

void StartDetection(const std::string& resourceDir) {
    if (gDetectThread.joinable()) return;
    gDetectThread = std::thread([resourceDir]() { Detect(resourceDir); });
    gDetectThread.detach();
}
void DetectNow(const std::string& resourceDir) { Detect(resourceDir); }
bool DetectionDone() { return gDone.load(); }
const std::vector<Compiler>& Compilers() { return gCompilers; }
int ActiveIndex() { return gActive.load(); }
void SetActive(int i) { if (i >= 0 && i < (int)gCompilers.size()) gActive = i; }
const Compiler* Active() {
    if (!gDone || gCompilers.empty()) return nullptr;
    return &gCompilers[std::min<int>(gActive, (int)gCompilers.size() - 1)];
}
} // namespace runner

// ========================================================= harness ======
static const char* kPrelude = R"PQ(#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __cplusplus
#include <stdbool.h>
#define nullptr NULL
#endif
#define examPrintf printf
)PQ";

static const char* kHarness = R"PQ(
static void pq_case(int id, int ok, const char* in, const char* exp, const char* got) {
    printf("\n@@CASE\x1f%d\x1f%d\x1f%s\x1f%s\x1f%s\n", id, ok, in, exp, got);
    fflush(stdout);
}
static const char* pq_i(long long v) {
    static char b[8][48]; static int k = 0; k = (k + 1) % 8;
    sprintf(b[k], "%lld", v); return b[k];
}
static const char* pq_b(int v) { return v ? "true" : "false"; }
static const char* pq_arr(const int* a, int n) {
    static char b[6][2048]; static int k = 0; k = (k + 1) % 6;
    char* p = b[k]; int i;
    p += sprintf(p, "{");
    for (i = 0; i < n && i < 200; i++) p += sprintf(p, "%s%d", i ? ", " : "", a[i]);
    sprintf(p, "}");
    return b[k];
}
static const char* pq_s(const char* s) {
    static char b[6][1024]; static int k = 0; k = (k + 1) % 6;
    if (s == NULL) return "(null)";
    snprintf(b[k], sizeof(b[k]), "\"%s\"", s); return b[k];
}
#define PQ_TOTAL(n) printf("\n@@TOTAL\x1f%d\n", (int)(n))
#define PQ_INT(id, in, expr, exp) do { long long g_ = (long long)(expr); long long e_ = (long long)(exp); \
    pq_case(id, g_ == e_, in, pq_i(e_), pq_i(g_)); } while (0)
#define PQ_BOOL(id, in, expr, exp) do { int g_ = (expr) ? 1 : 0; int e_ = (exp) ? 1 : 0; \
    pq_case(id, g_ == e_, in, pq_b(e_), pq_b(g_)); } while (0)
#define PQ_STR(id, in, got, exp) do { const char* g_ = (got); const char* e_ = (exp); \
    pq_case(id, g_ != NULL && strcmp(g_, e_) == 0, in, e_, g_ ? g_ : "(null)"); } while (0)
#define PQ_OUT_BEGIN(id, in, exp) printf("\n@@OUT\x1f%d\x1f%s\x1f%s\n", id, in, exp)
#define PQ_OUT_END(id) printf("\n@@END\x1f%d\n", id)
)PQ";

static bool HasUserMain(const std::string& code) {
    // ignore comments and strings
    std::string clean;
    for (size_t i = 0; i < code.size(); i++) {
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') { while (i < code.size() && code[i] != '\n') i++; }
        else if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') { i += 2; while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) i++; i++; }
        else if (code[i] == '"') { i++; while (i < code.size() && code[i] != '"') { if (code[i] == '\\') i++; i++; } }
        else clean += code[i];
    }
    return std::regex_search(clean, std::regex("\\bmain\\s*\\("));
}

std::string BuildSource(const Problem& p, const std::string& code, bool asCpp) {
    (void)asCpp;
    std::string s = kPrelude;
    bool userMain = HasUserMain(code);
    if (userMain) s += "#define main pq_student_main\n";
    s += "#line 1 \"solution.cpp\"\n";
    s += code;
    s += "\n";
    if (userMain) s += "#undef main\n";
    s += "#line 1 \"tests\"\n";
    s += kHarness;
    // tests may start with global helpers, separated from the main body by //@@MAIN
    std::string globals, body = p.tests;
    size_t split = p.tests.find("//@@MAIN");
    if (split != std::string::npos) { globals = p.tests.substr(0, split); body = p.tests.substr(split + 8); }
    s += globals;
    s += "int main(void) {\n    setvbuf(stdout, NULL, _IONBF, 0);\n";
    s += body;
    s += "\n    printf(\"\\n@@DONE\\n\");\n    return 0;\n}\n";
    return s;
}

// =========================================================== helpers ======
const char* StatusText(RunReport::Status s) {
    switch (s) {
        case RunReport::Accepted: return "Accepted";
        case RunReport::WrongAnswer: return "Wrong Answer";
        case RunReport::CompileError: return "Compile Error";
        case RunReport::RuntimeError: return "Runtime Error";
        case RunReport::TimeLimit: return "Time Limit Exceeded";
        case RunReport::NoCompiler: return "No compiler found";
        case RunReport::NeedsCpp: return "Needs a C++ compiler";
        case RunReport::RuleBroken: return "Rule broken";
    }
    return "";
}

static std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}

static std::string Unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { o += '\n'; i++; }
        else o += s[i];
    }
    return o;
}

// Normalises printed output: collapse spaces, trim lines, drop trailing blank lines.
static std::string NormaliseOutput(const std::string& s) {
    std::vector<std::string> lines;
    for (std::string l : Split(s, '\n')) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        std::string o;
        bool space = false;
        for (char c : l) {
            if (c == ' ' || c == '\t') space = true;
            else { if (space && !o.empty()) o += ' '; o += c; space = false; }
        }
        lines.push_back(o);
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    while (!lines.empty() && lines.front().empty()) lines.erase(lines.begin());
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) out += (i ? "\n" : "") + lines[i];
    return out;
}

static std::string StripCommentsAndStrings(const std::string& code) {
    std::string clean;
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') { while (i < code.size() && code[i] != '\n') i++; clean += '\n'; }
        else if (c == '/' && i + 1 < code.size() && code[i + 1] == '*') { i += 2; while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) i++; i++; }
        else if (c == '"' || c == '\'') { char q = c; i++; while (i < code.size() && code[i] != q) { if (code[i] == '\\') i++; i++; } clean += "\"\""; }
        else clean += c;
    }
    return clean;
}

static std::string CleanLog(std::string log, const std::string& workDir) {
    // remove absolute temp paths so messages read "solution.cpp:3: error ..."
    size_t pos;
    std::string wd = workDir;
    for (int pass = 0; pass < 2; pass++) {
        while (!wd.empty() && (pos = log.find(wd)) != std::string::npos) {
            size_t end = pos + wd.size();
            if (end < log.size() && (log[end] == '/' || log[end] == '\\')) end++;
            log.erase(pos, end - pos);
        }
        for (auto& ch : wd) if (ch == '/') ch = '\\';
    }
    // MSVC echoes the file name on its own line
    std::string out;
    for (const std::string& l : Split(log, '\n')) {
        std::string t = l;
        if (!t.empty() && t.back() == '\r') t.pop_back();
        if (t == "main.cpp" || t == "main.c" || t.empty()) continue;
        out += t + "\n";
    }
    // When the student's code has errors, the test harness usually fails too
    // ("pq_case was not declared..."). Those follow-on errors only confuse, so
    // keep just the messages about solution.cpp (and their context lines).
    if (out.find("solution.c") != std::string::npos) {
        std::string kept;
        bool keep = false;
        for (const std::string& l : Split(out, '\n')) {
            if (l.rfind("solution.c", 0) == 0) keep = true;
            else if (l.rfind("tests", 0) == 0 || l.rfind("main.c", 0) == 0 || l.rfind("In file included", 0) == 0 ||
                     l.find(": In function") != std::string::npos) keep = l.rfind("solution.c", 0) == 0;
            if (keep && !l.empty()) kept += l + "\n";
        }
        if (!kept.empty()) out = kept;
    }
    return out;
}

// ============================================================ running ======
RunReport RunCode(const Problem& p, const std::string& code, const Compiler* c) {
    RunReport rep;
    if (!c) { rep.status = RunReport::NoCompiler; return rep; }
    rep.compilerLabel = c->label;
    if (p.cppOnly && !c->cpp) {
        rep.status = RunReport::NeedsCpp;
        rep.compileLog = "This problem uses C++ features (references, classes or new/delete).\n"
                         "The built-in TinyCC compiler only understands C.\n"
                         "Install Visual Studio (Desktop development with C++) or g++ and restart the app.";
        return rep;
    }

    static std::atomic<int> counter{0};
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec) / ("segfault-" + std::to_string((long long)std::chrono::steady_clock::now().time_since_epoch().count() % 1000000007) + "-" + std::to_string(counter++));
    fs::create_directories(dir, ec);
    std::string srcName = c->cpp ? "main.cpp" : "main.c";
#ifdef _WIN32
    std::string exeName = "prog.exe";
#else
    std::string exeName = "prog";
#endif
    {
        std::ofstream f(dir / srcName, std::ios::binary);
        f << BuildSource(p, code, c->cpp);
    }

    // ---- compile -------------------------------------------------------
    std::vector<std::string> args;
    int compileTimeout = 60000;
    switch (c->kind) {
        case Backend::GCC:
        case Backend::Clang:
            args = {c->exe, "-std=c++17", "-O1", "-w", "-fdiagnostics-color=never", "-o", exeName, srcName};
#ifdef _WIN32
            args.insert(args.begin() + 1, {"-static-libgcc", "-static-libstdc++"});
#endif
            break;
        case Backend::MSVC:
            args = {c->exe, "/nologo", "/EHsc", "/std:c++17", "/W0", "/Fe:" + exeName, srcName};
            break;
        case Backend::TCC:
            args = {c->exe, "-w", "-o", exeName, srcName};
            compileTimeout = 20000;
            break;
    }
    ProcResult cr = RunProcess(args, dir.string(), compileTimeout, c->hasEnv ? &c->env : nullptr);
    rep.compileMs = cr.ms;
    if (!cr.started || cr.exitCode != 0 || !fs::exists(dir / exeName, ec)) {
        rep.status = RunReport::CompileError;
        rep.compileLog = cr.started ? CleanLog(cr.output, dir.string()) : "Could not start the compiler: " + c->exe;
        if (cr.timedOut) rep.compileLog = "The compiler took too long.";
        std::smatch m;
        std::string log = rep.compileLog;
        std::regex re("solution\\.cpp[:(]([0-9]+)");
        for (auto it = std::sregex_iterator(log.begin(), log.end(), re); it != std::sregex_iterator(); ++it) {
            int ln = std::stoi((*it)[1].str());
            if (std::find(rep.errorLines.begin(), rep.errorLines.end(), ln) == rep.errorLines.end()) rep.errorLines.push_back(ln);
        }
        fs::remove_all(dir, ec);
        return rep;
    }

    // ---- run -------------------------------------------------------------
    ProcResult rr = RunProcess({(dir / exeName).string()}, dir.string(), 4000);
    rep.runMs = rr.ms;
    rep.exitCode = rr.exitCode;
    fs::remove_all(dir, ec);

    // ---- parse ------------------------------------------------------------
    bool done = false;
    bool capturing = false;
    CaseResult capCase;
    std::string capText;
    std::string userOut;
    for (std::string line : Split(rr.output, '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("@@TOTAL\x1f", 0) == 0) { rep.total = std::atoi(line.c_str() + 8); continue; }
        if (line == "@@DONE") { done = true; continue; }
        if (line.rfind("@@CASE\x1f", 0) == 0) {
            auto f = Split(line, '\x1f');
            if (f.size() >= 6) {
                CaseResult cs;
                cs.id = std::atoi(f[1].c_str()); cs.ok = f[2] == "1"; cs.input = f[3]; cs.expected = f[4];
                cs.got = f[5];
                for (size_t k = 6; k < f.size(); k++) cs.got += "|" + f[k];
                rep.cases.push_back(cs);
            }
            continue;
        }
        if (line.rfind("@@OUT\x1f", 0) == 0) {
            auto f = Split(line, '\x1f');
            capturing = true; capText.clear();
            capCase = CaseResult();
            capCase.id = f.size() > 1 ? std::atoi(f[1].c_str()) : 0;
            capCase.input = f.size() > 2 ? f[2] : "";
            capCase.expected = f.size() > 3 ? Unescape(f[3]) : "";
            continue;
        }
        if (line.rfind("@@END\x1f", 0) == 0 && capturing) {
            capturing = false;
            capCase.got = NormaliseOutput(capText);
            capCase.expected = NormaliseOutput(capCase.expected);
            capCase.ok = capCase.got == capCase.expected;
            rep.cases.push_back(capCase);
            continue;
        }
        if (capturing) capText += line + "\n";
        else userOut += line + "\n";
    }
    if (capturing) {   // crashed while printing
        capCase.got = NormaliseOutput(capText);
        capCase.expected = NormaliseOutput(capCase.expected);
        capCase.ok = false;
        rep.cases.push_back(capCase);
    }
    rep.stdoutText = NormaliseOutput(userOut);
    if (rep.total == 0) rep.total = (int)rep.cases.size();
    for (const CaseResult& cs : rep.cases) rep.passed += cs.ok;
    // cases that never ran (crash / timeout)
    for (int id = (int)rep.cases.size() + 1; id <= rep.total; id++) {
        CaseResult cs; cs.id = id; cs.ok = false; cs.ran = false; cs.got = "(not run)";
        rep.cases.push_back(cs);
    }

    if (rr.timedOut) rep.status = RunReport::TimeLimit;
    else if (!done) rep.status = RunReport::RuntimeError;
    else rep.status = rep.passed == rep.total ? RunReport::Accepted : RunReport::WrongAnswer;

    if (!p.forbid.empty()) {
        std::string clean = StripCommentsAndStrings(code);
        for (const std::string& tok : p.forbid) {
            bool word = std::isalpha((unsigned char)tok[0]);
            bool hit = word ? std::regex_search(clean, std::regex("\\b" + tok + "\\b"))
                            : clean.find(tok) != std::string::npos;
            if (hit) {
                rep.ruleMessage = "Rule broken: '" + tok + "' is not allowed here. " + p.ruleText;
                if (rep.status == RunReport::Accepted) rep.status = RunReport::RuleBroken;
                break;
            }
        }
    }
    return rep;
}

// ============================================================ RunJob ======
RunJob::~RunJob() {
    if (worker.joinable()) worker.join();
}

void RunJob::Start(const Problem& p, const std::string& code) {
    if (worker.joinable()) worker.join();
    state = std::make_shared<State>();
    state->started = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::shared_ptr<State> st = state;
    Problem copy = p;
    const Compiler* active = runner::Active();
    Compiler comp = active ? *active : Compiler{Backend::GCC, "", "", true, {}, false};
    bool has = active != nullptr;
    worker = std::thread([st, copy, code, comp, has]() {
        st->report = RunCode(copy, code, has ? &comp : nullptr);
        st->done = true;
    });
}

RunReport RunJob::Take() {
    RunReport r;
    if (state && state->done) { r = state->report; }
    if (worker.joinable()) worker.join();
    state.reset();
    return r;
}

float RunJob::Elapsed() const {
    if (!state) return 0;
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return (float)(now - state->started);
}
