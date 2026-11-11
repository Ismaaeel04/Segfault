// ============================================================================
// http.h  -  One HTTPS POST with the answer read as it arrives (for the
// Claude API's streamed replies). Uses WinHTTP on Windows, which ships with
// the system, so there is nothing extra to install. Free of raylib (windows.h).
// ============================================================================
#pragma once
#include <functional>
#include <string>
#include <vector>

struct HttpResult {
    int status = 0;          // HTTP status, 0 = no answer (offline, cancelled, ...)
    std::string error;       // why there is no answer
    std::string body;        // the whole body when status isn't 200
};

// headers: "Name: value". onData gets each piece of a 200 answer; return false
// to stop reading (the request is cancelled).
HttpResult HttpPost(const std::string& host, const std::string& path, const std::vector<std::string>& headers,
                    const std::string& body, const std::function<bool(const char*, size_t)>& onData);

// A GET; the whole answer ends up in body (any status).
HttpResult HttpGet(const std::string& host, const std::string& path, const std::vector<std::string>& headers);
