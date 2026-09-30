// image_lab: every ImageOp keeps a valid image and has the expected effect
#include "gfx/image_lab.hpp"
#include "testing.hpp"

#include <string>

using namespace rd;

namespace {
Image Sample() {
    Image img = GenImageGradientLinear(40, 24, 0, RED, BLUE);
    ImageDrawCircle(&img, 10, 10, 4, WHITE);
    ImageDrawPixel(&img, 0, 0, BLANK);
    return img;
}
}  // namespace

TEST(image_lab, every_op_leaves_valid_image) {
    for (int i = 0; i < kImageOpCount; ++i) {
        Image img = Sample();
        ApplyImageOp(img, static_cast<ImageOp>(i));
        CHECK(IsImageValid(img));
        CHECK(img.width > 0 && img.height > 0);
        CHECK(std::string(ImageOpName(static_cast<ImageOp>(i))) != "?");
        UnloadImage(img);
    }
}

TEST(image_lab, geometry_ops) {
    Image img = Sample();
    ApplyImageOp(img, ImageOp::RotateCW);
    CHECK_EQ(img.width, 24);
    CHECK_EQ(img.height, 40);
    ApplyImageOp(img, ImageOp::RotateCCW);
    CHECK_EQ(img.width, 40);
    ApplyImageOp(img, ImageOp::ResizeHalf);
    CHECK_EQ(img.width, 20);
    CHECK_EQ(img.height, 12);
    ApplyImageOp(img, ImageOp::ResizeNN2x);
    CHECK_EQ(img.width, 40);
    ApplyImageOp(img, ImageOp::ResizeCanvas);
    CHECK_EQ(img.width, 56);
    CHECK_EQ(img.height, 40);
    ApplyImageOp(img, ImageOp::ToPOT);
    CHECK_EQ(img.width, 64);
    CHECK_EQ(img.height, 64);
    ApplyImageOp(img, ImageOp::Crop);
    CHECK_EQ(img.width, 32);
    ApplyImageOp(img, ImageOp::Rotate30);
    CHECK(img.width > 32);  // canvas grows to fit
    UnloadImage(img);
}

TEST(image_lab, flip_moves_pixels) {
    Image img = GenImageColor(4, 4, BLACK);
    ImageDrawPixel(&img, 0, 0, RED);
    ApplyImageOp(img, ImageOp::FlipHorizontal);
    CHECK(ColorIsEqual(GetImageColor(img, 3, 0), RED));
    ApplyImageOp(img, ImageOp::FlipVertical);
    CHECK(ColorIsEqual(GetImageColor(img, 3, 3), RED));
    UnloadImage(img);
}

TEST(image_lab, color_ops) {
    Image img = GenImageColor(4, 4, {200, 40, 40, 255});
    ApplyImageOp(img, ImageOp::ColorInvert);
    const Color inv = GetImageColor(img, 0, 0);
    CHECK_EQ(static_cast<int>(inv.r), 55);
    CHECK_EQ(static_cast<int>(inv.g), 215);
    ApplyImageOp(img, ImageOp::Grayscale);
    const Color g = GetImageColor(img, 0, 0);
    CHECK_EQ(g.r, g.g);
    CHECK_EQ(g.g, g.b);
    ApplyImageOp(img, ImageOp::ColorReplace);  // replaces color at (0,0) -> MAGENTA
    CHECK(ColorIsEqual(GetImageColor(img, 2, 2), MAGENTA));
    UnloadImage(img);
}

TEST(image_lab, alpha_ops) {
    Image img = GenImageColor(16, 16, BLANK);
    ImageDrawRectangle(&img, 4, 4, 8, 8, WHITE);
    ApplyImageOp(img, ImageOp::AlphaCrop);
    CHECK_EQ(img.width, 8);
    CHECK_EQ(img.height, 8);
    UnloadImage(img);

    Image empty = GenImageColor(8, 8, BLANK);
    ApplyImageOp(empty, ImageOp::AlphaCrop);  // fully transparent -> fallback
    CHECK(IsImageValid(empty));
    UnloadImage(empty);

    Image masked = GenImageColor(32, 32, WHITE);
    ApplyImageOp(masked, ImageOp::AlphaMask);
    CHECK(GetImageColor(masked, 16, 16).a > GetImageColor(masked, 0, 0).a);
    UnloadImage(masked);
}

TEST(image_lab, format_ops) {
    Image img = Sample();
    ApplyImageOp(img, ImageOp::FromChannelRed);
    CHECK_EQ(img.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_GRAYSCALE));
    ApplyImageOp(img, ImageOp::FormatGrayAlpha);
    CHECK_EQ(img.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA));
    CHECK_STREQ(PixelFormatName(img.format), "GRAY_ALPHA");
    ApplyImageOp(img, ImageOp::Mipmaps);  // converts back to RGBA first
    CHECK_EQ(img.format, static_cast<int>(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8));
    CHECK(img.mipmaps > 1);
    UnloadImage(img);
}

TEST(image_lab, invalid_image_is_ignored) {
    Image none{};
    ApplyImageOp(none, ImageOp::RotateCW);
    CHECK(!IsImageValid(none));
}
