#include "files.h"
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

static std::wstring Wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string Utf8(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}
#endif

namespace files {

bool ReadAll(const std::string& utf8Path, std::vector<unsigned char>& out, size_t maxBytes) {
#ifdef _WIN32
    FILE* f = _wfopen(Wide(utf8Path).c_str(), L"rb");
#else
    FILE* f = std::fopen(utf8Path.c_str(), "rb");
#endif
    if (!f) return false;
    out.clear();
    unsigned char buf[65536];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (out.size() + n > maxBytes) { ok = false; break; }
        out.insert(out.end(), buf, buf + n);
    }
    std::fclose(f);
    return ok;
}

bool PickImage(void* ownerWindow, std::string& utf8Path) {
#ifdef _WIN32
    wchar_t file[MAX_PATH * 4] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ownerWindow;
    ofn.lpstrFilter = L"Pictures (*.png, *.jpg, *.bmp, *.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file) / sizeof(file[0]);
    ofn.lpstrTitle = L"Choose a profile picture";
    // NOCHANGEDIR: the app finds its resources relative to the current folder
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return false;
    utf8Path = Utf8(file);
    return true;
#else
    (void)ownerWindow; (void)utf8Path;
    return false;
#endif
}

} // namespace files
