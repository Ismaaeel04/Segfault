// ============================================================================
// process.h  -  Run an external program with a timeout and capture its
// output (stdout + stderr). Used to compile and run the student's code.
// This file must not include raylib.h (windows.h clashes with it).
// ============================================================================
#pragma once
#include <string>
#include <vector>

struct ProcResult {
    bool started = false;
    bool timedOut = false;
    bool outputTruncated = false;
    int exitCode = -1;
    std::string output;
    double ms = 0;
};

// args[0] is the program (searched in PATH if it has no directory).
// env: optional full environment ("KEY=VALUE" entries) for the child.
// stdinFile: a file the child reads as its standard input ("" = nothing).
ProcResult RunProcess(const std::vector<std::string>& args, const std::string& workDir, int timeoutMs,
                      const std::vector<std::string>* env = nullptr, const std::string& stdinFile = "");

// Captures the environment after running a Windows batch file (vcvars64.bat).
std::vector<std::string> CaptureBatchEnvironment(const std::string& batchFile);

// Finds an executable in a PATH-style list ("a;b;c" on Windows, "a:b" elsewhere).
std::string FindInPath(const std::string& exe, const std::string& pathList);
std::string GetEnvVar(const std::string& name);
