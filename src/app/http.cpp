// ============================================================================
// http.cpp  -  HTTPS POST with WinHTTP (see http.h).
// ============================================================================
#include "http.h"
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

static std::wstring Wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string LastError(const char* what) {
    DWORD e = GetLastError();
    switch (e) {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_CANNOT_CONNECT: return "No internet connection (can't reach the server).";
        case ERROR_WINHTTP_TIMEOUT: return "The server took too long to answer.";
        case ERROR_WINHTTP_CONNECTION_ERROR: return "The connection was dropped.";
        case ERROR_WINHTTP_SECURE_FAILURE: return "Secure connection failed (check the computer's date and time).";
        case ERROR_WINHTTP_OPERATION_CANCELLED: return "Stopped.";
    }
    return std::string(what) + " failed (error " + std::to_string((unsigned long)e) + ").";
}

static HttpResult Request(const wchar_t* method, const std::string& host, const std::string& path, const std::vector<std::string>& headers,
                          const std::string& body, const std::function<bool(const char*, size_t)>* onData) {
    HttpResult r;
    HINTERNET session = WinHttpOpen(L"SegFault/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)   // Windows before 8.1 has no automatic proxy
        session = WinHttpOpen(L"SegFault/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { r.error = LastError("WinHttpOpen"); return r; }
    // resolve, connect, send, receive (ms). Receive is the gap between two pieces of the stream.
    WinHttpSetTimeouts(session, 15000, 15000, 30000, 120000);
    HINTERNET conn = WinHttpConnect(session, Wide(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, method, Wide(path).c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    auto close = [&]() {
        if (req) WinHttpCloseHandle(req);
        if (conn) WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
    };
    if (!req) { r.error = LastError("WinHttpOpenRequest"); close(); return r; }
    std::wstring hs;
    for (const std::string& h : headers) hs += Wide(h) + L"\r\n";
    if (!hs.empty()) WinHttpAddRequestHeaders(req, hs.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) ||
        !WinHttpReceiveResponse(req, nullptr)) {
        r.error = LastError("The request");
        close();
        return r;
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    r.status = (int)status;

    std::vector<char> buf(16384);
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) { r.error = LastError("Reading the answer"); break; }
        if (avail == 0) break;   // the end
        DWORD got = 0;
        if (!WinHttpReadData(req, buf.data(), (DWORD)std::min<size_t>(buf.size(), avail), &got)) { r.error = LastError("Reading the answer"); break; }
        if (got == 0) break;
        if (r.status != 200 || !onData) { if (r.body.size() < (1u << 20)) r.body.append(buf.data(), got); continue; }
        if (!(*onData)(buf.data(), got)) { r.error = "Stopped."; break; }
    }
    close();
    return r;
}

HttpResult HttpPost(const std::string& host, const std::string& path, const std::vector<std::string>& headers,
                    const std::string& body, const std::function<bool(const char*, size_t)>& onData) {
    return Request(L"POST", host, path, headers, body, &onData);
}

HttpResult HttpGet(const std::string& host, const std::string& path, const std::vector<std::string>& headers) {
    return Request(L"GET", host, path, headers, "", nullptr);
}

#else

HttpResult HttpPost(const std::string&, const std::string&, const std::vector<std::string>&,
                    const std::string&, const std::function<bool(const char*, size_t)>&) {
    HttpResult r;
    r.error = "SegFault AI uses WinHTTP, so for now it only works on Windows.";
    return r;
}

HttpResult HttpGet(const std::string&, const std::string&, const std::vector<std::string>&) {
    HttpResult r;
    r.error = "SegFault AI uses WinHTTP, so for now it only works on Windows.";
    return r;
}

#endif
