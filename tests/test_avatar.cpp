// ============================================================================
// test_avatar.cpp  -  Console unit tests for profile pictures: how a picture
// is cropped and rounded, and reading files whose path has non-ASCII letters.
// Build target: avatar_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "../src/app/avatar_image.h"
#include "../src/app/files.h"

namespace fs = std::filesystem;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static Color At(const Image& img, int x, int y) {
    const unsigned char* p = (const unsigned char*)img.data + (y * img.width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}

int main() {
    SetTraceLogLevel(LOG_NONE);

    printf("[square crop from the middle]\n");
    {
        // wide: red | green | blue thirds. Only the green middle survives.
        const Color red = {255, 0, 0, 255}, green = {0, 255, 0, 255}, blue = {0, 0, 255, 255};
        Image wide = GenImageColor(300, 100, red);
        ImageDrawRectangle(&wide, 100, 0, 100, 100, green);
        ImageDrawRectangle(&wide, 200, 0, 100, 100, blue);
        Image sq = avatar::SquareCrop(wide, 64);
        CHECK(sq.width == 64 && sq.height == 64);
        CHECK(At(sq, 2, 32).g > 200 && At(sq, 2, 32).r < 40);
        CHECK(At(sq, 61, 32).g > 200 && At(sq, 61, 32).b < 40);
        UnloadImage(sq);

        Image tall = GenImageColor(40, 120, GREEN);   // small and tall: made bigger
        sq = avatar::SquareCrop(tall);
        CHECK(sq.width == avatar::kSize && sq.height == avatar::kSize);
        UnloadImage(sq);
    }

    printf("[round mask]\n");
    {
        Image img = GenImageColor(64, 64, WHITE);
        avatar::RoundMask(img);
        CHECK(At(img, 0, 0).a == 0 && At(img, 63, 63).a == 0);          // corners gone
        CHECK(At(img, 32, 32).a == 255 && At(img, 32, 2).a == 255);     // inside kept
        int a = At(img, 32, 0).a;
        CHECK(a > 0 && a < 255);                                        // a soft edge
        UnloadImage(img);
    }

    printf("[files with non-ASCII paths]\n");
    {
        fs::path dir = fs::temp_directory_path() / fs::u8path(u8"segfault_año_test");
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir);
        fs::path file = dir / fs::u8path(u8"foto_niña.png");
        Image img = GenImageColor(10, 6, BLUE);
        int size = 0;
        unsigned char* png = ExportImageToMemory(img, ".png", &size);
        { std::ofstream out(file, std::ios::binary); out.write((const char*)png, size); }
        MemFree(png);
        UnloadImage(img);

        std::vector<unsigned char> data;
        std::string utf8 = file.u8string();
        CHECK(files::ReadAll(utf8, data, 1 << 20) && (int)data.size() == size);
        Image back = LoadImageFromMemory(".png", data.data(), (int)data.size());
        CHECK(back.width == 10 && back.height == 6);
        UnloadImage(back);
        CHECK(!files::ReadAll(utf8, data, 10));                         // over the size limit
        CHECK(!files::ReadAll((dir / "missing.png").u8string(), data, 1 << 20));
        fs::remove_all(dir, ec);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
