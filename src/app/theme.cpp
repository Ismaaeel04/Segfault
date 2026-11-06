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
    static const std::vector<Theme> themes = {
        {"midnight", "Midnight", "The original violet", false,
         {11, 13, 19, 255}, {16, 19, 27, 255}, {22, 26, 37, 255}, {31, 37, 52, 255}, {45, 53, 74, 255},
         {255, 255, 255, 255}, {236, 239, 246, 255}, {163, 171, 192, 255}, {106, 115, 140, 255},
         {139, 108, 255, 255}, {45, 212, 191, 255}, {251, 191, 36, 255}, {248, 113, 113, 255}, {74, 222, 128, 255},
         {251, 146, 60, 255}, {192, 132, 252, 255}, {96, 165, 250, 255},
         {14, 17, 25, 255},
         {214, 220, 236, 255}, {103, 232, 249, 255}, {196, 167, 255, 255}, {253, 186, 116, 255},
         {134, 239, 172, 255}, {115, 125, 152, 255}, {250, 224, 120, 255}, {244, 143, 177, 255}, {150, 160, 186, 255}},

        {"ocean", "Ocean", "Deep blue and cyan", false,
         {7, 17, 31, 255}, {10, 24, 42, 255}, {14, 32, 55, 255}, {21, 45, 74, 255}, {40, 72, 108, 255},
         {255, 255, 255, 255}, {226, 238, 250, 255}, {156, 180, 208, 255}, {108, 136, 168, 255},
         {56, 189, 248, 255}, {45, 212, 191, 255}, {252, 211, 77, 255}, {251, 113, 133, 255}, {74, 222, 128, 255},
         {251, 146, 60, 255}, {167, 139, 250, 255}, {125, 211, 252, 255},
         {8, 20, 36, 255},
         {214, 230, 245, 255}, {94, 234, 212, 255}, {125, 211, 252, 255}, {253, 186, 116, 255},
         {134, 239, 172, 255}, {112, 140, 170, 255}, {253, 224, 120, 255}, {249, 168, 212, 255}, {150, 175, 202, 255}},

        {"forest", "Forest", "Calm greens", false,
         {9, 17, 13, 255}, {13, 24, 18, 255}, {19, 33, 25, 255}, {28, 46, 35, 255}, {48, 74, 58, 255},
         {255, 255, 255, 255}, {232, 243, 235, 255}, {164, 190, 174, 255}, {114, 140, 124, 255},
         {74, 222, 128, 255}, {190, 220, 80, 255}, {250, 204, 21, 255}, {248, 113, 113, 255}, {110, 231, 183, 255},
         {251, 146, 60, 255}, {196, 181, 253, 255}, {103, 212, 240, 255},
         {10, 19, 14, 255},
         {220, 234, 224, 255}, {110, 231, 183, 255}, {190, 220, 80, 255}, {253, 186, 116, 255},
         {167, 243, 208, 255}, {116, 142, 126, 255}, {253, 224, 120, 255}, {249, 168, 212, 255}, {158, 182, 168, 255}},

        {"ember", "Ember", "Warm reds and orange", false,
         {19, 11, 11, 255}, {27, 16, 15, 255}, {37, 22, 21, 255}, {53, 32, 30, 255}, {84, 54, 50, 255},
         {255, 255, 255, 255}, {248, 238, 232, 255}, {198, 174, 164, 255}, {150, 124, 114, 255},
         {251, 113, 71, 255}, {251, 191, 36, 255}, {253, 224, 71, 255}, {252, 110, 130, 255}, {134, 239, 172, 255},
         {253, 160, 80, 255}, {216, 160, 250, 255}, {125, 190, 250, 255},
         {22, 13, 12, 255},
         {240, 226, 218, 255}, {253, 186, 116, 255}, {252, 140, 110, 255}, {253, 224, 120, 255},
         {190, 230, 140, 255}, {150, 124, 114, 255}, {251, 191, 36, 255}, {249, 168, 212, 255}, {196, 172, 162, 255}},

        {"arcade", "Arcade", "Neon pink on purple", false,
         {14, 9, 27, 255}, {20, 13, 37, 255}, {29, 19, 51, 255}, {42, 29, 71, 255}, {69, 50, 110, 255},
         {255, 255, 255, 255}, {240, 234, 252, 255}, {182, 170, 214, 255}, {132, 120, 168, 255},
         {236, 72, 153, 255}, {34, 211, 238, 255}, {253, 224, 71, 255}, {251, 113, 133, 255}, {74, 222, 128, 255},
         {251, 146, 60, 255}, {192, 132, 252, 255}, {96, 165, 250, 255},
         {16, 10, 31, 255},
         {228, 222, 245, 255}, {103, 232, 249, 255}, {244, 114, 182, 255}, {253, 186, 116, 255},
         {134, 239, 172, 255}, {134, 122, 170, 255}, {253, 224, 120, 255}, {216, 180, 254, 255}, {172, 160, 204, 255}},

        {"paper", "Paper", "Clean and bright", true,
         {241, 244, 249, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {234, 238, 246, 255}, {205, 212, 226, 255},
         {15, 23, 42, 255}, {31, 41, 55, 255}, {71, 82, 104, 255}, {106, 116, 136, 255},
         {79, 70, 229, 255}, {13, 128, 118, 255}, {161, 98, 7, 255}, {200, 38, 38, 255}, {21, 128, 61, 255},
         {194, 84, 8, 255}, {126, 58, 200, 255}, {29, 100, 216, 255},
         {247, 249, 252, 255},
         {30, 41, 59, 255}, {3, 105, 161, 255}, {109, 40, 217, 255}, {180, 70, 9, 255},
         {21, 110, 61, 255}, {105, 115, 134, 255}, {146, 84, 6, 255}, {190, 24, 93, 255}, {71, 85, 105, 255}},

        {"sand", "Sand", "Soft and warm", true,
         {243, 236, 223, 255}, {251, 246, 236, 255}, {253, 249, 241, 255}, {238, 229, 213, 255}, {216, 203, 181, 255},
         {40, 29, 18, 255}, {57, 45, 31, 255}, {96, 80, 60, 255}, {126, 108, 85, 255},
         {176, 79, 8, 255}, {15, 112, 104, 255}, {146, 90, 6, 255}, {185, 38, 38, 255}, {28, 112, 58, 255},
         {176, 79, 8, 255}, {126, 58, 170, 255}, {30, 92, 180, 255},
         {250, 245, 234, 255},
         {52, 40, 26, 255}, {14, 100, 110, 255}, {150, 50, 80, 255}, {170, 70, 8, 255},
         {42, 105, 50, 255}, {122, 105, 82, 255}, {130, 80, 6, 255}, {160, 40, 110, 255}, {96, 80, 60, 255}},
    };
    return themes;
}

int ThemeIndex(const std::string& id) {
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
