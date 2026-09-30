// assets.hpp - GPU resources created from procedural images at startup
#pragma once
#include "game/storage.hpp"
#include "game/types.hpp"
#include "raylib.h"

#include <string>

namespace rd {

struct Assets {
    Texture2D ship{}, gem{}, heal{}, glow{}, particle{}, panel{}, bg{}, floorTex{}, noise{}, explosion{}, icon{}, white{};
    Texture2D enemy[kEnemyKindCount]{};
    NPatchInfo panelPatch{};
    Font ui{}, title{}, atlas{}, mono{};  // mono: LoadFont() ASCII @32px for the debug console
    bool uiIsTtf = false, titleIsTtf = false, atlasOwned = false, monoOwned = false;
    int sdfGlyphs = 0;                    // FONT_SDF probe (LoadFontData -> UnloadFontData)
    std::string fontPath;
    float enemyRadius[kEnemyKindCount]{};
    float shipRadius = 14.f;
    int explosionFrames = 8;
    Image noiseImage{};  // CPU copy, re-uploaded with UpdateTexture for animated grain
    std::string report;  // human readable load log

    bool load(Storage& storage);
    void unload();
    void animateNoise(unsigned int frame);        // UpdateTexture / UpdateTextureRec
    bool setPlayerSkin(Image img);                // from a dropped file
    Font fontFor(float size) const { return size >= 40 ? title : ui; }
};

// UI strings rendered with the TTF font (includes CJK). Codepoints are collected from this.
extern const char* kGlyphText;

}  // namespace rd
