// raylib_color: color math + pixel format helpers
#include "testing.hpp"

#include "raylib.h"

TEST(raylib_color, int_hex_roundtrip) {
    CHECK_EQ(static_cast<unsigned int>(ColorToInt(RED)), 0xE62937FFu);
    const Color c = GetColor(0x11223344);
    CHECK_EQ(static_cast<int>(c.r), 0x11);
    CHECK_EQ(static_cast<int>(c.a), 0x44);
    CHECK(ColorIsEqual(GetColor(static_cast<unsigned int>(ColorToInt(SKYBLUE))), SKYBLUE));
}

TEST(raylib_color, normalize_hsv) {
    const Vector4 n = ColorNormalize({255, 0, 127, 255});
    CHECK_NEAR(n.x, 1.0, 1e-6);
    CHECK_NEAR(n.z, 127.0 / 255.0, 1e-4);
    CHECK(ColorIsEqual(ColorFromNormalized(n), {255, 0, 127, 255}));
    const Vector3 hsv = ColorToHSV({255, 0, 0, 255});
    CHECK_NEAR(hsv.x, 0.0, 1e-3);
    CHECK_NEAR(hsv.y, 1.0, 1e-4);
    const Color green = ColorFromHSV(120, 1, 1);
    CHECK_EQ(static_cast<int>(green.g), 255);
    CHECK_EQ(static_cast<int>(green.r), 0);
}

TEST(raylib_color, tint_brightness_contrast_alpha) {
    CHECK(ColorIsEqual(ColorTint(WHITE, RED), RED));
    CHECK(ColorBrightness(GRAY, 0.5f).r > GRAY.r);
    CHECK(ColorBrightness(GRAY, -0.5f).r < GRAY.r);
    CHECK(ColorContrast({200, 200, 200, 255}, 0.5f).r > 200);
    CHECK_EQ(static_cast<int>(ColorAlpha(RED, 0.5f).a), 127);
    CHECK_EQ(static_cast<int>(Fade(RED, 0.0f).a), 0);
    const Color blended = ColorAlphaBlend(BLACK, {255, 255, 255, 128}, WHITE);
    CHECK(blended.r > 100 && blended.r < 160);
    const Color mid = ColorLerp(BLACK, WHITE, 0.5f);
    CHECK(mid.r >= 127 && mid.r <= 128);
}

TEST(raylib_color, pixel_helpers) {
    CHECK_EQ(GetPixelDataSize(4, 4, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8), 64);
    CHECK_EQ(GetPixelDataSize(4, 4, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE), 16);
    CHECK_EQ(GetPixelDataSize(4, 4, PIXELFORMAT_UNCOMPRESSED_R5G6B5), 32);
    unsigned char px[4] = {0, 0, 0, 0};
    SetPixelColor(px, ORANGE, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    CHECK(ColorIsEqual(GetPixelColor(px, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8), ORANGE));
    unsigned short p565 = 0;
    SetPixelColor(&p565, {255, 0, 0, 255}, PIXELFORMAT_UNCOMPRESSED_R5G6B5);
    CHECK_EQ(static_cast<int>(GetPixelColor(&p565, PIXELFORMAT_UNCOMPRESSED_R5G6B5).r), 255);
}

TEST(raylib_color, image_color_access) {
    Image img = GenImageGradientLinear(8, 8, 90, BLACK, WHITE);  // horizontal
    const Color left = GetImageColor(img, 0, 4), right = GetImageColor(img, 7, 4);
    CHECK(left.r != right.r);
    Image sq = GenImageGradientSquare(8, 8, 0.5f, WHITE, BLACK);
    CHECK(IsImageValid(sq));
    Image noise = GenImagePerlinNoise(16, 16, 0, 0, 2.f);
    CHECK(IsImageValid(noise));
    UnloadImage(noise);
    UnloadImage(sq);
    UnloadImage(img);
}
