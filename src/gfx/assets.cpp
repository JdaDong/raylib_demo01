#include "gfx/assets.hpp"

#include "gfx/procgen.hpp"

#include <set>
#include <vector>

namespace rd {

const char* kGlyphText =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    "霓虹收割者开始游戏设置展示退出最高分波次暂停继续重新返回标题结束输入名字保存你好世界中文渲染测试"
    "音量乐效全屏垂直同步后处理难度简单普通困难显示帧率屏幕震动准星击杀连击生命冲刺按键方向瞄准射击"
    "×·—…←→↑↓★☆●○■□▲△";

namespace {

const char* kFontCandidates[] = {
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",  // macOS, has CJK
    "/Library/Fonts/Arial Unicode.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",       // Linux, no CJK
    "C:/Windows/Fonts/arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
};

Texture2D Upload(Image img, int filter = TEXTURE_FILTER_BILINEAR) {
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, filter);
    return t;
}

std::vector<int> UniqueCodepoints(const char* text) {
    int count = 0;
    int* cps = LoadCodepoints(text, &count);
    std::set<int> uniq(cps, cps + count);
    UnloadCodepoints(cps);
    return {uniq.begin(), uniq.end()};
}

// Bitmap font assembled with the Image API and parsed back by LoadFontFromImage():
// glyph boxes (non-key color) separated by key-colored gaps, codepoints 32..126.
Font BuildBitmapFont(std::string& report) {
    const Color key = MAGENTA;
    const int fontSize = 20, gap = 2, boxH = 22, maxW = 480;
    Font def = GetFontDefault();
    struct Box { int cp; int w; };
    std::vector<Box> boxes;
    for (int cp = 32; cp <= 126; ++cp) {
        const char s[2] = {static_cast<char>(cp), 0};
        const int w = static_cast<int>(MeasureTextEx(def, s, fontSize, 0).x) + 2;
        boxes.push_back({cp, w < 6 ? 6 : w});
    }
    // layout rows
    int rows = 1, x = gap;
    for (const Box& b : boxes) {
        if (x + b.w + gap > maxW) { ++rows; x = gap; }
        x += b.w + gap;
    }
    const int imgH = gap + rows * (boxH + gap);
    Image img = GenImageColor(maxW, imgH, key);
    x = gap;
    int y = gap;
    for (const Box& b : boxes) {
        if (x + b.w + gap > maxW) { x = gap; y += boxH + gap; }
        ImageDrawRectangle(&img, x, y, b.w, boxH, BLANK);
        const char s[2] = {static_cast<char>(b.cp), 0};
        ImageDrawTextEx(&img, def, s, {static_cast<float>(x + 1), static_cast<float>(y + 1)}, fontSize, 0, GetColor(0x9CFFE6FF));
        x += b.w + gap;
    }
    Font f = LoadFontFromImage(img, key, 32);
    UnloadImage(img);
    report += TextFormat("bitmap font: %d glyphs (LoadFontFromImage)\n", f.glyphCount);
    return f;
}

}  // namespace

bool Assets::load(Storage& storage) {
    report.clear();
    // --- sprites -----------------------------------------------------------
    {
        Image s = GenShipImage(40, GetColor(0x5AD7FFFF), GetColor(0xFFE066FF));
        shipRadius = AlphaBorderRadius(s) * 0.75f;  // collision radius from sprite alpha
        ship = Upload(s);
    }
    for (int k = 0; k < kEnemyKindCount; ++k) {
        Image e = GenEnemyImage(static_cast<EnemyKind>(k), 64);
        enemyRadius[k] = AlphaBorderRadius(e);
        enemy[k] = Upload(e);
    }
    gem = Upload(GenGemImage(20, GetColor(0x7CFF6BFF)));
    heal = Upload(GenHealImage(24));
    glow = Upload(GenGlowImage(128, GetColor(0xFFFFFFFF)));
    particle = Upload(GenParticleImage(16));
    explosion = Upload(GenExplosionSheet(64, explosionFrames));
    white = Upload(GenImageColor(1, 1, WHITE), TEXTURE_FILTER_POINT);

    Image panelImg = GenPanelImage(48, 12, GetColor(0x101830E6), GetColor(0x5AD7FFFF));
    panel = Upload(panelImg, TEXTURE_FILTER_POINT);
    panelPatch = {{0, 0, 48, 48}, 12, 12, 12, 12, NPATCH_NINE_PATCH};

    // background tile: uploaded without mips, GPU builds the chain (GenTextureMipmaps)
    {
        Image bgImg = GenBackgroundTile(256, 1337);
        bg = LoadTextureFromImage(bgImg);
        UnloadImage(bgImg);
    }
    GenTextureMipmaps(&bg);
    SetTextureFilter(bg, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(bg, TEXTURE_WRAP_REPEAT);  // tiled via oversized source rect

    // floor: CPU-built mip chain (ImageMipmaps) uploaded as-is
    {
        Image fl = GenFloorImage(256, 256);
        const int cpuMips = fl.mipmaps;
        floorTex = Upload(fl, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(floorTex, TEXTURE_WRAP_MIRROR_REPEAT);
        report += TextFormat("floor: cpu mips=%d, bg gpu mips=%d\n", cpuMips, bg.mipmaps);
    }

    noiseImage = GenNoiseImage(128, 128, 0.5f);
    noise = LoadTextureFromImage(noiseImage);
    SetTextureWrap(noise, TEXTURE_WRAP_REPEAT);

    // --- icon: generate -> export PNG -> reload through the file loaders ------
    {
        Image iconImg = GenIconImage(64);
        const std::string png = storage.path("exports/icon.png");
        ExportImage(iconImg, png.c_str());
        ExportImageAsCode(iconImg, storage.path("exports/icon.h").c_str());
        Image fromDisk = LoadImage(png.c_str());
        if (IsImageValid(fromDisk)) {
            ImageFormat(&fromDisk, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            Image small = ImageCopy(fromDisk);
            ImageResize(&small, 32, 32);
            Image icons[2] = {fromDisk, small};
            SetWindowIcon(fromDisk);   // GLFW: no-op + warning on macOS
            SetWindowIcons(icons, 2);
            UnloadImage(small);
            UnloadImage(fromDisk);
        }
        icon = LoadTexture(png.c_str());
        // GPU -> CPU readback sanity check
        Image readback = LoadImageFromTexture(icon);
        report += TextFormat("icon: %dx%d, readback %dx%d valid=%d\n", icon.width, icon.height, readback.width,
                             readback.height, IsTextureValid(icon));
        UnloadImage(readback);
        UnloadImage(iconImg);
    }

    // --- fonts ----------------------------------------------------------------
    const std::vector<int> cps = UniqueCodepoints(kGlyphText);
    for (const char* path : kFontCandidates) {
        if (!FileExists(path)) continue;
        Font f = LoadFontEx(path, 26, cps.data(), static_cast<int>(cps.size()));
        if (IsFontValid(f) && f.glyphCount > 90) {
            ui = f;
            uiIsTtf = true;
            fontPath = path;
            break;
        }
        UnloadFont(f);
    }
    if (!uiIsTtf) ui = GetFontDefault();
    SetTextureFilter(ui.texture, TEXTURE_FILTER_BILINEAR);

    if (uiIsTtf) {
        // same TTF through the memory loader at a big size for titles
        int size = 0;
        unsigned char* data = LoadFileData(fontPath.c_str(), &size);
        if (data) {
            title = LoadFontFromMemory(GetFileExtension(fontPath.c_str()), data, size, 64, cps.data(), static_cast<int>(cps.size()));
            titleIsTtf = IsFontValid(title);
            // manual pipeline: LoadFontData -> GenImageFontAtlas -> texture
            int glyphCount = 0;
            GlyphInfo* glyphs = LoadFontData(data, size, 30, nullptr, 95, FONT_DEFAULT, &glyphCount);
            if (glyphs) {
                Rectangle* recs = nullptr;
                Image atlasImg = GenImageFontAtlas(glyphs, &recs, glyphCount, 30, 4, 0);
                atlas.baseSize = 30;
                atlas.glyphCount = glyphCount;
                atlas.glyphPadding = 4;
                atlas.glyphs = glyphs;
                atlas.recs = recs;
                atlas.texture = LoadTextureFromImage(atlasImg);
                UnloadImage(atlasImg);
                atlasOwned = true;
            }
            // signed-distance-field glyph generation (would need an SDF shader to draw): probe + release
            GlyphInfo* sdf = LoadFontData(data, size, 32, nullptr, 95, FONT_SDF, &sdfGlyphs);
            if (sdf) UnloadFontData(sdf, sdfGlyphs);
            UnloadFileData(data);
        }
        if (titleIsTtf) SetTextureFilter(title.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (!titleIsTtf) title = ui;
    if (uiIsTtf) {
        mono = LoadFont(fontPath.c_str());  // simplest loader: 32px, ASCII 32..126
        monoOwned = IsFontValid(mono);
    }
    if (!monoOwned) mono = GetFontDefault();
    if (!atlasOwned) atlas = BuildBitmapFont(report);  // fallback also shows LoadFontFromImage
    else {
        Font bmp = BuildBitmapFont(report);  // still exercise the image-font path once
        UnloadFont(bmp);
    }

    report += TextFormat("font: %s (%d glyphs)\n", uiIsTtf ? fontPath.c_str() : "raylib default", ui.glyphCount);
    TraceLog(LOG_INFO, "ASSETS: loaded\n%s", report.c_str());
    return IsTextureValid(ship) && IsTextureValid(panel) && IsFontValid(ui);
}

void Assets::animateNoise(unsigned int frame) {
    if (!IsTextureValid(noise) || !IsImageValid(noiseImage)) return;
    // shift a band of pixels every frame (cheap animated grain)
    Color* px = static_cast<Color*>(noiseImage.data);
    const int w = noiseImage.width, h = noiseImage.height;
    const int row = static_cast<int>(frame % static_cast<unsigned int>(h));
    for (int x = 0; x < w; ++x) px[row * w + x].r = static_cast<unsigned char>(GetRandomValue(0, 255));
    if (frame % 30 == 0) {
        UpdateTexture(noise, noiseImage.data);  // full upload
    } else {
        UpdateTextureRec(noise, {0, static_cast<float>(row), static_cast<float>(w), 1}, &px[row * w]);  // one row
    }
}

bool Assets::setPlayerSkin(Image img) {
    if (!IsImageValid(img)) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageAlphaCrop(&img, 0.05f);
    ImageResize(&img, 40, 40);
    UnloadTexture(ship);
    ship = Upload(img);
    return true;
}

void Assets::unload() {
    Texture2D* all[] = {&ship, &gem, &heal, &glow, &particle, &panel, &bg, &floorTex, &noise, &explosion, &icon, &white};
    for (Texture2D* t : all)
        if (IsTextureValid(*t)) UnloadTexture(*t);
    for (auto& e : enemy)
        if (IsTextureValid(e)) UnloadTexture(e);
    if (IsImageValid(noiseImage)) UnloadImage(noiseImage);
    if (titleIsTtf) UnloadFont(title);
    if (monoOwned) UnloadFont(mono);
    if (uiIsTtf) UnloadFont(ui);
    UnloadFont(atlas);  // default-font aliases are ignored by UnloadFont
    *this = Assets{};
}

}  // namespace rd
