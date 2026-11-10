// ============================================================================
// files.h  -  Reading files the student picks, and the "open file" window.
// Paths here are UTF-8 (what the file window and drag and drop give us), so
// names like "3 año" work; raylib's own loaders can't open those on Windows.
// Kept free of raylib so <windows.h> can be used in files.cpp.
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace files {

// Reads a whole file. False if it can't be opened or is bigger than maxBytes.
bool ReadAll(const std::string& utf8Path, std::vector<unsigned char>& out, size_t maxBytes);

// Shows the system "open file" window for pictures. True and the chosen path
// when the student picked one; always false where there is no such window.
bool PickImage(void* ownerWindow, std::string& utf8Path);

} // namespace files
