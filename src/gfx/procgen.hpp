// procgen.hpp - every sprite is generated at runtime with raylib's CPU Image API
// (no asset files). All functions here are GPU-free and unit-tested.
#pragma once
#include "game/types.hpp"
#include "raylib.h"

#include <cstdint>
#include <vector>

namespace rd {

Image GenShipImage(int size, Color body, Color accent);
Image GenEnemyImage(EnemyKind kind, int size);
Image GenGemImage(int size, Color c);
Image GenHealImage(int size);
Image GenGlowImage(int size, Color inner);
Image GenParticleImage(int size);
// 9-patch panel: `border` px corners, used with DrawTextureNPatch
Image GenPanelImage(int size, int border, Color fill, Color edge);
Image GenBackgroundTile(int size, uint32_t seed);
Image GenFloorImage(int w, int h);
Image GenIconImage(int size);
Image GenNoiseImage(int w, int h, float factor);
// horizontal strip of `frames` explosion frames of frameSize x frameSize
Image GenExplosionSheet(int frameSize, int frames);

// helpers ------------------------------------------------------------------
// Regular polygon points (CCW in screen space) for ImageDrawTriangleFan
std::vector<Vector2> RegularPolygon(Vector2 center, int sides, float radius, float rotationDeg);
// Collision radius derived from the sprite's opaque area (GetImageAlphaBorder)
float AlphaBorderRadius(const Image& img, float threshold = 0.1f);
Color AverageColor(const Image& img);                        // LoadImageColors
std::vector<Color> DominantPalette(const Image& img, int max);  // LoadImagePalette
int CountOpaquePixels(const Image& img, unsigned char minAlpha = 1);

}  // namespace rd
