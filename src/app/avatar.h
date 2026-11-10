// ============================================================================
// avatar.h  -  Students' profile pictures, and the coloured circle with their
// initial for those who have none. The picture is saved as avatar.png in the
// student's folder.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>
#include <vector>

namespace avatar {

constexpr int kColourCount = 9;   // 0 = the theme's colours, 1-8 = fixed pairs
const char* ColourName(int index);

// The picture if the student has one, otherwise their initial on their colour.
void Draw(Rectangle r, const std::string& user, int colour);
// Always the initial on the colour (for choosing a colour).
void DrawInitial(Rectangle r, const std::string& user, int colour);

bool HasPicture(const std::string& user);
// Makes `src` the student's picture (centre square, 256 px). "" or an error.
std::string SetPicture(const std::string& user, Image src);
// From a file the student chose or dropped (any path, any size up to 20 MB).
std::string SetPictureFromFile(const std::string& user, const std::string& utf8Path);
void RemovePicture(const std::string& user);

// Forgets loaded pictures, so changes made by other students show up.
void ForgetAll();

} // namespace avatar
