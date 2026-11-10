// ============================================================================
// runner.h  -  Compiles and runs the student's code against a problem's tests.
//
// Compilers are detected once at start-up, in this order:
//   g++  ->  clang++  ->  Visual Studio (cl.exe)  ->  bundled TinyCC (C only)
// The code is compiled together with a small test harness. The harness prints
// one machine-readable line per test case, which is parsed into a RunReport.
// ============================================================================
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct Problem;

enum class Backend { GCC, Clang, MSVC, TCC };

struct Compiler {
    Backend kind;
    std::string label;          // shown in the UI
    std::string exe;
    bool cpp;                   // false = C only (TinyCC)
    std::vector<std::string> env;
    bool hasEnv = false;
};

namespace runner {
    void StartDetection(const std::string& resourceDir);   // runs in the background
    void DetectNow(const std::string& resourceDir);        // blocking (self test)
    bool DetectionDone();
    const std::vector<Compiler>& Compilers();
    int ActiveIndex();
    void SetActive(int index);
    void SetPreferredLabel(const std::string& label);
    const std::string& PreferredLabel();
    const Compiler* Active();
}

struct CaseResult {
    int id = 0;
    bool ok = false;
    bool ran = true;
    std::string input, expected, got;
};

struct RunReport {
    enum Status { Accepted, WrongAnswer, CompileError, RuntimeError, TimeLimit, NoCompiler, NeedsCpp, RuleBroken };
    Status status = NoCompiler;
    std::vector<CaseResult> cases;
    int total = 0, passed = 0;
    std::string compileLog;
    std::vector<int> errorLines;   // 1-based lines in the student's code
    std::string stdoutText;        // anything the student printed
    std::string compilerLabel;
    std::string ruleMessage;
    double compileMs = 0, runMs = 0;
    int exitCode = 0;
};

const char* StatusText(RunReport::Status s);

// Blocking: compile + run. Safe to call from a worker thread.
RunReport RunCode(const Problem& p, const std::string& code, const Compiler* c);

// Background job wrapper used by the UI so it never freezes.
class RunJob {
public:
    ~RunJob();
    void Start(const Problem& p, const std::string& code);
    bool Busy() const { return state && !state->done.load(); }
    bool Ready() const { return state && state->done.load(); }
    RunReport Take();
    float Elapsed() const;
private:
    struct State { std::atomic<bool> done{false}; RunReport report; double started = 0; };
    std::shared_ptr<State> state;
    std::thread worker;
};

// Full source sent to the compiler (exposed for the self test / debugging).
std::string BuildSource(const Problem& p, const std::string& code, bool asCpp);
