// Showcase: an interactive tour through raylib's modules, one page each.
//   Q/E or PgUp/PgDn or click a tab to switch pages, Esc back to title.
#include "app/context.hpp"
#include "app/logger.hpp"
#include "audio/wavgen.hpp"
#include "core/easing.hpp"
#include "core/math2d.hpp"
#include "game/hashing.hpp"
#include "gfx/image_lab.hpp"
#include "gfx/procgen.hpp"
#include "rlgl.h"
#include "ui/ui_logic.hpp"

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace rd {

namespace {

const char* kPages[] = {"Shapes", "Splines", "Collision", "Image Lab", "Textures", "Text", "Audio", "Input/System"};
constexpr int kPageCount = 8;

const char* GestureName(int g) {
    switch (g) {
        case GESTURE_TAP: return "TAP";
        case GESTURE_DOUBLETAP: return "DOUBLETAP";
        case GESTURE_HOLD: return "HOLD";
        case GESTURE_DRAG: return "DRAG";
        case GESTURE_SWIPE_RIGHT: return "SWIPE_RIGHT";
        case GESTURE_SWIPE_LEFT: return "SWIPE_LEFT";
        case GESTURE_SWIPE_UP: return "SWIPE_UP";
        case GESTURE_SWIPE_DOWN: return "SWIPE_DOWN";
        case GESTURE_PINCH_IN: return "PINCH_IN";
        case GESTURE_PINCH_OUT: return "PINCH_OUT";
        default: return "NONE";
    }
}

const char* BlendName(int b) {
    static const char* n[] = {"ALPHA", "ADDITIVE", "MULTIPLIED", "ADD_COLORS", "SUBTRACT_COLORS", "ALPHA_PREMULTIPLY", "CUSTOM", "CUSTOM_SEPARATE"};
    return (b >= 0 && b < 8) ? n[b] : "?";
}

class ShowcaseScene final : public Scene {
public:
    const char* name() const override { return "Showcase"; }

    void enter(AppContext& ctx) override {
        page_ = ctx.showcasePage;
        t_ = 0.f;
        ShowCursor();
        // spline control points (7 = valid for quadratic (2n+1) and cubic (3n+1) beziers)
        for (int i = 0; i < 7; ++i) spline_[i] = {120.f + i * 110.f, 300.f + ((i % 2) ? -110.f : 90.f)};
        // image lab
        labBase_ = GenShipImage(96, SKYBLUE, GOLD);
        ImageResizeCanvas(&labBase_, 128, 128, 16, 16, BLANK);
        Image grad = GenImageGradientLinear(128, 128, 45, Fade(VIOLET, 0.6f), Fade(DARKBLUE, 0.3f));
        ImageDraw(&grad, labBase_, {0, 0, 128, 128}, {0, 0, 128, 128}, WHITE);
        UnloadImage(labBase_);
        labBase_ = grad;
        labImg_ = ImageCopy(labBase_);
        labTexBase_ = LoadTextureFromImage(labBase_);
        labTex_ = LoadTextureFromImage(labImg_);
        palette_ = DominantPalette(labImg_, 8);
        avg_ = AverageColor(labImg_);
        canvas_ = GenImageColor(160, 120, Fade(BLACK, 0.8f));
        canvasTex_ = LoadTextureFromImage(canvas_);
        // text page: CPU text rendering into images
        Image txt = GenImageColor(340, 60, Fade(DARKBLUE, 0.7f));
        ImageDrawText(&txt, "ImageDrawText", 8, 6, 20, RAYWHITE);
        ImageDrawTextEx(&txt, ctx.assets->ui, "ImageDrawTextEx 中文", {8, 30}, 22, 1, GOLD);
        Image label = ImageText("ImageText", 20, LIME);
        Image labelEx = ImageTextEx(ctx.assets->ui, "ImageTextEx", 22, 1, SKYBLUE);
        const float lx = 340.f - label.width - 8.f;
        ImageDraw(&txt, label, {0, 0, (float)label.width, (float)label.height}, {lx, 6, (float)label.width, (float)label.height}, WHITE);
        textImgTex_ = LoadTextureFromImage(txt);
        textImgTex2_ = LoadTextureFromImage(labelEx);
        UnloadImage(label);
        UnloadImage(labelEx);
        UnloadImage(txt);
        cps_ = LoadCodepoints("你好世界 raylib ★ 霓虹", &cpsCount_);
        rt_ = LoadRenderTexture(128, 128);
        input_.set("Hello raylib_demo World");
        randomSeq_.clear();
        refreshFiles(ctx);
    }

    void exit(AppContext& ctx) override {
        ctx.showcasePage = page_;
        UnloadImage(labBase_);
        UnloadImage(labImg_);
        UnloadImage(canvas_);
        UnloadTexture(labTexBase_);
        UnloadTexture(labTex_);
        UnloadTexture(canvasTex_);
        UnloadTexture(textImgTex_);
        UnloadTexture(textImgTex2_);
        if (IsTextureValid(screenTex_)) UnloadTexture(screenTex_);
        if (IsTextureValid(clipTex_)) UnloadTexture(clipTex_);
        UnloadCodepoints(cps_);
        UnloadRenderTexture(rt_);
        SetTextLineSpacing(2);
    }

    void update(AppContext& ctx, float dt) override {
        t_ += dt;
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_PAGE_DOWN)) page_ = (page_ + 1) % kPageCount;
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_PAGE_UP)) page_ = (page_ + kPageCount - 1) % kPageCount;
        if (IsKeyPressed(KEY_ESCAPE)) ctx.go(SceneId::Title);
        if (ctx.autopilot) {
            autoT_ += dt;
            if (autoT_ > 0.5f) {
                autoT_ = 0.f;
                if (++page_ >= kPageCount) { page_ = 0; ctx.go(SceneId::Title); }
            }
        }
        switch (page_) {
            case 1: updateSplines(); break;
            case 3: updateImageLab(ctx); break;
            case 5: updateText(); break;
            default: break;
        }
        if (page_ == 7) {
            for (int k = GetKeyPressed(); k > 0; k = GetKeyPressed()) {
                keyLog_.insert(keyLog_.begin(), k);
                if (keyLog_.size() > 8) keyLog_.pop_back();
            }
            if (IsKeyReleased(KEY_SPACE)) ++spaceReleases_;
        }
    }

    void prerender(AppContext&) override {
        // offscreen render texture pass (texture modes cannot nest)
        BeginTextureMode(rt_);
        ClearBackground(Fade(BLACK, 0.f));
        DrawPoly({64, 64}, 5, 50, t_ * 90.f, Fade(GOLD, 0.8f));
        DrawPolyLinesEx({64, 64}, 5, 56, -t_ * 60.f, 3, SKYBLUE);
        DrawText("RT", 52, 56, 20, BLACK);
        EndTextureMode();
    }

    void draw(AppContext& ctx) override {
        ClearBackground(GetColor(0x0A0D18FF));
        DrawRectangleGradientV(0, 0, ctx.vw, ctx.vh, GetColor(0x0E1328FF), GetColor(0x06080FFF));
        drawTabs(ctx);
        switch (page_) {
            case 0: drawShapes(ctx); break;
            case 1: drawSplines(ctx); break;
            case 2: drawCollision(ctx); break;
            case 3: drawImageLab(ctx); break;
            case 4: drawTextures(ctx); break;
            case 5: drawText(ctx); break;
            case 6: drawAudio(ctx); break;
            case 7: drawInputSystem(ctx); break;
            default: break;
        }
    }

private:
    // ---------------------------------------------------------------------------
    void drawTabs(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        const float w = (ctx.vw - 20.f) / kPageCount;
        for (int i = 0; i < kPageCount; ++i) {
            const Rectangle r{10 + i * w, 6, w - 4, 26};
            const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
            DrawRectangleRounded(r, 0.4f, 6, i == page_ ? Fade(th.accent, 0.85f) : Fade(hover ? th.accent : GRAY, 0.25f));
            UiTextCentered(th, TextFormat("%d %s", i + 1, kPages[i]), {r.x + r.width * 0.5f, r.y + 13}, 15, i == page_ ? BLACK : RAYWHITE);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) page_ = i;
        }
        UiText(th, "Q/E switch page   Esc back", {10, ctx.vh - 18.f}, 14, th.muted);
    }

    static void Label(const UiTheme& th, const char* s, float x, float y) { UiText(th, s, {x, y}, 13, th.muted); }

    // --- 1: shapes -----------------------------------------------------------------
    void drawShapes(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        const float spin = t_ * 60.f;
        using Fn = std::function<void(Rectangle)>;
        const std::vector<std::pair<const char*, Fn>> items = {
            {"DrawPixel(V)", [&](Rectangle r) { for (int i = 0; i < 40; ++i) DrawPixel((int)r.x + 10 + (i * 37) % 90, (int)r.y + 10 + (i * 53) % 40, RAYWHITE); DrawPixelV({r.x + 55, r.y + 30}, RED); }},
            {"DrawLine / V", [&](Rectangle r) { DrawLine((int)r.x + 10, (int)r.y + 10, (int)r.x + 100, (int)r.y + 50, GOLD); DrawLineV({r.x + 10, r.y + 50}, {r.x + 100, r.y + 10}, SKYBLUE); }},
            {"DrawLineEx", [&](Rectangle r) { DrawLineEx({r.x + 10, r.y + 30}, {r.x + 100, r.y + 30 + 15 * std::sin(t_ * 3)}, 6, LIME); }},
            {"DrawLineStrip", [&](Rectangle r) { Vector2 p[6]; for (int i = 0; i < 6; ++i) p[i] = {r.x + 10 + i * 18.f, r.y + 30 + ((i % 2) ? -15.f : 15.f)}; DrawLineStrip(p, 6, ORANGE); }},
            {"DrawLineBezier", [&](Rectangle r) { DrawLineBezier({r.x + 10, r.y + 50}, {r.x + 100, r.y + 10}, 3, PINK); }},
            {"DrawLineDashed", [&](Rectangle r) { DrawLineDashed({r.x + 10, r.y + 30}, {r.x + 100, r.y + 30}, 8, 5, RAYWHITE); }},
            {"DrawCircle / V", [&](Rectangle r) { DrawCircle((int)r.x + 35, (int)r.y + 30, 18, MAROON); DrawCircleV({r.x + 80, r.y + 30}, 14, RED); }},
            {"DrawCircleGradient", [&](Rectangle r) { DrawCircleGradient({r.x + 55, r.y + 30}, 25, WHITE, BLANK); }},
            {"DrawCircleSector", [&](Rectangle r) { DrawCircleSector({r.x + 55, r.y + 32}, 24, spin, spin + 270, 24, GOLD); }},
            {"DrawCircleSectorLines", [&](Rectangle r) { DrawCircleSectorLines({r.x + 55, r.y + 32}, 24, -spin, -spin + 200, 16, GOLD); }},
            {"DrawCircleLines / V", [&](Rectangle r) { DrawCircleLines((int)r.x + 35, (int)r.y + 30, 18, SKYBLUE); DrawCircleLinesV({r.x + 80, r.y + 30}, 14, BLUE); }},
            {"DrawEllipse / V", [&](Rectangle r) { DrawEllipse((int)r.x + 35, (int)r.y + 30, 22, 12, PURPLE); DrawEllipseV({r.x + 85, r.y + 30}, 10, 20, VIOLET); }},
            {"DrawEllipseLines / V", [&](Rectangle r) { DrawEllipseLines((int)r.x + 35, (int)r.y + 30, 22, 12, PURPLE); DrawEllipseLinesV({r.x + 85, r.y + 30}, 10, 20, VIOLET); }},
            {"DrawRing", [&](Rectangle r) { DrawRing({r.x + 55, r.y + 32}, 12, 24, spin, spin + 300, 32, LIME); }},
            {"DrawRingLines", [&](Rectangle r) { DrawRingLines({r.x + 55, r.y + 32}, 12, 24, 0, 360, 32, GREEN); }},
            {"DrawRectangle / V / Rec", [&](Rectangle r) { DrawRectangle((int)r.x + 8, (int)r.y + 12, 30, 36, DARKBLUE); DrawRectangleV({r.x + 42, r.y + 12}, {30, 36}, BLUE); DrawRectangleRec({r.x + 76, r.y + 12, 30, 36}, SKYBLUE); }},
            {"DrawRectanglePro", [&](Rectangle r) { DrawRectanglePro({r.x + 55, r.y + 32, 40, 22}, {20, 11}, spin, ORANGE); }},
            {"DrawRectangleGradientV/H", [&](Rectangle r) { DrawRectangleGradientV((int)r.x + 8, (int)r.y + 10, 45, 42, RED, GOLD); DrawRectangleGradientH((int)r.x + 58, (int)r.y + 10, 45, 42, BLUE, LIME); }},
            {"DrawRectangleGradientEx", [&](Rectangle r) { DrawRectangleGradientEx({r.x + 10, r.y + 10, 90, 42}, RED, GREEN, BLUE, YELLOW); }},
            {"DrawRectangleLines / Ex", [&](Rectangle r) { DrawRectangleLines((int)r.x + 8, (int)r.y + 10, 45, 42, RAYWHITE); DrawRectangleLinesEx({r.x + 58, r.y + 10, 45, 42}, 4, GOLD); }},
            {"DrawRectangleRounded", [&](Rectangle r) { DrawRectangleRounded({r.x + 10, r.y + 10, 90, 42}, 0.3f + 0.3f * std::sin(t_), 8, DARKGREEN); }},
            {"RoundedLines / Ex", [&](Rectangle r) { DrawRectangleRoundedLines({r.x + 8, r.y + 10, 45, 42}, 0.5f, 8, LIME); DrawRectangleRoundedLinesEx({r.x + 58, r.y + 10, 45, 42}, 0.5f, 8, 3, GREEN); }},
            {"DrawTriangle / Lines", [&](Rectangle r) { DrawTriangle({r.x + 30, r.y + 8}, {r.x + 10, r.y + 52}, {r.x + 50, r.y + 52}, RED); DrawTriangleLines({r.x + 80, r.y + 8}, {r.x + 60, r.y + 52}, {r.x + 100, r.y + 52}, PINK); }},
            {"DrawTriangleFan", [&](Rectangle r) { auto pts = RegularPolygon({r.x + 55, r.y + 32}, 6, 24, spin); std::vector<Vector2> fan{{r.x + 55, r.y + 32}}; fan.insert(fan.end(), pts.begin(), pts.end()); fan.push_back(pts.front()); /* RegularPolygon is already CCW on screen */ DrawTriangleFan(fan.data(), (int)fan.size(), GOLD); }},
            {"DrawTriangleStrip", [&](Rectangle r) { Vector2 s[8]; for (int i = 0; i < 8; ++i) s[i] = {r.x + 10 + (i / 2) * 28.f, r.y + ((i % 2) ? 52.f : 12.f)}; DrawTriangleStrip(s, 8, SKYBLUE); }},
            {"DrawPoly", [&](Rectangle r) { DrawPoly({r.x + 35, r.y + 32}, 5, 20, spin, VIOLET); DrawPoly({r.x + 80, r.y + 32}, 8, 18, -spin, PURPLE); }},
            {"DrawPolyLines / Ex", [&](Rectangle r) { DrawPolyLines({r.x + 35, r.y + 32}, 6, 20, spin, VIOLET); DrawPolyLinesEx({r.x + 80, r.y + 32}, 3, 20, -spin, 4, PINK); }},
            {"DrawFPS / DrawText", [&](Rectangle r) { DrawFPS((int)r.x + 8, (int)r.y + 8); DrawText("DrawText", (int)r.x + 8, (int)r.y + 34, 16, RAYWHITE); }},
        };
        const auto cells = HGrid({10, 40, ctx.vw - 20.f, ctx.vh - 64.f}, 7, 4, 6);
        for (size_t i = 0; i < items.size() && i < cells.size(); ++i) {
            const Rectangle c = cells[i];
            DrawRectangleRec(c, Fade(BLACK, 0.35f));
            DrawRectangleLinesEx(c, 1, Fade(th.accent, 0.25f));
            items[i].second({c.x + (c.width - 110) * 0.5f, c.y + 6, 110, 60});
            Label(th, items[i].first, c.x + 4, c.y + c.height - 16);
        }
    }

    // --- 2: splines --------------------------------------------------------------------
    void updateSplines() {
        const Vector2 m = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for (int i = 0; i < 7; ++i)
                if (CheckCollisionPointCircle(m, spline_[i], 12)) drag_ = i;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) drag_ = -1;
        if (drag_ >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) spline_[drag_] = m;
        if (IsKeyPressed(KEY_SPACE)) splineType_ = (splineType_ + 1) % 5;
    }

    void drawSplines(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        static const char* names[5] = {"DrawSplineLinear", "DrawSplineBasis (B-spline)", "DrawSplineCatmullRom", "DrawSplineBezierQuadratic", "DrawSplineBezierCubic"};
        const float thick = 4.f;
        switch (splineType_) {
            case 0: DrawSplineLinear(spline_, 7, thick, RED); break;
            case 1: DrawSplineBasis(spline_, 7, thick, GOLD); break;
            case 2: DrawSplineCatmullRom(spline_, 7, thick, LIME); break;
            case 3: DrawSplineBezierQuadratic(spline_, 7, thick, SKYBLUE); break;
            default: DrawSplineBezierCubic(spline_, 7, thick, VIOLET); break;
        }
        DrawLineStrip(spline_, 7, Fade(GRAY, 0.4f));
        for (int i = 0; i < 7; ++i) {
            DrawCircleV(spline_[i], drag_ == i ? 10 : 7, drag_ == i ? th.accent : RAYWHITE);
            DrawText(TextFormat("%d", i), static_cast<int>(spline_[i].x) + 10, static_cast<int>(spline_[i].y) - 10, 10, th.muted);
        }
        // moving marker via GetSplinePoint* on the first segment of the selected type
        const float u = ease::pingPong(std::fmod(t_ * 0.3f, 1.f));
        Vector2 pt{};
        switch (splineType_) {
            case 0: pt = GetSplinePointLinear(spline_[0], spline_[1], u); break;
            case 1: pt = GetSplinePointBasis(spline_[0], spline_[1], spline_[2], spline_[3], u); break;
            case 2: pt = GetSplinePointCatmullRom(spline_[0], spline_[1], spline_[2], spline_[3], u); break;
            case 3: pt = GetSplinePointBezierQuad(spline_[0], spline_[1], spline_[2], u); break;
            default: pt = GetSplinePointBezierCubic(spline_[0], spline_[1], spline_[2], spline_[3], u); break;
        }
        DrawCircleV(pt, 9, WHITE);
        DrawCircleLinesV(pt, 13, th.accent);
        // single-segment helpers along the bottom
        const float y = ctx.vh - 70.f;
        DrawSplineSegmentLinear({30, y}, {130, y - 20}, 3, RED);
        DrawSplineSegmentBasis({160, y}, {200, y - 40}, {260, y + 20}, {300, y - 10}, 3, GOLD);
        DrawSplineSegmentCatmullRom({330, y}, {370, y - 30}, {430, y + 10}, {470, y - 20}, 3, LIME);
        DrawSplineSegmentBezierQuadratic({500, y}, {560, y - 60}, {620, y}, 3, SKYBLUE);
        DrawSplineSegmentBezierCubic({650, y}, {690, y - 60}, {760, y + 40}, {800, y - 10}, 3, VIOLET);
        UiText(th, TextFormat("%s  (SPACE: next type, drag points)", names[splineType_]), {20, 44}, 20, th.accent);
        Label(th, "DrawSplineSegment{Linear,Basis,CatmullRom,BezierQuadratic,BezierCubic}", 20, ctx.vh - 40.f);
    }

    // --- 3: collision ----------------------------------------------------------------------
    void drawCollision(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        const Vector2 m = GetMousePosition();
        probeR_ = Clamp(probeR_ + GetMouseWheelMove() * 3.f, 6.f, 80.f);
        auto col = [](bool hit, Color base) { return hit ? RED : base; };

        const Rectangle rect{80, 80, 150, 100};
        const bool hRect = CheckCollisionCircleRec(m, probeR_, rect);
        DrawRectangleRec(rect, Fade(col(hRect, SKYBLUE), 0.5f));
        Label(th, "CheckCollisionCircleRec", rect.x, rect.y - 16);

        const Vector2 cc{400, 130};
        const bool hCirc = CheckCollisionCircles(m, probeR_, cc, 50);
        DrawCircleV(cc, 50, Fade(col(hCirc, LIME), 0.5f));
        Label(th, "CheckCollisionCircles", cc.x - 60, cc.y - 70);

        const Vector2 t1{560, 190}, t2{700, 190}, t3{630, 70};
        const bool hTri = CheckCollisionPointTriangle(m, t1, t2, t3);
        DrawTriangle(t3, t1, t2, Fade(col(hTri, GOLD), 0.5f));
        Label(th, "CheckCollisionPointTriangle", 560, 200);

        const Vector2 poly[6] = {{780, 80}, {880, 70}, {920, 140}, {860, 200}, {800, 190}, {760, 130}};
        const bool hPoly = CheckCollisionPointPoly(m, poly, 6);
        Vector2 fan[8] = {{840, 135}, poly[0], poly[5], poly[4], poly[3], poly[2], poly[1], poly[0]};
        DrawTriangleFan(fan, 8, Fade(col(hPoly, VIOLET), 0.5f));
        Label(th, "CheckCollisionPointPoly", 770, 210);

        const Vector2 l1{80, 300}, l2{300, 420};
        const bool hLine = CheckCollisionCircleLine(m, probeR_, l1, l2);
        const bool hPtLine = CheckCollisionPointLine(m, l1, l2, 6);
        DrawLineEx(l1, l2, 4, col(hLine, ORANGE));
        Label(th, hPtLine ? "CheckCollisionPointLine: ON LINE" : "CheckCollisionCircleLine / PointLine", 80, 430);

        const Vector2 a1{380, 280}, a2{600, 440}, center{490, 360};
        Vector2 hit{};
        const bool hLines = CheckCollisionLines(a1, a2, center, m, &hit);
        DrawLineEx(a1, a2, 3, PINK);
        DrawLineV(center, m, Fade(WHITE, 0.4f));
        if (hLines) DrawCircleV(hit, 6, RED);
        Label(th, "CheckCollisionLines (+ point)", 380, 450);

        const Rectangle fixed{680, 280, 140, 120};
        const Rectangle moving{m.x - 50, m.y - 35, 100, 70};
        DrawRectangleLinesEx(fixed, 2, GREEN);
        if (CheckCollisionRecs(fixed, moving)) {
            const Rectangle ov = GetCollisionRec(fixed, moving);
            DrawRectangleRec(ov, Fade(RED, 0.6f));
            Label(th, TextFormat("GetCollisionRec %.0fx%.0f", ov.width, ov.height), 680, 410);
        } else {
            Label(th, "CheckCollisionRecs", 680, 410);
        }
        DrawRectangleLinesEx(moving, 1, Fade(WHITE, 0.3f));

        const bool inProbe = CheckCollisionPointCircle(cc, m, probeR_);
        const bool inRect = CheckCollisionPointRec(m, rect);
        DrawCircleLinesV(m, probeR_, inProbe || inRect ? RED : RAYWHITE);
        UiText(th, TextFormat("probe r=%.0f (wheel)   PointRec=%d  PointCircle=%d", probeR_, inRect, inProbe), {20, 44}, 18, th.accent);
    }

    // --- 4: image lab ------------------------------------------------------------------------
    void reuploadLab() {
        UnloadTexture(labTex_);
        Image show = ImageCopy(labImg_);
        if (show.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(&show, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        labTex_ = LoadTextureFromImage(show);
        UnloadImage(show);
        palette_ = DominantPalette(labImg_, 8);
        avg_ = AverageColor(labImg_);
    }

    void updateImageLab(AppContext& ctx) {
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) op_ = (op_ + 1) % kImageOpCount;
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) op_ = (op_ + kImageOpCount - 1) % kImageOpCount;
        if (IsKeyPressed(KEY_ENTER)) {
            ApplyImageOp(labImg_, static_cast<ImageOp>(op_));
            if (labImg_.width > 512 || labImg_.height > 512) ImageResize(&labImg_, 256, 256);
            reuploadLab();
        }
        if (IsKeyPressed(KEY_R)) {
            UnloadImage(labImg_);
            labImg_ = ImageCopy(labBase_);
            reuploadLab();
        }
        if (IsKeyPressed(KEY_X)) {
            const std::string p = ctx.storage->path("exports/image_lab.png");
            int size = 0;
            unsigned char* png = ExportImageToMemory(labImg_, ".png", &size);
            const bool ok = ExportImage(labImg_, p.c_str());
            ctx.notify(TextFormat("ExportImage %s (%d bytes in memory)", ok ? "ok" : "FAILED", size));
            if (png) MemFree(png);
        }
        if (IsKeyPressed(KEY_G)) {  // grab the last presented frame
            Image scr = LoadImageFromScreen();
            if (IsImageValid(scr)) {
                ImageResize(&scr, 192, 108);
                if (IsTextureValid(screenTex_)) UnloadTexture(screenTex_);
                screenTex_ = LoadTextureFromImage(scr);
            }
            UnloadImage(scr);
        }
        // paint canvas: CPU ImageDrawCircle + partial GPU upload (UpdateTextureRec)
        const Rectangle cv{canvasPos_.x, canvasPos_.y, static_cast<float>(canvas_.width), static_cast<float>(canvas_.height)};
        const Vector2 m = GetMousePosition();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, cv)) {
            const Vector2 local{m.x - cv.x, m.y - cv.y};
            ImageDrawCircleV(&canvas_, local, 5, ColorFromHSV(std::fmod(t_ * 120.f, 360.f), 0.8f, 1.f));
            Rectangle dirty{local.x - 6, local.y - 6, 12, 12};
            dirty = GetCollisionRec(dirty, {0, 0, cv.width, cv.height});
            if (dirty.width > 0 && dirty.height > 0) {
                Image sub = ImageFromImage(canvas_, dirty);
                UpdateTextureRec(canvasTex_, dirty, sub.data);
                UnloadImage(sub);
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(m, cv)) {
            ImageClearBackground(&canvas_, Fade(BLACK, 0.8f));
            UpdateTexture(canvasTex_, canvas_.data);
        }
    }

    void drawImageLab(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        // op list
        for (int i = 0; i < kImageOpCount; ++i) {
            const float y = 42.f + i * 16.f;
            if (i == op_) DrawRectangle(12, static_cast<int>(y) - 1, 250, 16, Fade(th.accent, 0.35f));
            UiText(th, ImageOpName(static_cast<ImageOp>(i)), {16, y}, 14, i == op_ ? RAYWHITE : th.muted);
        }
        DrawTexture(labTexBase_, 290, 60, WHITE);
        Label(th, "original", 290, 44);
        const float scale = std::min(1.f, 256.f / std::max(labTex_.width, labTex_.height));
        DrawTextureEx(labTex_, {440, 60}, 0.f, scale, WHITE);
        const Rectangle border = GetImageAlphaBorder(labImg_, 0.1f);
        DrawRectangleLinesEx({440 + border.x * scale, 60 + border.y * scale, border.width * scale, border.height * scale}, 1, Fade(RED, 0.7f));
        UiText(th, TextFormat("%dx%d  %s  mips=%d  alphaBorder=%.0fx%.0f", labImg_.width, labImg_.height, PixelFormatName(labImg_.format), labImg_.mipmaps, border.width, border.height), {440, 44}, 14, th.accent);
        // palette + average
        for (size_t i = 0; i < palette_.size(); ++i) DrawRectangle(290 + static_cast<int>(i) * 18, 200, 16, 16, palette_[i]);
        Label(th, "LoadImagePalette", 290, 218);
        DrawRectangle(290, 240, 34, 16, avg_);
        Label(th, "avg (LoadImageColors)", 330, 240);
        // paint canvas
        DrawTexture(canvasTex_, static_cast<int>(canvasPos_.x), static_cast<int>(canvasPos_.y), WHITE);
        DrawRectangleLines(static_cast<int>(canvasPos_.x), static_cast<int>(canvasPos_.y), canvas_.width, canvas_.height, th.accent);
        Label(th, "paint: LMB (UpdateTextureRec)  RMB clear (UpdateTexture)", canvasPos_.x, canvasPos_.y + canvas_.height + 4);
        if (IsTextureValid(screenTex_)) {
            DrawTexture(screenTex_, 740, 330, WHITE);
            Label(th, "LoadImageFromScreen", 740, 442);
        }
        UiText(th, "UP/DOWN select op  ENTER apply  R reset  X export PNG  G grab screen", {290, ctx.vh - 40.f}, 16, th.muted);
    }

    // --- 5: textures / blend / npatch -----------------------------------------------------------
    void drawTextures(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        const Assets& a = *ctx.assets;
        const Vector2 m = GetMousePosition();
        // n-patch stretching
        const float w = Clamp(m.x - 20.f, 60.f, 300.f), h = Clamp(m.y - 50.f, 40.f, 160.f);
        DrawTextureNPatch(a.panel, a.panelPatch, {20, 50, w, h}, {0, 0}, 0.f, WHITE);
        const NPatchInfo three{{0, 0, 48, 48}, 12, 0, 12, 0, NPATCH_THREE_PATCH_HORIZONTAL};
        DrawTextureNPatch(a.panel, three, {20, 230, 300, 30}, {0, 0}, 0.f, GOLD);
        const NPatchInfo vert{{0, 0, 48, 48}, 0, 12, 0, 12, NPATCH_THREE_PATCH_VERTICAL};
        DrawTextureNPatch(a.panel, vert, {330, 50, 30, 210}, {0, 0}, 0.f, LIME);
        Label(th, "DrawTextureNPatch: 9-patch (follows mouse), 3-patch H/V", 20, 264);

        // blend modes
        for (int b = 0; b < 8; ++b) {
            const float x = 400 + (b % 4) * 135.f, y = 50 + (b / 4) * 110.f;
            DrawTexturePro(a.floorTex, {0, 0, 64, 64}, {x, y, 120, 90}, {0, 0}, 0.f, WHITE);
            if (b == BLEND_CUSTOM) rlSetBlendFactors(RL_DST_COLOR, RL_ONE, RL_FUNC_ADD);
            if (b == BLEND_CUSTOM_SEPARATE) rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE, RL_ONE, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(b);
            DrawCircleV({x + 45, y + 45}, 30, Fade(RED, 0.8f));
            DrawCircleV({x + 75, y + 45}, 30, Fade(SKYBLUE, 0.8f));
            DrawTextureEx(a.glow, {x + 30, y + 10}, 0.f, 0.5f, GOLD);
            EndBlendMode();
            Label(th, BlendName(b), x, y + 92);
        }

        // filter & wrap comparison (DrawTextureRec with oversized source -> wrap)
        const float y2 = 300;
        DrawTextureRec(a.noise, {0, 0, 128, 64}, {400, y2}, WHITE);
        Label(th, "noise: WRAP_REPEAT", 400, y2 + 66);
        Label(th, "UpdateTextureRec/frame", 400, y2 + 80);
        DrawTextureV(a.icon, {540, y2}, WHITE);
        DrawTexture(a.ship, 612, static_cast<int>(y2), WHITE);
        DrawTexturePro(a.explosion, {0, 0, static_cast<float>(a.explosion.width), 64}, {660, y2, 256, 32}, {0, 0}, 0.f, WHITE);
        Label(th, "DrawTextureV | DrawTexture | DrawTexturePro (sprite sheet)", 560, y2 + 66);

        // render texture from prerender()
        DrawTextureRec(rt_.texture, {0, 0, 128, -128}, {20, 300}, WHITE);
        Label(th, "RenderTexture (prerender pass)", 20, 430);

        // textured shapes: SetShapesTexture swaps the 1x1 white texel used by shapes
        const Texture2D prevTex = GetShapesTexture();
        const Rectangle prevRec = GetShapesTextureRectangle();
        SetShapesTexture(a.bg, {0, 0, static_cast<float>(a.bg.width), static_cast<float>(a.bg.height)});
        DrawRectangle(170, 300, 100, 60, WHITE);
        DrawTriangle({220, 370}, {170, 430}, {270, 430}, WHITE);
        SetShapesTexture(prevTex, prevRec);
        Label(th, TextFormat("SetShapesTexture (default id=%u)", prevTex.id), 160, 436);

        // rlgl immediate matrix stack
        rlPushMatrix();
        rlTranslatef(860, 420, 0);
        rlRotatef(t_ * 90.f, 0, 0, 1);
        rlScalef(1.f + 0.2f * std::sin(t_ * 3.f), 1.f, 1.f);
        DrawRectangle(-30, -12, 60, 24, VIOLET);
        rlPopMatrix();
        Label(th, TextFormat("rlgl matrix stack, GL ver enum %d", rlGetVersion()), 760, 450);
    }

    // --- 6: text ---------------------------------------------------------------------------------
    void updateText() {
        for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) input_.insertCodepoint(cp);
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) input_.backspace();
    }

    void drawText(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        const Assets& a = *ctx.assets;
        DrawText("DrawText (default font)", 20, 44, 20, RAYWHITE);
        DrawTextEx(a.ui, "DrawTextEx TTF 你好世界", {20, 70}, 26, 1, GOLD);
        const char* rot = "DrawTextPro";
        const Vector2 rm = MeasureTextEx(a.title, rot, 30, 1);
        DrawTextPro(a.title, rot, {420, 90}, {rm.x * 0.5f, rm.y * 0.5f}, std::sin(t_) * 15.f, 30, 1, SKYBLUE);
        DrawTextCodepoint(a.ui, 0x2605 /* ★ */, {560, 60}, 48, GOLD);
        DrawTextCodepoints(a.ui, cps_, cpsCount_, {620, 70}, 26, 2, LIME);
        const Vector2 cpm = MeasureTextCodepoints(a.ui, cps_, cpsCount_, 26, 2);
        Label(th, TextFormat("MeasureTextCodepoints %.0fx%.0f, MeasureText=%d", cpm.x, cpm.y, MeasureText("DrawText", 20)), 620, 100);
        DrawTextEx(a.atlas, "LoadFontData + GenImageFontAtlas / bitmap font", {20, 104}, 22, 1, VIOLET);

        SetTextLineSpacing(26);
        DrawTextEx(a.ui, "SetTextLineSpacing(26)\nmulti-line\nstrings", {20, 140}, 20, 1, RAYWHITE);
        SetTextLineSpacing(2);

        // glyph inspector
        const int cp = input_.text().empty() ? 'A' : GetCodepoint(input_.text().c_str(), &glyphSize_);
        const GlyphInfo gi = GetGlyphInfo(a.ui, cp);
        const Rectangle ar = GetGlyphAtlasRec(a.ui, cp);
        DrawTexturePro(a.ui.texture, ar, {230, 140, ar.width * 2, ar.height * 2}, {0, 0}, 0, WHITE);
        Label(th, TextFormat("glyph U+%04X idx=%d adv=%d off=%d,%d atlas=%.0f,%.0f", cp, GetGlyphIndex(a.ui, cp), gi.advanceX, gi.offsetX, gi.offsetY, ar.x, ar.y), 300, 150);
        DrawTextureEx(a.ui.texture, {760, 130}, 0.f, 180.f / std::max(1, a.ui.texture.width), WHITE);
        Label(th, "font atlas", 760, 312);

        // CPU text rendering into images
        DrawTexture(textImgTex_, 230, 184, WHITE);
        DrawTexture(textImgTex2_, 580, 190, WHITE);

        // text input + utilities
        const Rectangle box{20, 250, 520, 36};
        UiPanel(th, box, th.accent);
        BeginScissorMode(static_cast<int>(box.x + 4), static_cast<int>(box.y), static_cast<int>(box.width - 8), static_cast<int>(box.height));
        UiText(th, (input_.text() + ((static_cast<int>(t_ * 2) % 2) ? "|" : "")).c_str(), {box.x + 10, box.y + 8}, 20, RAYWHITE);
        EndScissorMode();
        const char* s = input_.text().c_str();
        int words = 0;
        TextSplit(s, ' ', &words);
        char* upperCopy = TextReplaceAlloc(s, " ", "_");
        const std::string lines[] = {
            TextFormat("TextLength=%u bytes  GetCodepointCount=%d  words(TextSplit)=%d  TextFindIndex('o')=%d", TextLength(s), GetCodepointCount(s), words, TextFindIndex(s, "o")),
            TextFormat("TextToUpper: %s", TextToUpper(s)),
            TextFormat("TextToLower: %s", TextToLower(s)),
            TextFormat("TextToSnake: %s   TextToCamel: %s", TextToSnake(TextRemoveSpaces(s)), TextToCamel(TextReplace(s, " ", "_"))),
            TextFormat("TextToPascal: %s   TextSubtext(0,5): %s", TextToPascal(TextReplace(s, " ", "_")), TextSubtext(s, 0, 5)),
            TextFormat("TextReplaceAlloc(' ','_'): %s   TextRemoveSpaces: %s", upperCopy, TextRemoveSpaces(s)),
            // raylib 6.0 TextInsert reads past the end when pos+len(insert) > len(text): guard it
            TextFormat("raylib TextInsert(s,'<>',2): %s   TextInsertSafe: %s", TextLength(s) >= 4 ? TextInsert(s, "<>", 2) : "(skipped)",
                       TextInsertSafe(s, "<>", 2).c_str()),
            TextFormat("GetTextBetween('[a]b','[',']')=%s  TextReplaceBetween=%s  TextToInteger('42')=%d  TextToFloat('2.5')=%.1f",
                       GetTextBetween("[a]b", "[", "]"), TextReplaceBetween("x[a]y", "[", "]", "Q"), TextToInteger("42"), TextToFloat("2.5")),
        };
        MemFree(upperCopy);
        for (size_t i = 0; i < 8; ++i) UiText(th, lines[i].c_str(), {20, 294 + i * 18.f}, 14, i == 6 ? th.warn : RAYWHITE);
        const char* words3[] = {"TextJoin", "with", "pipes"};
        UiText(th, TextJoin(const_cast<char**>(words3), 3, " | "), {20, 444}, 14, th.muted);
        const auto wrapped = WrapText("WrapText measures each candidate line with MeasureTextEx and breaks greedily, keeping words intact.", 300,
                                      [&](const char* x) { return MeasureTextEx(a.ui, x, 16, 1).x; });
        for (size_t i = 0; i < wrapped.size(); ++i) UiText(th, wrapped[i].c_str(), {580, 400 + i * 18.f}, 16, SKYBLUE);
        if (IsKeyPressed(KEY_F9)) {
            const bool ok = ExportFontAsCode(a.atlas, ctx.storage->path("exports/atlas_font.h").c_str());
            ctx.notify(ok ? "ExportFontAsCode -> exports/atlas_font.h" : "ExportFontAsCode failed");
        }
        Label(th, "type to edit (GetCharPressed, UTF-8), F9 ExportFontAsCode", 20, ctx.vh - 40.f);
    }

    // --- 7: audio -------------------------------------------------------------------------------------
    void drawAudio(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        AudioSystem& au = *ctx.audio;
        if (!au.ready()) {
            UiTextCentered(th, "audio device not available", {ctx.vw * 0.5f, ctx.vh * 0.5f}, 28, th.warn);
            return;
        }
        for (int i = 0; i < kSfxCount; ++i) {
            const Rectangle r{20, 44.f + i * 40.f, 130, 34};
            Sound& snd = au.sound(static_cast<Sfx>(i));
            if (UiButton(th, r, SfxName(static_cast<Sfx>(i)), selSfx_ == i)) {
                selSfx_ = i;
                SetSoundPitch(snd, sfxPitch_);
                SetSoundPan(snd, sfxPan_);
                PlaySound(snd);
            }
            if (IsSoundPlaying(snd)) DrawCircle(160, static_cast<int>(r.y + 17), 5, LIME);
            const auto& wf = au.waveform(static_cast<Sfx>(i));
            if (!wf.empty()) {
                std::vector<Vector2> pts(wf.size());
                for (size_t k = 0; k < wf.size(); ++k) pts[k] = {175.f + k * 0.9f, r.y + 17 - wf[k] * 15.f};
                DrawLineStrip(pts.data(), static_cast<int>(pts.size()), i == selSfx_ ? th.accent : Fade(th.muted, 0.7f));
            }
        }
        Sound& sel = au.sound(static_cast<Sfx>(selSfx_));
        if (UiSlider(th, {430, 44, 300, 32}, "Pitch", &sfxPitch_, 0.5f, 2.f, false)) SetSoundPitch(sel, sfxPitch_);
        if (UiSlider(th, {430, 80, 300, 32}, "Pan", &sfxPan_, 0.f, 1.f, false)) SetSoundPan(sel, sfxPan_);
        if (UiButton(th, {740, 44, 90, 32}, "Pause", false)) PauseSound(sel);
        if (UiButton(th, {740, 80, 90, 32}, "Resume", false)) ResumeSound(sel);
        if (UiButton(th, {840, 44, 90, 32}, "Stop", false)) StopSound(sel);

        // music
        Music& mu = au.music();
        const Rectangle bar{430, 140, 500, 14};
        DrawRectangleRec(bar, Fade(BLACK, 0.5f));
        DrawRectangleRec({bar.x, bar.y, bar.width * au.musicProgress(), bar.height}, th.accent);
        if (CheckCollisionPointRec(GetMousePosition(), bar) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            SeekMusicStream(mu, (GetMouseX() - bar.x) / bar.width * GetMusicTimeLength(mu));
        UiText(th, TextFormat("Music %s  %.1f / %.1f s  %s  (click bar: SeekMusicStream)", au.musicFromDisk() ? "LoadMusicStream(file)" : "LoadMusicStreamFromMemory",
                              GetMusicTimePlayed(mu), GetMusicTimeLength(mu), IsMusicStreamPlaying(mu) ? "playing" : "paused"), {430, 120}, 14, RAYWHITE);
        if (UiButton(th, {430, 162, 110, 32}, IsMusicStreamPlaying(mu) ? "Pause" : "Resume", false)) {
            if (IsMusicStreamPlaying(mu)) PauseMusicStream(mu);
            else ResumeMusicStream(mu);
        }
        if (UiButton(th, {550, 162, 110, 32}, "Restart", false)) {
            StopMusicStream(mu);
            PlayMusicStream(mu);
        }
        if (UiButton(th, {670, 162, 140, 32}, "Swap source", false)) au.reloadMusicFromDisk();
        bool lp = au.lowPass();
        if (UiToggle(th, {820, 162, 110, 32}, "LPF", &lp, false)) au.setLowPass(lp);
        if (UiSlider(th, {430, 200, 250, 32}, "M.pitch", &musicPitch_, 0.5f, 1.5f, false)) SetMusicPitch(mu, musicPitch_);
        if (UiSlider(th, {690, 200, 240, 32}, "M.pan", &musicPan_, 0.f, 1.f, false)) SetMusicPan(mu, musicPan_);

        // streams
        bool hum = au.humOn(), pad = au.padOn();
        if (UiToggle(th, {430, 250, 240, 32}, "Callback stream (hum)", &hum, false)) au.setHum(hum, 0.5f);
        if (UiToggle(th, {690, 250, 240, 32}, "Push stream (pad)", &pad, false)) au.setPad(pad);
        if (UiSlider(th, {430, 288, 250, 32}, "Hum pitch", &humPitch_, 0.5f, 2.f, false)) SetAudioStreamPitch(au.humStream(), humPitch_);
        if (UiSlider(th, {690, 288, 240, 32}, "Hum pan", &humPan_, 0.f, 1.f, false)) SetAudioStreamPan(au.humStream(), humPan_);
        UiText(th, TextFormat("hum playing=%d  pad playing=%d  processed=%d", IsAudioStreamPlaying(au.humStream()), IsAudioStreamPlaying(au.padStream()),
                              IsAudioStreamProcessed(au.padStream())), {430, 326}, 14, th.muted);

        float master = GetMasterVolume();
        if (UiSlider(th, {430, 350, 500, 32}, "Master (SetMasterVolume)", &master, 0.f, 1.f, false)) SetMasterVolume(master);
        const float vu = std::min(1.f, au.vuLevel() * 3.f);
        DrawRectangleGradientH(430, 400, static_cast<int>(500 * vu), 18, GREEN, vu > 0.8f ? RED : YELLOW);
        DrawRectangleLines(430, 400, 500, 18, th.muted);
        Label(th, "VU from AttachAudioMixedProcessor", 430, 422);
    }

    // --- 8: input + system ------------------------------------------------------------------------------
    void refreshFiles(AppContext& ctx) {
        files_ = ctx.storage->list("", nullptr, true);
        const std::string s = SerializeSettings(*ctx.settings);
        hashLines_ = {"settings.ini fingerprints:", "CRC32  " + Crc32Hex(s.data(), (int)s.size()), "MD5    " + Md5Hex(s.data(), (int)s.size()),
                      "SHA1   " + Sha1Hex(s.data(), (int)s.size()), "SHA256 " + Sha256Hex(s).substr(0, 32) + "..."};
        const auto packed = Compress(s.data(), (int)s.size());
        hashLines_.push_back(TextFormat("deflate %d -> %d bytes, base64 %d chars", (int)s.size(), (int)packed.size(),
                                        (int)Base64Encode(packed.data(), (int)packed.size()).size()));
    }

    void drawInputSystem(AppContext& ctx) {
        const UiTheme& th = ctx.theme;
        float y = 44;
        auto line = [&](const std::string& s, Color c = RAYWHITE) {
            UiText(th, s.c_str(), {20, y}, 14, c);
            y += 17;
        };
        line("KEYBOARD", th.accent);
        std::string keys = "GetKeyPressed:";
        for (int k : keyLog_) {
            const char* n = GetKeyName(k);
            keys += TextFormat(" %d(%s)", k, (n && *n) ? n : "-");
        }
        line(keys);
        line(TextFormat("W down=%d up=%d  SPACE released x%d", IsKeyDown(KEY_W), IsKeyUp(KEY_W), spaceReleases_));
        line("MOUSE", th.accent);
        const Vector2 wv = GetMouseWheelMoveV();
        line(TextFormat("pos %d,%d  delta %.1f,%.1f  wheelV %.1f,%.1f", GetMouseX(), GetMouseY(), GetMouseDelta().x, GetMouseDelta().y, wv.x, wv.y));
        std::string btn = "buttons:";
        for (int b = MOUSE_BUTTON_LEFT; b <= MOUSE_BUTTON_BACK; ++b)
            btn += TextFormat(" %d[%c%c%c%c]", b, IsMouseButtonPressed(b) ? 'P' : '.', IsMouseButtonDown(b) ? 'D' : '.', IsMouseButtonReleased(b) ? 'R' : '.', IsMouseButtonUp(b) ? 'U' : '.');
        line(btn);
        if (IsKeyPressed(KEY_M)) SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
        line("M: SetMousePosition(center)", th.muted);

        line("GAMEPADS", th.accent);
        bool any = false;
        for (int g = 0; g < 4; ++g) {
            if (!IsGamepadAvailable(g)) continue;
            any = true;
            std::string ax;
            for (int a = 0; a < GetGamepadAxisCount(g) && a < 6; ++a) ax += TextFormat(" %.2f", GetGamepadAxisMovement(g, a));
            line(TextFormat("#%d %s axes:%s  A[%d%d%d%d]", g, GetGamepadName(g), ax.c_str(), IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN),
                            IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN), IsGamepadButtonReleased(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN), IsGamepadButtonUp(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)));
            if (IsKeyPressed(KEY_V)) SetGamepadVibration(g, 1.f, 1.f, 0.4f);
        }
        if (!any) line("none connected (V: vibrate when present)", th.muted);

        line("TOUCH / GESTURES", th.accent);
        line(TextFormat("points=%d  touch0=%d,%d id=%d", GetTouchPointCount(), GetTouchX(), GetTouchY(), GetTouchPointId(0)));
        const int g = GetGestureDetected();
        if (g != GESTURE_NONE) lastGesture_ = g;
        const Vector2 dv = GetGestureDragVector(), pv = GetGesturePinchVector();
        line(TextFormat("last=%s hold=%.2fs drag=(%.2f,%.2f)@%.0f pinch=(%.2f,%.2f)@%.0f", GestureName(lastGesture_), GetGestureHoldDuration(), dv.x, dv.y,
                        GetGestureDragAngle(), pv.x, pv.y, GetGesturePinchAngle()));
        for (int i = 0; i < GetTouchPointCount(); ++i) DrawCircleV(GetTouchPosition(i), 20, Fade(th.accent, 0.4f));

        line("RANDOM", th.accent);
        if (IsKeyPressed(KEY_R) || randomSeq_.empty()) {
            int* seq = LoadRandomSequence(10, 0, 9);
            randomSeq_.assign(seq, seq + 10);
            UnloadRandomSequence(seq);
        }
        std::string rs = "LoadRandomSequence(10,0,9) [R]:";
        for (int v : randomSeq_) rs += TextFormat(" %d", v);
        line(rs);
        line(TextFormat("GetRandomValue(1,6) = %d", GetRandomValue(1, 6)));

        // right column: hashing + files + clipboard
        float ry = 44;
        auto rline = [&](const std::string& s, Color c = RAYWHITE) {
            UiText(th, s.c_str(), {520, ry}, 14, c);
            ry += 17;
        };
        for (size_t i = 0; i < hashLines_.size(); ++i) rline(hashLines_[i], i == 0 ? th.accent : RAYWHITE);
        rline(TextFormat("SAVE DIR  %s  (%u entries)", ctx.storage->root().c_str(), ctx.storage->count("", nullptr, true)), th.accent);
        for (size_t i = 0; i < files_.size() && i < 9; ++i) {
            const char* f = files_[i].c_str();
            rline(TextFormat("%-28s %6d B", GetFileName(f), IsPathFile(f) ? GetFileLength(f) : 0));
        }
        if (IsKeyPressed(KEY_F)) refreshFiles(ctx);
        if (IsKeyPressed(KEY_I)) {
            Image clip = GetClipboardImage();
            if (IsImageValid(clip)) {
                ImageResize(&clip, 160, 90);
                if (IsTextureValid(clipTex_)) UnloadTexture(clipTex_);
                clipTex_ = LoadTextureFromImage(clip);
                UnloadImage(clip);
            } else {
                ctx.notify("GetClipboardImage: no image (or unsupported platform)");
            }
        }
        if (IsTextureValid(clipTex_)) DrawTexture(clipTex_, 760, 400, WHITE);
        rline("F refresh files, I GetClipboardImage, drop files onto the window", th.muted);
        rline(TextFormat("log: %d warnings, %d errors (F1 console)", LogCount(LOG_WARNING), LogCount(LOG_ERROR)), th.muted);
    }

    // ---------------------------------------------------------------------------------------------------------
    int page_ = 0;
    float t_ = 0.f, autoT_ = 0.f;
    Vector2 spline_[7]{};
    int drag_ = -1, splineType_ = 2;
    float probeR_ = 24.f;
    Image labBase_{}, labImg_{}, canvas_{};
    Texture2D labTexBase_{}, labTex_{}, canvasTex_{}, screenTex_{}, clipTex_{}, textImgTex_{}, textImgTex2_{};
    Vector2 canvasPos_{290, 290};
    int op_ = 0;
    std::vector<Color> palette_;
    Color avg_ = BLANK;
    RenderTexture2D rt_{};
    int* cps_ = nullptr;
    int cpsCount_ = 0;
    int glyphSize_ = 0;
    TextInput input_{48};
    int selSfx_ = 0;
    float sfxPitch_ = 1.f, sfxPan_ = 0.5f, musicPitch_ = 1.f, musicPan_ = 0.5f, humPitch_ = 1.f, humPan_ = 0.5f;
    std::vector<int> keyLog_;
    int spaceReleases_ = 0;
    int lastGesture_ = 0;
    std::vector<int> randomSeq_;
    std::vector<std::string> files_, hashLines_;
};

}  // namespace

std::unique_ptr<Scene> MakeShowcaseScene() { return std::make_unique<ShowcaseScene>(); }

}  // namespace rd
