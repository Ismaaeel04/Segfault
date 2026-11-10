#include "avatar.h"
#include "app.h"
#include "auth.h"
#include "avatar_image.h"
#include "files.h"
#include "theme.h"
#include "ui.h"
#include <cctype>
#include <cstdio>
#include <map>

using namespace ui;

namespace avatar {

namespace {

struct Pair { Color a, b; const char* name; };
const Pair kPairs[kColourCount] = {
    {{0, 0, 0, 0}, {0, 0, 0, 0}, "Theme"},
    {{139, 92, 246, 255}, {45, 212, 191, 255}, "Violet"},
    {{37, 99, 235, 255}, {34, 211, 238, 255}, "Ocean"},
    {{22, 163, 74, 255}, {163, 230, 53, 255}, "Lime"},
    {{234, 88, 12, 255}, {244, 114, 182, 255}, "Sunset"},
    {{220, 38, 38, 255}, {251, 146, 60, 255}, "Fire"},
    {{219, 39, 119, 255}, {167, 139, 250, 255}, "Berry"},
    {{13, 148, 136, 255}, {59, 130, 246, 255}, "Lagoon"},
    {{202, 138, 4, 255}, {234, 88, 12, 255}, "Gold"},
};

struct Loaded { Texture2D tex = {}; bool checked = false; };
std::map<std::string, Loaded> gCache;   // by lowercase username

std::string Key(const std::string& user) {
    std::string k = user;
    for (char& c : k) c = (char)std::tolower((unsigned char)c);
    return k;
}

std::string PicturePath(const std::string& user) {
    return auth::UserStore(app.dataDir + "segfault_users").UserDir(user) + "avatar.png";
}

const Texture2D* Picture(const std::string& user) {
    if (user.empty()) return nullptr;
    Loaded& l = gCache[Key(user)];
    if (!l.checked) {
        l.checked = true;
        std::string path = PicturePath(user);
        if (FileExists(path.c_str())) {
            Image img = LoadImage(path.c_str());
            if (img.data) {
                img = SquareCrop(img);
                RoundMask(img);
                l.tex = LoadTextureFromImage(img);
                UnloadImage(img);
                GenTextureMipmaps(&l.tex);
                SetTextureFilter(l.tex, TEXTURE_FILTER_TRILINEAR);
            }
        }
    }
    return l.tex.id ? &l.tex : nullptr;
}

void Forget(const std::string& user) {
    auto it = gCache.find(Key(user));
    if (it == gCache.end()) return;
    if (it->second.tex.id) UnloadTexture(it->second.tex);
    gCache.erase(it);
}

} // namespace

const char* ColourName(int index) { return kPairs[index >= 0 && index < kColourCount ? index : 0].name; }

void DrawInitial(Rectangle r, const std::string& user, int colour) {
    Color a = col::accent, b = Mix(col::accent, col::accent2, 0.7f);
    if (colour > 0 && colour < kColourCount) { a = Tone(kPairs[colour].a); b = Tone(kPairs[colour].b); }
    GradientRoundRect(r, r.width / 2, a, b);
    std::string initial(1, user.empty() ? 'S' : (char)std::toupper((unsigned char)user[0]));
    TextCenter(initial, r, r.width * 0.45f, OnColor(Mix(a, b, 0.5f)), F_DISPLAY);
}

void Draw(Rectangle r, const std::string& user, int colour) {
    if (const Texture2D* t = Picture(user)) {
        DrawTexturePro(*t, {0, 0, (float)t->width, (float)t->height}, r, {0, 0}, 0, WHITE);
        DrawRing({r.x + r.width / 2, r.y + r.height / 2}, r.width / 2 - 0.5f, r.width / 2 + 0.7f, 0, 360, 48, Alpha(col::border, 0.9f));
    } else {
        DrawInitial(r, user, colour);
    }
}

bool HasPicture(const std::string& user) { return Picture(user) != nullptr; }

std::string SetPicture(const std::string& user, Image src) {
    if (!src.data || src.width < 1 || src.height < 1) return "That picture is empty";
    Image img = SquareCrop(src);
    std::string path = PicturePath(user);
    bool ok = ExportImage(img, path.c_str());
    UnloadImage(img);
    Forget(user);
    return ok ? "" : "Could not save the picture. Is this folder read-only?";
}

std::string SetPictureFromFile(const std::string& user, const std::string& utf8Path) {
    std::vector<unsigned char> data;
    if (!files::ReadAll(utf8Path, data, kMaxFileBytes))
        return data.empty() ? "Could not open that file" : "That file is too big (20 MB at most)";
    std::string ext = GetFileExtension(utf8Path.c_str()) ? GetFileExtension(utf8Path.c_str()) : "";
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == ".jpeg") ext = ".jpg";
    Image img = LoadImageFromMemory(ext.c_str(), data.data(), (int)data.size());
    if (!img.data) return "That isn't a picture Segfault can read. Use PNG, JPG, BMP or GIF.";
    return SetPicture(user, img);
}

void RemovePicture(const std::string& user) {
    std::remove(PicturePath(user).c_str());
    Forget(user);
}

void ForgetAll() {
    for (auto& e : gCache) if (e.second.tex.id) UnloadTexture(e.second.tex);
    gCache.clear();
}

} // namespace avatar
