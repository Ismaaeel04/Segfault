// ============================================================================
// avatar_image.h  -  Turning any picture into a profile picture. Only raylib's
// image functions (no window needed), so avatar_tests can check them.
// ============================================================================
#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace avatar {

constexpr int kSize = 256;                        // saved size, in pixels
constexpr int kMaxFileBytes = 20 * 1024 * 1024;   // larger files are refused

// The centre square of `src`, resized to size x size, RGBA. `src` is unloaded.
inline Image SquareCrop(Image src, int size = kSize) {
    ImageFormat(&src, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int side = std::min(src.width, src.height);
    ImageCrop(&src, {(float)(src.width - side) / 2, (float)(src.height - side) / 2, (float)side, (float)side});
    ImageResize(&src, size, size);
    return src;
}

// Makes the corners see-through, leaving a circle with a smooth edge.
inline void RoundMask(Image& img) {
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    unsigned char* px = (unsigned char*)img.data;
    float c = (img.width - 1) / 2.0f, rad = img.width / 2.0f - 0.5f;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            float d = std::sqrt((x - c) * (x - c) + (y - c) * (y - c));
            float a = std::clamp(rad - d + 0.5f, 0.0f, 1.0f);   // about one pixel of soft edge
            unsigned char& alpha = px[(y * img.width + x) * 4 + 3];
            alpha = (unsigned char)std::lround(alpha * a);
        }
}

} // namespace avatar
