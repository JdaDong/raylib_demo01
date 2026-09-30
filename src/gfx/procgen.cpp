#include "gfx/procgen.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace rd {

std::vector<Vector2> RegularPolygon(Vector2 center, int sides, float radius, float rotationDeg) {
    std::vector<Vector2> pts;
    sides = std::max(3, sides);
    for (int i = 0; i < sides; ++i) {
        const float a = (rotationDeg + 360.f * static_cast<float>(i) / static_cast<float>(sides)) * DEG2RAD;
        pts.push_back({center.x + std::cos(a) * radius, center.y - std::sin(a) * radius});
    }
    return pts;
}

Image GenShipImage(int size, Color body, Color accent) {
    Image img = GenImageColor(size, size, BLANK);
    const float s = static_cast<float>(size);
    // ship points to +X so that rotation == aim angle
    const Vector2 nose{s * 0.95f, s * 0.5f}, tl{s * 0.1f, s * 0.12f}, bl{s * 0.1f, s * 0.88f}, notch{s * 0.3f, s * 0.5f};
    ImageDrawTriangleEx(&img, nose, tl, notch, body, ColorBrightness(body, -0.35f), accent);
    ImageDrawTriangleEx(&img, nose, notch, bl, body, accent, ColorBrightness(body, -0.35f));
    ImageDrawTriangleLines(&img, nose, tl, notch, ColorBrightness(body, 0.4f));
    ImageDrawTriangleLines(&img, nose, notch, bl, ColorBrightness(body, 0.4f));
    ImageDrawCircleV(&img, {s * 0.55f, s * 0.5f}, std::max(2, size / 10), accent);
    ImageDrawCircleLinesV(&img, {s * 0.55f, s * 0.5f}, std::max(2, size / 10) + 1, WHITE);
    ImageDrawLineEx(&img, {s * 0.12f, s * 0.3f}, {s * 0.12f, s * 0.7f}, std::max(1, size / 16), accent);  // engine
    ImageDrawPixel(&img, static_cast<int>(nose.x) - 2, static_cast<int>(nose.y), WHITE);
    return img;
}

Image GenEnemyImage(EnemyKind kind, int size) {
    Image img = GenImageColor(size, size, BLANK);
    const float s = static_cast<float>(size);
    const Vector2 c{s * 0.5f, s * 0.5f};
    switch (kind) {
        case EnemyKind::Chaser: {
            auto fan = RegularPolygon(c, 3, s * 0.46f, 0.f);
            fan.insert(fan.begin(), c);
            fan.push_back(fan[1]);
            ImageDrawTriangleFan(&img, fan.data(), static_cast<int>(fan.size()), GetColor(0xFF4D6DFF));
            ImageDrawCircleLines(&img, static_cast<int>(c.x), static_cast<int>(c.y), size / 6, WHITE);
            break;
        }
        case EnemyKind::Weaver: {
            auto fan = RegularPolygon(c, 4, s * 0.46f, 45.f);
            fan.insert(fan.begin(), c);
            fan.push_back(fan[1]);
            ImageDrawTriangleFan(&img, fan.data(), static_cast<int>(fan.size()), GetColor(0x2EE6D6FF));
            ImageDrawLineV(&img, {s * 0.2f, s * 0.5f}, {s * 0.8f, s * 0.5f}, WHITE);
            ImageDrawLine(&img, size / 2, size / 5, size / 2, size * 4 / 5, WHITE);
            break;
        }
        case EnemyKind::Splitter: {
            auto fan = RegularPolygon(c, 6, s * 0.46f, 0.f);
            fan.insert(fan.begin(), c);
            fan.push_back(fan[1]);
            ImageDrawTriangleFan(&img, fan.data(), static_cast<int>(fan.size()), GetColor(0xFFB020FF));
            ImageDrawCircle(&img, size / 2, size / 2, size / 6, GetColor(0x7A3E00FF));
            ImageDrawCircleV(&img, c, size / 12, WHITE);
            break;
        }
        case EnemyKind::Tank: {
            ImageDrawRectangle(&img, size / 8, size / 8, size * 3 / 4, size * 3 / 4, GetColor(0x8A5CFFFF));
            ImageDrawRectangleLines(&img, {s / 8, s / 8, s * 3 / 4, s * 3 / 4}, std::max(1, size / 20), WHITE);
            ImageDrawRectangleV(&img, {s * 0.5f, s * 0.42f}, {s * 0.45f, s * 0.16f}, GetColor(0xD8C8FFFF));  // barrel
            ImageDrawRectangleRec(&img, {s * 0.35f, s * 0.35f, s * 0.3f, s * 0.3f}, GetColor(0x4B2FA0FF));
            break;
        }
        default: break;
    }
    return img;
}

Image GenGemImage(int size, Color c) {
    Image img = GenImageColor(size, size, BLANK);
    const float s = static_cast<float>(size);
    // diamond as a triangle strip: top, left, right, bottom
    const Vector2 strip[4] = {{s * 0.5f, s * 0.05f}, {s * 0.1f, s * 0.5f}, {s * 0.9f, s * 0.5f}, {s * 0.5f, s * 0.95f}};
    ImageDrawTriangleStrip(&img, strip, 4, c);
    // facet highlight on the upper-left face
    ImageDrawTriangle(&img, strip[0], strip[1], {s * 0.5f, s * 0.5f}, ColorBrightness(c, 0.35f));
    ImageDrawLineV(&img, strip[0], strip[3], ColorBrightness(c, 0.5f));
    ImageDrawTriangleLines(&img, strip[0], strip[1], strip[2], WHITE);
    return img;
}

Image GenHealImage(int size) {
    Image img = GenImageColor(size, size, BLANK);
    ImageDrawCircle(&img, size / 2, size / 2, size / 2 - 1, GetColor(0x1FAA59FF));
    const int t = std::max(2, size / 5);
    ImageDrawRectangle(&img, size / 2 - t / 2, size / 5, t, size * 3 / 5, WHITE);
    ImageDrawRectangle(&img, size / 5, size / 2 - t / 2, size * 3 / 5, t, WHITE);
    return img;
}

Image GenGlowImage(int size, Color inner) {
    return GenImageGradientRadial(size, size, 0.0f, inner, BLANK);
}

Image GenParticleImage(int size) {
    Image img = GenImageGradientRadial(size, size, 0.3f, WHITE, BLANK);
    ImageAlphaPremultiply(&img);  // works nicely with additive blending
    return img;
}

Image GenPanelImage(int size, int border, Color fill, Color edge) {
    Image img = GenImageColor(size, size, fill);
    ImageDrawRectangleLines(&img, {0, 0, static_cast<float>(size), static_cast<float>(size)}, 2, edge);
    ImageDrawRectangleLines(&img, {3, 3, static_cast<float>(size - 6), static_cast<float>(size - 6)}, 1, Fade(edge, 0.4f));
    // corner studs (they stay unscaled in 9-patch mode)
    const int b = border;
    ImageDrawPixel(&img, b / 2, b / 2, WHITE);
    ImageDrawPixelV(&img, {static_cast<float>(size - b / 2 - 1), static_cast<float>(b / 2)}, WHITE);
    ImageDrawPixel(&img, b / 2, size - b / 2 - 1, WHITE);
    ImageDrawPixel(&img, size - b / 2 - 1, size - b / 2 - 1, WHITE);
    return img;
}

Image GenBackgroundTile(int size, uint32_t seed) {
    Image perlin = GenImagePerlinNoise(size, size, static_cast<int>(seed % 1000), static_cast<int>((seed / 1000) % 1000), 4.0f);
    ImageColorTint(&perlin, GetColor(0x3040A0FF));
    ImageColorContrast(&perlin, 20.0f);
    ImageColorBrightness(&perlin, -60);

    // GenImageCellular draws its seed points with GetRandomValue() -> seed the
    // global raylib RNG so the tile is reproducible per seed
    SetRandomSeed(seed);
    Image cells = GenImageCellular(size, size, size / 4);
    ImageColorInvert(&cells);
    ImageColorTint(&cells, GetColor(0x6020A0FF));
    ImageDraw(&perlin, cells, {0, 0, static_cast<float>(size), static_cast<float>(size)},
              {0, 0, static_cast<float>(size), static_cast<float>(size)}, Fade(WHITE, 0.25f));
    UnloadImage(cells);
    ImageDither(&perlin, 5, 6, 5, 0);  // retro banding; raylib only dithers to <= 16 bpp total
    ImageFormat(&perlin, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return perlin;
}

Image GenFloorImage(int w, int h) {
    Image img = GenImageChecked(w, h, w / 8, h / 8, GetColor(0x141828FF), GetColor(0x1A2034FF));
    ImageBlurGaussian(&img, 1);
    const float sharpen[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
    ImageKernelConvolution(&img, sharpen, 9);
    ImageMipmaps(&img);
    return img;
}

Image GenIconImage(int size) {
    Image img = GenImageGradientSquare(size, size, 0.4f, GetColor(0x2EE6D6FF), GetColor(0x101830FF));
    ImageDrawCircleV(&img, {size * 0.5f, size * 0.5f}, size / 4, GetColor(0xFF4D6DFF));
    ImageDrawCircleLinesV(&img, {size * 0.5f, size * 0.5f}, size / 3, WHITE);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);  // SetWindowIcon requires RGBA8
    return img;
}

Image GenNoiseImage(int w, int h, float factor) {
    Image img = GenImageWhiteNoise(w, h, factor);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return img;
}

Image GenExplosionSheet(int frameSize, int frames) {
    Image sheet = GenImageColor(frameSize, frameSize, BLANK);
    ImageResizeCanvas(&sheet, frameSize * frames, frameSize, 0, 0, BLANK);
    for (int f = 0; f < frames; ++f) {
        const float t = static_cast<float>(f) / static_cast<float>(std::max(1, frames - 1));
        Image frame = GenImageGradientRadial(frameSize, frameSize, 0.2f + t * 0.6f,
                                             ColorLerp(WHITE, GetColor(0xFF6A00FF), t), BLANK);
        ImageDrawCircleLines(&frame, frameSize / 2, frameSize / 2, static_cast<int>(frameSize * 0.2f + t * frameSize * 0.28f),
                             Fade(GetColor(0xFFD080FF), 1.f - t));
        ImageDraw(&sheet, frame, {0, 0, static_cast<float>(frameSize), static_cast<float>(frameSize)},
                  {static_cast<float>(f * frameSize), 0, static_cast<float>(frameSize), static_cast<float>(frameSize)}, WHITE);
        UnloadImage(frame);
    }
    return sheet;
}

float AlphaBorderRadius(const Image& img, float threshold) {
    const Rectangle r = GetImageAlphaBorder(img, threshold);
    return 0.5f * std::min(r.width, r.height);
}

Color AverageColor(const Image& img) {
    Color* px = LoadImageColors(img);
    if (!px) return BLANK;
    unsigned long long acc[4] = {0, 0, 0, 0};
    const int n = img.width * img.height;
    for (int i = 0; i < n; ++i) {
        acc[0] += px[i].r; acc[1] += px[i].g; acc[2] += px[i].b; acc[3] += px[i].a;
    }
    UnloadImageColors(px);
    if (n == 0) return BLANK;
    return {static_cast<unsigned char>(acc[0] / n), static_cast<unsigned char>(acc[1] / n),
            static_cast<unsigned char>(acc[2] / n), static_cast<unsigned char>(acc[3] / n)};
}

std::vector<Color> DominantPalette(const Image& img, int max) {
    int count = 0;
    Color* pal = LoadImagePalette(img, max, &count);
    std::vector<Color> out;
    if (pal) {
        out.assign(pal, pal + count);
        UnloadImagePalette(pal);
    }
    return out;
}

int CountOpaquePixels(const Image& img, unsigned char minAlpha) {
    int n = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            if (GetImageColor(img, x, y).a >= minAlpha) ++n;
    return n;
}

}  // namespace rd
