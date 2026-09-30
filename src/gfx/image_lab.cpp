#include "gfx/image_lab.hpp"

#include <algorithm>

namespace rd {

const char* ImageOpName(ImageOp op) {
    switch (op) {
        case ImageOp::FlipVertical: return "ImageFlipVertical";
        case ImageOp::FlipHorizontal: return "ImageFlipHorizontal";
        case ImageOp::RotateCW: return "ImageRotateCW";
        case ImageOp::RotateCCW: return "ImageRotateCCW";
        case ImageOp::Rotate30: return "ImageRotate(30)";
        case ImageOp::ColorInvert: return "ImageColorInvert";
        case ImageOp::Grayscale: return "ImageColorGrayscale";
        case ImageOp::Contrast: return "ImageColorContrast(+40)";
        case ImageOp::Brightness: return "ImageColorBrightness(+40)";
        case ImageOp::Tint: return "ImageColorTint(SKYBLUE)";
        case ImageOp::ColorReplace: return "ImageColorReplace";
        case ImageOp::BlurGaussian: return "ImageBlurGaussian(3)";
        case ImageOp::KernelEmboss: return "ImageKernelConvolution(emboss)";
        case ImageOp::Dither: return "ImageDither(4,4,4,4)";
        case ImageOp::ResizeHalf: return "ImageResize(1/2)";
        case ImageOp::ResizeNN2x: return "ImageResizeNN(x2)";
        case ImageOp::ResizeCanvas: return "ImageResizeCanvas(+16)";
        case ImageOp::Crop: return "ImageCrop(center)";
        case ImageOp::AlphaClear: return "ImageAlphaClear";
        case ImageOp::AlphaCrop: return "ImageAlphaCrop";
        case ImageOp::AlphaMask: return "ImageAlphaMask(radial)";
        case ImageOp::AlphaPremultiply: return "ImageAlphaPremultiply";
        case ImageOp::ToPOT: return "ImageToPOT";
        case ImageOp::FromChannelRed: return "ImageFromChannel(R)";
        case ImageOp::FromImageCenter: return "ImageFromImage(center)";
        case ImageOp::Copy: return "ImageCopy";
        case ImageOp::Mipmaps: return "ImageMipmaps";
        case ImageOp::FormatGrayAlpha: return "ImageFormat(GRAY_ALPHA)";
        default: return "?";
    }
}

const char* PixelFormatName(int format) {
    switch (format) {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: return "GRAYSCALE";
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: return "GRAY_ALPHA";
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5: return "R5G6B5";
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: return "R8G8B8";
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1: return "R5G5B5A1";
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: return "R4G4B4A4";
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return "R8G8B8A8";
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: return "R32G32B32A32";
        default: return "OTHER";
    }
}

static void Replace(Image& img, Image next) {
    UnloadImage(img);
    img = next;
}

void ApplyImageOp(Image& img, ImageOp op) {
    if (!IsImageValid(img)) return;
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 && op != ImageOp::FormatGrayAlpha)
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const float w = static_cast<float>(img.width), h = static_cast<float>(img.height);
    switch (op) {
        case ImageOp::FlipVertical: ImageFlipVertical(&img); break;
        case ImageOp::FlipHorizontal: ImageFlipHorizontal(&img); break;
        case ImageOp::RotateCW: ImageRotateCW(&img); break;
        case ImageOp::RotateCCW: ImageRotateCCW(&img); break;
        case ImageOp::Rotate30: ImageRotate(&img, 30); break;
        case ImageOp::ColorInvert: ImageColorInvert(&img); break;
        case ImageOp::Grayscale: ImageColorGrayscale(&img); break;
        case ImageOp::Contrast: ImageColorContrast(&img, 40.f); break;
        case ImageOp::Brightness: ImageColorBrightness(&img, 40); break;
        case ImageOp::Tint: ImageColorTint(&img, SKYBLUE); break;
        case ImageOp::ColorReplace: ImageColorReplace(&img, GetImageColor(img, 0, 0), MAGENTA); break;
        case ImageOp::BlurGaussian: ImageBlurGaussian(&img, 3); break;
        case ImageOp::KernelEmboss: {
            const float k[9] = {-2, -1, 0, -1, 1, 1, 0, 1, 2};
            ImageKernelConvolution(&img, k, 9);
            break;
        }
        case ImageOp::Dither: ImageDither(&img, 4, 4, 4, 4); break;
        case ImageOp::ResizeHalf: ImageResize(&img, std::max(1, img.width / 2), std::max(1, img.height / 2)); break;
        case ImageOp::ResizeNN2x: ImageResizeNN(&img, std::min(1024, img.width * 2), std::min(1024, img.height * 2)); break;
        case ImageOp::ResizeCanvas: ImageResizeCanvas(&img, img.width + 16, img.height + 16, 8, 8, DARKPURPLE); break;
        case ImageOp::Crop: ImageCrop(&img, {w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f}); break;
        case ImageOp::AlphaClear: ImageAlphaClear(&img, BLANK, 0.5f); break;
        case ImageOp::AlphaCrop: ImageAlphaCrop(&img, 0.1f); break;
        case ImageOp::AlphaMask: {
            Image mask = GenImageGradientRadial(img.width, img.height, 0.2f, WHITE, BLACK);
            ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
            ImageAlphaMask(&img, mask);
            UnloadImage(mask);
            break;
        }
        case ImageOp::AlphaPremultiply: ImageAlphaPremultiply(&img); break;
        case ImageOp::ToPOT: ImageToPOT(&img, DARKGRAY); break;
        case ImageOp::FromChannelRed: Replace(img, ImageFromChannel(img, 0)); break;
        case ImageOp::FromImageCenter: Replace(img, ImageFromImage(img, {w * 0.2f, h * 0.2f, w * 0.6f, h * 0.6f})); break;
        case ImageOp::Copy: Replace(img, ImageCopy(img)); break;
        case ImageOp::Mipmaps: ImageMipmaps(&img); break;
        case ImageOp::FormatGrayAlpha: ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA); break;
        default: break;
    }
    if (op == ImageOp::AlphaCrop && (img.width <= 0 || img.height <= 0)) {
        Replace(img, GenImageColor(8, 8, MAGENTA));  // fully transparent input -> keep something visible
    }
}

}  // namespace rd
