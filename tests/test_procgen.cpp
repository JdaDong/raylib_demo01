// procgen: every generator produces a valid, non-empty image headlessly
#include "gfx/procgen.hpp"
#include "testing.hpp"

#include <string>

using namespace rd;

namespace {
void CheckSprite(Image img, int w, int h, const char* what) {
    CHECK(IsImageValid(img));
    CHECK_EQ(img.width, w);
    CHECK_EQ(img.height, h);
    CHECK(CountOpaquePixels(img) > 0);
    if (!IsImageValid(img)) std::fprintf(stderr, "    invalid: %s\n", what);
    UnloadImage(img);
}
}  // namespace

TEST(procgen, sprites_are_valid) {
    CheckSprite(GenShipImage(48, SKYBLUE, GOLD), 48, 48, "ship");
    for (int k = 0; k < kEnemyKindCount; ++k) CheckSprite(GenEnemyImage(static_cast<EnemyKind>(k), 64), 64, 64, "enemy");
    CheckSprite(GenGemImage(20, LIME), 20, 20, "gem");
    CheckSprite(GenHealImage(24), 24, 24, "heal");
    CheckSprite(GenGlowImage(64, GOLD), 64, 64, "glow");
    CheckSprite(GenParticleImage(16), 16, 16, "particle");
    CheckSprite(GenPanelImage(48, 12, DARKBLUE, SKYBLUE), 48, 48, "panel");
    CheckSprite(GenIconImage(64), 64, 64, "icon");
    CheckSprite(GenNoiseImage(32, 32, 0.5f), 32, 32, "noise");
}

TEST(procgen, background_tile_is_rgba_and_opaque) {
    Image bg = GenBackgroundTile(128, 42);
    CHECK_EQ(bg.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8));
    CHECK_EQ(CountOpaquePixels(bg, 255), 128 * 128);
    UnloadImage(bg);
}

TEST(procgen, floor_has_mipmaps) {
    Image f = GenFloorImage(128, 128);
    CHECK(f.mipmaps > 1);
    UnloadImage(f);
}

TEST(procgen, icon_is_rgba8_for_window_icon) {
    Image i = GenIconImage(32);
    CHECK_EQ(i.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8));
    UnloadImage(i);
}

TEST(procgen, explosion_sheet_layout) {
    Image s = GenExplosionSheet(32, 8);
    CHECK_EQ(s.width, 256);
    CHECK_EQ(s.height, 32);
    // first frame is brighter at the center than the last one
    const Color first = GetImageColor(s, 16, 16);
    const Color last = GetImageColor(s, 7 * 32 + 16, 16);
    CHECK(first.a > 0);
    CHECK(first.g >= last.g);
    UnloadImage(s);
}

TEST(procgen, regular_polygon) {
    const auto p = RegularPolygon({0, 0}, 4, 10, 0);
    REQUIRE(p.size() == 4u);
    CHECK_NEAR(p[0].x, 10.0, 1e-4);
    CHECK_NEAR(p[1].y, -10.0, 1e-4);  // screen space: CCW goes up first
    CHECK_EQ(RegularPolygon({0, 0}, 1, 5, 0).size(), 3u);  // clamped to triangle
}

TEST(procgen, alpha_border_radius_matches_shape) {
    Image img = GenImageColor(64, 64, BLANK);
    ImageDrawCircle(&img, 32, 32, 10, WHITE);
    const float r = AlphaBorderRadius(img);
    CHECK_NEAR(r, 10.5, 1.0);
    UnloadImage(img);
}

TEST(procgen, average_color_and_palette) {
    Image img = GenImageChecked(16, 16, 8, 8, RED, BLUE);
    const Color avg = AverageColor(img);
    CHECK(avg.r > 100 && avg.b > 100);
    const auto pal = DominantPalette(img, 8);
    CHECK_EQ(pal.size(), 2u);
    UnloadImage(img);
}

TEST(procgen, sprites_are_deterministic) {
    Image a = GenBackgroundTile(64, 7), b = GenBackgroundTile(64, 7);
    Color* pa = LoadImageColors(a);
    Color* pb = LoadImageColors(b);
    bool same = true;
    for (int i = 0; i < 64 * 64; ++i) same = same && ColorIsEqual(pa[i], pb[i]);
    CHECK(same);
    UnloadImageColors(pa);
    UnloadImageColors(pb);
    UnloadImage(a);
    UnloadImage(b);
}

TEST(procgen, export_import_roundtrip) {
    Image icon = GenIconImage(32);
    const std::string png = std::string(RD_TEST_TMP) + "/icon_test.png";
    CHECK(ExportImage(icon, png.c_str()));
    Image back = LoadImage(png.c_str());
    CHECK(IsImageValid(back));
    CHECK_EQ(back.width, 32);
    CHECK(ColorIsEqual(GetImageColor(back, 16, 16), GetImageColor(icon, 16, 16)));
    int size = 0;
    unsigned char* mem = ExportImageToMemory(icon, ".png", &size);
    REQUIRE(mem != nullptr);
    Image fromMem = LoadImageFromMemory(".png", mem, size);
    CHECK_EQ(fromMem.height, 32);
    MemFree(mem);
    const std::string code = std::string(RD_TEST_TMP) + "/icon_test.h";
    CHECK(ExportImageAsCode(icon, code.c_str()));
    CHECK(FileExists(code.c_str()));
    // raw loader: dump RGBA bytes, read back with LoadImageRaw
    const std::string raw = std::string(RD_TEST_TMP) + "/icon_test.raw";
    SaveFileData(raw.c_str(), icon.data, GetPixelDataSize(32, 32, icon.format));
    Image rawImg = LoadImageRaw(raw.c_str(), 32, 32, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 0);
    CHECK(ColorIsEqual(GetImageColor(rawImg, 3, 3), GetImageColor(icon, 3, 3)));
    UnloadImage(rawImg);
    UnloadImage(fromMem);
    UnloadImage(back);
    UnloadImage(icon);
}

TEST(procgen, gen_image_text_uses_raw_bytes) {
    Image t = GenImageText(4, 4, "0123456789abcdef");
    CHECK(IsImageValid(t));
    CHECK_EQ(t.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_GRAYSCALE));
    CHECK_EQ(static_cast<int>(static_cast<unsigned char*>(t.data)[0]), static_cast<int>('0'));
    UnloadImage(t);
}

TEST(procgen, image_draw_helpers) {
    Image img = GenImageColor(32, 32, BLACK);
    ImageClearBackground(&img, DARKGRAY);
    CHECK(ColorIsEqual(GetImageColor(img, 0, 0), DARKGRAY));
    ImageDrawRectangle(&img, 0, 0, 8, 8, RED);
    CHECK(ColorIsEqual(GetImageColor(img, 4, 4), RED));
    ImageDrawLine(&img, 0, 20, 31, 20, GREEN);
    CHECK(ColorIsEqual(GetImageColor(img, 16, 20), GREEN));
    const Vector2 fan[4] = {{16, 16}, {28, 16}, {16, 4}, {4, 16}};
    ImageDrawTriangleFan(&img, fan, 4, BLUE);
    CHECK(ColorIsEqual(GetImageColor(img, 16, 12), BLUE));
    UnloadImage(img);
}
