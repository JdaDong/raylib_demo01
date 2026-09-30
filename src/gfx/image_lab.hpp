// image_lab.hpp - catalogue of raylib CPU image operations (used by the
// Showcase "Image Lab" page and exhaustively unit-tested).
#pragma once
#include "raylib.h"

namespace rd {

enum class ImageOp {
    FlipVertical, FlipHorizontal, RotateCW, RotateCCW, Rotate30,
    ColorInvert, Grayscale, Contrast, Brightness, Tint, ColorReplace,
    BlurGaussian, KernelEmboss, Dither, ResizeHalf, ResizeNN2x, ResizeCanvas,
    Crop, AlphaClear, AlphaCrop, AlphaMask, AlphaPremultiply, ToPOT,
    FromChannelRed, FromImageCenter, Copy, Mipmaps, FormatGrayAlpha,
    Count
};
constexpr int kImageOpCount = static_cast<int>(ImageOp::Count);

const char* ImageOpName(ImageOp op);
// Applies op in place. Always leaves a valid image (RGBA8 unless op is a format op).
void ApplyImageOp(Image& img, ImageOp op);
const char* PixelFormatName(int format);

}  // namespace rd
