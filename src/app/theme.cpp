#include "theme.h"
#include "ui.h"
#include <cmath>

namespace ui {

namespace col {
    Color bg, bg2, panel, panelHi, border, heading, text, dim, faint;
    Color accent, accent2, warn, danger, success, pointer, heap, stack;
    Color codeBg, codeLine, shadow;
    Color synText, synType, synKey, synNum, synStr, synCom, synFn, synPre, synPunct;
}

static int gCurrent = 0;

const std::vector<Theme>& Themes() {
    // Order of each entry: surfaces (bg, bar, panel, raised panel, border),
    // text (heading, body, secondary, faint), accents and status colours,
    // code background, then syntax colours (text, type, keyword, number,
    // string, comment, function, preprocessor, punctuation).
    // The community palettes (all MIT licensed) use their published colours;
    // a few are made a little brighter or darker where --selftest found them
    // too faint to read. Following common dark-UI advice: no pure black
    // backgrounds or pure white text, and the two accents and the status
    // colours (warning, success) stay distinct from one another.
    static const std::vector<Theme> themes = {
        {"midnight", "Midnight", "The original violet", false,
         {14, 18, 32, 255}, {18, 23, 38, 255}, {22, 28, 45, 255}, {33, 44, 64, 255}, {54, 68, 90, 255},
         {238, 241, 250, 255}, {222, 228, 238, 255}, {150, 165, 186, 255}, {112, 127, 150, 255},
         {142, 102, 240, 255}, {58, 200, 182, 255}, {251, 191, 36, 255}, {248, 113, 113, 255}, {74, 222, 128, 255},
         {251, 146, 60, 255}, {192, 132, 252, 255}, {96, 165, 250, 255},
         {15, 19, 32, 255},
         {214, 220, 236, 255}, {103, 232, 249, 255}, {196, 167, 255, 255}, {253, 186, 116, 255},
         {134, 239, 172, 255}, {120, 132, 158, 255}, {250, 224, 120, 255}, {244, 143, 177, 255}, {150, 160, 186, 255}},

        {"catppuccin", "Catppuccin", "Soft pastels (Mocha)", false,
         {17, 17, 27, 255}, {24, 24, 37, 255}, {30, 30, 46, 255}, {49, 50, 68, 255}, {69, 71, 90, 255},
         {239, 241, 253, 255}, {205, 214, 244, 255}, {166, 173, 200, 255}, {127, 132, 156, 255},
         {203, 166, 247, 255}, {148, 226, 213, 255}, {249, 226, 175, 255}, {243, 139, 168, 255}, {166, 227, 161, 255},
         {250, 179, 135, 255}, {245, 194, 231, 255}, {137, 180, 250, 255},
         {24, 24, 37, 255},
         {205, 214, 244, 255}, {249, 226, 175, 255}, {203, 166, 247, 255}, {250, 179, 135, 255},
         {166, 227, 161, 255}, {147, 153, 178, 255}, {137, 180, 250, 255}, {245, 194, 231, 255}, {147, 153, 178, 255}},

        {"nord", "Nord", "Calm arctic blues", false,
         {36, 41, 51, 255}, {42, 47, 58, 255}, {46, 52, 64, 255}, {59, 66, 82, 255}, {76, 86, 106, 255},
         {236, 239, 244, 255}, {229, 233, 240, 255}, {184, 192, 207, 255}, {123, 136, 161, 255},
         {136, 192, 208, 255}, {180, 142, 173, 255}, {235, 203, 139, 255}, {222, 122, 132, 255}, {163, 190, 140, 255},
         {208, 135, 112, 255}, {180, 142, 173, 255}, {129, 161, 193, 255},
         {40, 45, 56, 255},
         {216, 222, 233, 255}, {143, 188, 187, 255}, {129, 161, 193, 255}, {180, 142, 173, 255},
         {163, 190, 140, 255}, {128, 140, 165, 255}, {136, 192, 208, 255}, {208, 135, 112, 255}, {180, 188, 204, 255}},

        {"dracula", "Dracula", "Vivid neon", false,
         {33, 34, 44, 255}, {30, 31, 41, 255}, {40, 42, 54, 255}, {52, 55, 70, 255}, {68, 71, 90, 255},
         {248, 248, 242, 255}, {240, 240, 234, 255}, {190, 194, 216, 255}, {122, 135, 186, 255},
         {189, 147, 249, 255}, {255, 121, 198, 255}, {241, 250, 140, 255}, {255, 85, 85, 255}, {80, 250, 123, 255},
         {255, 184, 108, 255}, {255, 121, 198, 255}, {139, 233, 253, 255},
         {36, 37, 48, 255},
         {248, 248, 242, 255}, {139, 233, 253, 255}, {255, 121, 198, 255}, {189, 147, 249, 255},
         {241, 250, 140, 255}, {122, 135, 186, 255}, {80, 250, 123, 255}, {255, 184, 108, 255}, {200, 203, 222, 255}},

        {"gruvbox", "Gruvbox", "Warm retro", false,
         {29, 32, 33, 255}, {40, 40, 40, 255}, {50, 48, 47, 255}, {60, 56, 54, 255}, {80, 73, 69, 255},
         {251, 241, 199, 255}, {235, 219, 178, 255}, {189, 174, 147, 255}, {146, 131, 116, 255},
         {254, 128, 25, 255}, {142, 192, 124, 255}, {250, 189, 47, 255}, {251, 73, 52, 255}, {184, 187, 38, 255},
         {254, 128, 25, 255}, {211, 134, 155, 255}, {131, 165, 152, 255},
         {33, 35, 36, 255},
         {235, 219, 178, 255}, {250, 189, 47, 255}, {251, 73, 52, 255}, {211, 134, 155, 255},
         {184, 187, 38, 255}, {146, 131, 116, 255}, {142, 192, 124, 255}, {254, 128, 25, 255}, {189, 174, 147, 255}},

        {"everforest", "Everforest", "Soft forest greens", false,
         {35, 42, 46, 255}, {40, 48, 53, 255}, {45, 53, 59, 255}, {61, 72, 77, 255}, {79, 88, 94, 255},
         {236, 228, 207, 255}, {211, 198, 170, 255}, {180, 189, 178, 255}, {140, 150, 138, 255},
         {167, 192, 128, 255}, {127, 187, 179, 255}, {219, 188, 127, 255}, {230, 126, 128, 255}, {131, 192, 146, 255},
         {230, 152, 117, 255}, {214, 153, 182, 255}, {127, 187, 179, 255},
         {39, 46, 51, 255},
         {211, 198, 170, 255}, {219, 188, 127, 255}, {230, 126, 128, 255}, {214, 153, 182, 255},
         {131, 192, 146, 255}, {133, 146, 137, 255}, {167, 192, 128, 255}, {230, 152, 117, 255}, {157, 169, 160, 255}},

        {"paper", "Paper", "Clean and bright", true,
         {241, 244, 249, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {234, 238, 246, 255}, {205, 212, 226, 255},
         {15, 23, 42, 255}, {31, 41, 55, 255}, {71, 82, 104, 255}, {106, 116, 136, 255},
         {79, 70, 229, 255}, {13, 128, 118, 255}, {161, 98, 7, 255}, {200, 38, 38, 255}, {21, 128, 61, 255},
         {194, 84, 8, 255}, {126, 58, 200, 255}, {29, 100, 216, 255},
         {247, 249, 252, 255},
         {30, 41, 59, 255}, {3, 105, 161, 255}, {109, 40, 217, 255}, {180, 70, 9, 255},
         {21, 110, 61, 255}, {105, 115, 134, 255}, {146, 84, 6, 255}, {190, 24, 93, 255}, {71, 85, 105, 255}},

        {"latte", "Latte", "Light pastels (Catppuccin)", true,
         {230, 233, 239, 255}, {239, 241, 245, 255}, {243, 244, 248, 255}, {220, 224, 232, 255}, {188, 192, 204, 255},
         {35, 38, 58, 255}, {76, 79, 105, 255}, {92, 95, 119, 255}, {124, 127, 147, 255},
         {136, 57, 239, 255}, {14, 120, 127, 255}, {160, 98, 6, 255}, {200, 14, 54, 255}, {38, 112, 26, 255},
         {186, 70, 0, 255}, {176, 50, 138, 255}, {26, 92, 224, 255},
         {236, 238, 243, 255},
         {76, 79, 105, 255}, {138, 82, 6, 255}, {136, 57, 239, 255}, {168, 64, 0, 255},
         {38, 106, 26, 255}, {116, 119, 140, 255}, {26, 88, 216, 255}, {168, 40, 124, 255}, {92, 95, 119, 255}},
    };
    return themes;
}

int ThemeIndex(const std::string& savedId) {
    // ids of the themes these replaced
    static const char* renamed[][2] = {{"ocean", "nord"}, {"forest", "everforest"}, {"ember", "gruvbox"},
                                       {"arcade", "dracula"}, {"sand", "latte"}};
    std::string id = savedId;
    for (const auto& r : renamed) if (id == r[0]) id = r[1];
    const auto& ts = Themes();
    for (size_t i = 0; i < ts.size(); i++)
        if (id == ts[i].id) return (int)i;
    return 0;
}

int CurrentTheme() { return gCurrent; }

void ApplyTheme(int index) {
    const auto& ts = Themes();
    if (index < 0 || index >= (int)ts.size()) index = 0;
    gCurrent = index;
    const Theme& t = ts[index];
    col::bg = t.bg; col::bg2 = t.bg2; col::panel = t.panel; col::panelHi = t.panelHi; col::border = t.border;
    col::heading = t.heading; col::text = t.text; col::dim = t.dim; col::faint = t.faint;
    col::accent = t.accent; col::accent2 = t.accent2; col::warn = t.warn; col::danger = t.danger;
    col::success = t.success; col::pointer = t.pointer; col::heap = t.heap; col::stack = t.stack;
    col::codeBg = t.codeBg;
    col::codeLine = Alpha(t.accent, t.light ? 0.13f : 0.18f);
    col::shadow = t.light ? Color{70, 60, 40, 150} : Color{0, 0, 0, 255};
    col::synText = t.synText; col::synType = t.synType; col::synKey = t.synKey; col::synNum = t.synNum;
    col::synStr = t.synStr; col::synCom = t.synCom; col::synFn = t.synFn; col::synPre = t.synPre;
    col::synPunct = t.synPunct;
}

static float Luminance(Color c) {
    auto channel = [](unsigned char v) {
        float s = v / 255.0f;
        return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b);
}

float ContrastRatio(Color a, Color b) {
    float la = Luminance(a), lb = Luminance(b);
    if (la < lb) std::swap(la, lb);
    return (la + 0.05f) / (lb + 0.05f);
}

Color OnColor(Color c) {
    // White looks best on a coloured button, and 3:1 is enough for bold labels.
    const Color dark = {12, 15, 22, 255};
    return ContrastRatio(c, WHITE) >= 3.0f || ContrastRatio(c, WHITE) >= ContrastRatio(c, dark) ? WHITE : dark;
}

Color Tone(Color c) {
    return Themes()[gCurrent].light ? Mix(c, BLACK, 0.34f) : c;
}

} // namespace ui
