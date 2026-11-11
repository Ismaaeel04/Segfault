// ============================================================================
// process.cpp  -  Platform code for running child processes with a timeout.
// ============================================================================
#include "process.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>

static const size_t kMaxOutput = 512 * 1024;   // stop runaway printing

std::string GetEnvVar(const std::string& name) {
    const char* v = std::getenv(name.c_str());
    return v ? v : "";
}

std::string FindInPath(const std::string& exe, const std::string& pathList) {
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    size_t start = 0;
    while (start <= pathList.size()) {
        size_t end = pathList.find(sep, start);
        if (end == std::string::npos) end = pathList.size();
        std::string dir = pathList.substr(start, end - start);
        if (!dir.empty()) {
            std::error_code ec;
            std::filesystem::path p = std::filesystem::path(dir) / exe;
            if (std::filesystem::is_regular_file(p, ec)) return p.string();
        }
        start = end + 1;
    }
    return "";
}

#ifdef _WIN32
// --------------------------------------------------------------- Windows ---
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static std::string QuoteArg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string out = "\"";
    int backslashes = 0;
    for (char c : a) {
        if (c == '\\') { backslashes++; continue; }
        if (c == '"') { out.append(backslashes * 2 + 1, '\\'); out += '"'; }
        else { out.append(backslashes, '\\'); out += c; }
        backslashes = 0;
    }
    out.append(backslashes * 2, '\\');
    return out + "\"";
}

ProcResult RunProcess(const std::vector<std::string>& args, const std::string& workDir, int timeoutMs,
                      const std::vector<std::string>* env, const std::string& stdinFile) {
    ProcResult r;
    if (args.empty()) return r;
    auto t0 = std::chrono::steady_clock::now();

    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return r;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE nulIn = CreateFileA(stdinFile.empty() ? "NUL" : stdinFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::string cmd;
    for (size_t i = 0; i < args.size(); i++) cmd += (i ? " " : "") + QuoteArg(args[i]);
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    std::vector<char> envBlock;
    if (env) {
        for (const std::string& e : *env) { envBlock.insert(envBlock.end(), e.begin(), e.end()); envBlock.push_back('\0'); }
        envBlock.push_back('\0');
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = nulIn;
    PROCESS_INFORMATION pi = {};
    // A crashing program must end at once: no "stopped working" window (the
    // child inherits this error mode), or it would sit there until the time limit.
    static const bool quietCrashes = (SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS), true);
    (void)quietCrashes;
    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 6; attempt++) {
        ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                            env ? envBlock.data() : nullptr, workDir.empty() ? nullptr : workDir.c_str(), &si, &pi);
        if (ok) break;
        // a program that was just linked can be locked for a moment (antivirus scan): try again
        DWORD e = GetLastError();
        if (e != ERROR_SHARING_VIOLATION && e != ERROR_ACCESS_DENIED && e != ERROR_LOCK_VIOLATION) break;
        Sleep(150);
    }
    CloseHandle(writePipe);
    if (nulIn != INVALID_HANDLE_VALUE) CloseHandle(nulIn);
    if (!ok) { CloseHandle(readPipe); return r; }
    r.started = true;

    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD got = 0;
            if (!ReadFile(readPipe, buf, (DWORD)std::min<size_t>(sizeof(buf), avail), &got, nullptr) || got == 0) break;
            if (r.output.size() < kMaxOutput) r.output.append(buf, got);
            else r.outputTruncated = true;
        }
        DWORD w = WaitForSingleObject(pi.hProcess, 5);
        double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (w == WAIT_OBJECT_0) break;
        // Over the limit means over it in CPU time, so a program held up before it
        // even starts (an antivirus scan of a new .exe) isn't cut off for nothing.
        // Something that waits without using the CPU still stops at 4x the limit.
        bool over = false;
        if (elapsed > timeoutMs) {
            FILETIME c0, e0, k0, u0;
            double cpuMs = 0;
            if (GetProcessTimes(pi.hProcess, &c0, &e0, &k0, &u0)) {
                ULARGE_INTEGER k, u;
                k.LowPart = k0.dwLowDateTime; k.HighPart = k0.dwHighDateTime;
                u.LowPart = u0.dwLowDateTime; u.HighPart = u0.dwHighDateTime;
                cpuMs = (k.QuadPart + u.QuadPart) / 10000.0;
            }
            over = cpuMs > timeoutMs * 0.5 || elapsed > timeoutMs * 4.0;
        }
        if (over || r.outputTruncated) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 2000);
            r.timedOut = !r.outputTruncated;
            break;
        }
    }
    // drain what is left
    DWORD avail = 0;
    while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
        DWORD got = 0;
        if (!ReadFile(readPipe, buf, (DWORD)std::min<size_t>(sizeof(buf), avail), &got, nullptr) || got == 0) break;
        if (r.output.size() < kMaxOutput) r.output.append(buf, got);
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    r.exitCode = (int)code;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readPipe);
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::vector<std::string> CaptureBatchEnvironment(const std::string& batchFile) {
    std::string comspec = GetEnvVar("ComSpec");
    if (comspec.empty()) comspec = "cmd.exe";
    // cmd /c ""batch" >nul 2>&1 && set"
    std::string line = "\"\"" + batchFile + "\" >nul 2>&1 && set\"";
    std::vector<char> cmdBuf;
    std::string full = "\"" + comspec + "\" /s /c " + line;
    // RunProcess quotes arguments itself, so call CreateProcess through a tiny wrapper
    std::vector<std::string> env;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return env;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    cmdBuf.assign(full.begin(), full.end());
    cmdBuf.push_back('\0');
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!ok) { CloseHandle(readPipe); return env; }
    std::string out;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &got, nullptr) && got > 0) out.append(buf, got);
    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readPipe);
    size_t start = 0;
    while (start < out.size()) {
        size_t end = out.find('\n', start);
        if (end == std::string::npos) end = out.size();
        std::string l = out.substr(start, end - start);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.find('=') != std::string::npos && l.find('=') > 0) env.push_back(l);
        start = end + 1;
    }
    return env;
}

#else
// ----------------------------------------------------------------- POSIX ---
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

ProcResult RunProcess(const std::vector<std::string>& args, const std::string& workDir, int timeoutMs,
                      const std::vector<std::string>* env, const std::string& stdinFile) {
    ProcResult r;
    if (args.empty()) return r;
    auto t0 = std::chrono::steady_clock::now();
    // build argv/envp BEFORE fork (no allocation in the child)
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    std::vector<char*> envp;
    if (env) {
        for (const auto& e : *env) envp.push_back(const_cast<char*>(e.c_str()));
        envp.push_back(nullptr);
    }
    int fds[2];
    if (pipe(fds) != 0) return r;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return r; }
    if (pid == 0) {
        setpgid(0, 0);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[0]);
        close(fds[1]);
        int devnull = open(stdinFile.empty() ? "/dev/null" : stdinFile.c_str(), O_RDONLY);
        if (devnull >= 0) dup2(devnull, 0);
        if (!workDir.empty() && chdir(workDir.c_str()) != 0) _exit(127);
        if (env) environ = envp.data();
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fds[1]);
    r.started = true;
    char buf[4096];
    bool exited = false;
    int status = 0;
    for (;;) {
        struct pollfd p = {fds[0], POLLIN, 0};
        int pr = poll(&p, 1, 5);
        if (pr > 0) {
            ssize_t n = read(fds[0], buf, sizeof(buf));
            if (n > 0) {
                if (r.output.size() < kMaxOutput) r.output.append(buf, (size_t)n);
                else r.outputTruncated = true;
            } else if (n == 0) {
                // EOF: wait for the child below
                if (waitpid(pid, &status, 0) == pid) exited = true;
                break;
            }
        }
        if (!exited && waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            // drain
            ssize_t n;
            while ((n = read(fds[0], buf, sizeof(buf))) > 0)
                if (r.output.size() < kMaxOutput) r.output.append(buf, (size_t)n);
            break;
        }
        double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (elapsed > timeoutMs || r.outputTruncated) {
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            exited = true;
            r.timedOut = !r.outputTruncated;
            break;
        }
    }
    close(fds[0]);
    if (WIFEXITED(status)) r.exitCode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) r.exitCode = 128 + WTERMSIG(status);
    if (r.exitCode == 127 && r.output.empty()) r.started = false;   // exec failed
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::vector<std::string> CaptureBatchEnvironment(const std::string&) { return {}; }
#endif
