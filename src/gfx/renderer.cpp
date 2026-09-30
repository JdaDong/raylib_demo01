#include "gfx/renderer.hpp"

#include "core/easing.hpp"
#include "core/math2d.hpp"
#include "ui/ui_logic.hpp"

#include <algorithm>
#include <cmath>

namespace rd {

namespace {
const Color kEnemyColors[kEnemyKindCount] = {{255, 77, 109, 255}, {46, 230, 214, 255}, {255, 176, 32, 255}, {138, 92, 255, 255}};

void DrawSprite(Texture2D tex, Vector2 pos, float size, float rotationDeg, Color tint) {
    const Rectangle src{0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    const Rectangle dst{pos.x, pos.y, size, size};
    DrawTexturePro(tex, src, dst, {size * 0.5f, size * 0.5f}, rotationDeg, tint);
}
}  // namespace

void Renderer::init(int w, int h) {
    minimap_ = LoadRenderTexture(w, h);
    explosions_.clear();
    trail_.clear();
}

void Renderer::shutdown() {
    if (IsRenderTextureValid(minimap_)) UnloadRenderTexture(minimap_);
    minimap_ = {};
}

void Renderer::update(float dt) {
    for (auto& e : explosions_) e.t += dt * 1.8f;
    explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(), [](const Explosion& e) { return e.t >= 1.f; }), explosions_.end());
    trailTimer_ += dt;
}

void Renderer::drawWorld(const AppContext& ctx, const World& w, const ParticleSystem& ps, const Camera2D& cam, Vector2 aimWorld) {
    const Assets& a = *ctx.assets;
    const Rectangle bounds = w.bounds();
    const Player& p = w.player();

    // parallax nebula in screen space (tiled via TEXTURE_WRAP_REPEAT + oversized source)
    DrawTexturePro(a.bg, {cam.target.x * 0.25f, cam.target.y * 0.25f, static_cast<float>(ctx.vw), static_cast<float>(ctx.vh)},
                   {0, 0, static_cast<float>(ctx.vw), static_cast<float>(ctx.vh)}, {0, 0}, 0.f, WHITE);

    BeginMode2D(cam);
    // floor + grid
    DrawTexturePro(a.floorTex, {0, 0, bounds.width * 0.5f, bounds.height * 0.5f}, bounds, {0, 0}, 0.f, Fade(WHITE, 0.85f));
    for (float x = 0; x <= bounds.width; x += 120) DrawLine(static_cast<int>(x), 0, static_cast<int>(x), static_cast<int>(bounds.height), Fade(SKYBLUE, 0.06f));
    for (float y = 0; y <= bounds.height; y += 120) DrawLineV({0, y}, {bounds.width, y}, Fade(SKYBLUE, 0.06f));
    DrawRectangleLinesEx(bounds, 4.f, Fade(ctx.theme.accent, 0.8f));

    // walls
    for (const Rectangle& r : w.walls()) {
        DrawRectangleRec({r.x + 6, r.y + 8, r.width, r.height}, Fade(BLACK, 0.35f));  // drop shadow
        DrawRectangleGradientEx(r, GetColor(0x2A3358FF), GetColor(0x151A30FF), GetColor(0x0E1224FF), GetColor(0x3A4578FF));
        DrawRectangleLinesEx(r, 2.f, Fade(ctx.theme.accent, 0.6f));
    }

    // pickups
    for (const Pickup& pk : w.pickups()) {
        if (pk.life < 2.f && std::fmod(pk.life, 0.2f) < 0.1f) continue;  // blink before expiring
        const float pulse = 1.f + 0.15f * ease::pingPong(std::fmod(ctx.time * 2.f, 1.f));
        if (pk.kind == PickupKind::Gem) DrawSprite(a.gem, pk.pos, 16.f * pulse, ctx.time * 120.f, WHITE);
        else DrawSprite(a.heal, pk.pos, 22.f * pulse, 0.f, WHITE);
    }

    // weaver paths (debug-ish flair)
    for (const Enemy& e : w.enemies()) {
        if (e.kind != EnemyKind::Weaver) continue;
        DrawSplineCatmullRom(e.path, 4, 1.5f, Fade(kEnemyColors[1], 0.18f));
        DrawSplineSegmentCatmullRom(e.path[0], e.path[1], e.path[2], e.path[3], 2.5f, Fade(kEnemyColors[1], 0.3f));
    }

    // enemies
    for (const Enemy& e : w.enemies()) {
        const int k = static_cast<int>(e.kind);
        const float size = e.radius * 2.f * (64.f / (2.f * std::max(1.f, a.enemyRadius[k])));
        const float rot = e.kind == EnemyKind::Tank ? DirToAngleDeg(Vector2Subtract(p.pos, e.pos)) : e.age * 90.f;
        DrawEllipse(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y + e.radius * 0.9f), e.radius, e.radius * 0.35f, Fade(BLACK, 0.3f));
        DrawSprite(a.enemy[k], e.pos, size, rot, WHITE);
        if (e.hitFlash > 0.f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSprite(a.enemy[k], e.pos, size, rot, Fade(WHITE, e.hitFlash));
            EndBlendMode();
        }
        if (e.hp < e.maxHp) {
            const Rectangle bar{e.pos.x - e.radius, e.pos.y - e.radius - 10, e.radius * 2, 4};
            DrawRectangleRounded(bar, 1.f, 4, Fade(BLACK, 0.6f));
            DrawRectangleRounded({bar.x, bar.y, bar.width * std::max(0.f, e.hp / e.maxHp), bar.height}, 1.f, 4, kEnemyColors[k]);
        }
        if (e.kind == EnemyKind::Tank && e.gun.progress() > 0.8f)  // charging telegraph
            DrawCircleSectorLines(e.pos, e.radius + 6, 0, 360.f * e.gun.progress(), 24, Fade(ORANGE, 0.7f));
    }

    // player trail + shadow + ship
    if (trailTimer_ > 0.02f) {
        trailTimer_ = 0.f;
        auto& tr = const_cast<std::vector<Vector2>&>(trail_);
        tr.push_back(p.pos);
        if (tr.size() > 24) tr.erase(tr.begin());
    }
    if (trail_.size() >= 4) {
        DrawSplineBasis(trail_.data(), static_cast<int>(trail_.size()), p.dashTime > 0 ? 6.f : 2.f, Fade(SKYBLUE, 0.35f));
        DrawLineStrip(trail_.data(), static_cast<int>(trail_.size()), Fade(WHITE, 0.08f));
    }
    if (p.alive) {
        DrawEllipseV({p.pos.x, p.pos.y + p.radius}, p.radius, p.radius * 0.35f, Fade(BLACK, 0.35f));
        DrawCircleLinesV(p.pos, w.config().magnetRadius, Fade(GREEN, 0.05f));
        DrawLineDashed(p.pos, aimWorld, 6, 8, Fade(WHITE, 0.22f));
        const bool blink = p.invuln > 0.f && std::fmod(ctx.time, 0.12f) < 0.06f;
        if (!blink) DrawSprite(a.ship, p.pos, 36.f, DirToAngleDeg(p.aimDir), WHITE);
        if (p.invuln > 0.f) {
            DrawRing(p.pos, p.radius + 6, p.radius + 9, 0, 360, 36, Fade(SKYBLUE, 0.45f));
            DrawRingLines(p.pos, p.radius + 5, p.radius + 10, ctx.time * 180.f, ctx.time * 180.f + 90.f, 12, Fade(WHITE, 0.6f));
        }
    }

    // bullets (additive glow)
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Bullet& b : w.bullets()) {
        if (b.hostile) {
            DrawSprite(a.glow, b.pos, 34.f, 0.f, Fade(ORANGE, 0.55f));
        } else {
            DrawSprite(a.glow, b.pos, 22.f, 0.f, Fade(SKYBLUE, 0.6f));
            DrawLineEx(b.prevPos, b.pos, 3.f, Fade(WHITE, 0.6f));
        }
    }
    for (const Particle& pt : ps.items()) {
        const float s = ParticleSystem::CurrentSize(pt) * 2.f;
        if (s <= 0.2f) continue;
        DrawSprite(a.particle, pt.pos, s, pt.angle, ParticleSystem::CurrentColor(pt));
    }
    // explosions: sprite-sheet frame + expanding shock ring
    for (const Explosion& ex : explosions_) {
        const int frame = std::min(a.explosionFrames - 1, static_cast<int>(ex.t * a.explosionFrames));
        const float fs = 64.f;
        const Rectangle src{frame * fs, 0, fs, fs};
        const float size = fs * ex.scale * (0.8f + ex.t);
        DrawTexturePro(a.explosion, src, {ex.pos.x, ex.pos.y, size, size}, {size * 0.5f, size * 0.5f}, 0.f, WHITE);
        DrawCircleLinesV(ex.pos, 10.f + ex.t * 70.f * ex.scale, Fade(GOLD, 1.f - ex.t));
    }
    EndBlendMode();
    for (const Bullet& b : w.bullets()) {
        if (b.hostile) {
            DrawPoly(b.pos, 6, b.radius, ctx.time * 360.f, ORANGE);
            DrawPolyLinesEx(b.pos, 6, b.radius + 2, ctx.time * 360.f, 1.5f, YELLOW);
        } else {
            DrawCircleV(b.pos, b.radius * 0.7f, WHITE);
        }
    }
    EndMode2D();
}

void Renderer::renderMinimap(const World& w, const Camera2D& cam, Vector2 viewSize) {
    if (!IsRenderTextureValid(minimap_)) return;
    const float sx = static_cast<float>(minimap_.texture.width) / w.bounds().width;
    const float sy = static_cast<float>(minimap_.texture.height) / w.bounds().height;
    auto map = [&](Vector2 v) { return Vector2{v.x * sx, v.y * sy}; };
    BeginTextureMode(minimap_);
    ClearBackground(Fade(BLACK, 0.55f));
    for (const Rectangle& r : w.walls()) DrawRectangleRec({r.x * sx, r.y * sy, r.width * sx, r.height * sy}, GetColor(0x5A6AA0FF));
    for (const Pickup& pk : w.pickups()) DrawPixelV(map(pk.pos), GREEN);
    for (const Enemy& e : w.enemies()) DrawCircleV(map(e.pos), std::max(1.5f, e.radius * sx), kEnemyColors[static_cast<int>(e.kind)]);
    const Player& p = w.player();
    const Vector2 c = map(p.pos);
    const Vector2 f = Vector2Scale(p.aimDir, 6.f), l = Vector2Scale(Vector2Rotate(p.aimDir, 2.5f), 4.f), r = Vector2Scale(Vector2Rotate(p.aimDir, -2.5f), 4.f);
    DrawTriangle(Vector2Add(c, f), Vector2Add(c, r), Vector2Add(c, l), RAYWHITE);  // CCW for raylib
    DrawTriangleLines(Vector2Add(c, f), Vector2Add(c, l), Vector2Add(c, r), SKYBLUE);
    const Vector2 tl = GetScreenToWorld2D({0, 0}, cam), br = GetScreenToWorld2D(viewSize, cam);
    DrawRectangleLinesEx({tl.x * sx, tl.y * sy, (br.x - tl.x) * sx, (br.y - tl.y) * sy}, 1.f, Fade(WHITE, 0.7f));
    EndTextureMode();
}

void Renderer::drawHud(const AppContext& ctx, const World& w, float waveBannerT, bool showCrosshair, Vector2 aimScreen) {
    const UiTheme& t = ctx.theme;
    const Assets& a = *ctx.assets;
    const Player& p = w.player();

    // HP bar
    const Rectangle hp{16, 16, 220, 16};
    DrawRectangleRec(hp, Fade(BLACK, 0.5f));
    const float frac = std::max(0.f, p.hp / p.maxHp);
    DrawRectangleGradientH(static_cast<int>(hp.x), static_cast<int>(hp.y), static_cast<int>(hp.width * frac), static_cast<int>(hp.height),
                           GetColor(0xFF3B5CFF), ColorLerp(GetColor(0xFF3B5CFF), GetColor(0x7CFF6BFF), frac));
    DrawRectangleLines(static_cast<int>(hp.x), static_cast<int>(hp.y), static_cast<int>(hp.width), static_cast<int>(hp.height), Fade(WHITE, 0.6f));
    UiText(t, TextFormat("HP %d", static_cast<int>(p.hp)), {hp.x + 6, hp.y - 1}, 16, RAYWHITE);

    // dash cooldown dial
    const Vector2 dial{260, 24};
    DrawCircleV(dial, 13, Fade(BLACK, 0.5f));
    DrawCircleSector(dial, 12, -90, -90 + 360.f * p.dash.progress(), 32, p.dash.ready() ? t.accent : Fade(t.accent, 0.5f));
    DrawCircleSectorLines(dial, 13, 0, 360, 32, Fade(WHITE, 0.5f));
    DrawText("DASH", static_cast<int>(dial.x) + 18, static_cast<int>(dial.y) - 5, 10, t.muted);

    // score + combo
    const std::string score = FormatScore(w.score());
    const Vector2 sm = MeasureTextEx(a.title, score.c_str(), 34, 1);
    DrawTextEx(a.title, score.c_str(), {ctx.vw * 0.5f - sm.x * 0.5f, 10}, 34, 1, RAYWHITE);
    if (w.combo() > 1) {
        const char* combo = TextFormat("x%.1f  COMBO %d", w.multiplier(), w.combo());
        const float pulse = 1.f + 0.1f * std::sin(ctx.time * 12.f);
        const Vector2 cm = MeasureTextEx(a.ui, combo, 20 * pulse, 1);
        DrawTextPro(a.ui, combo, {ctx.vw * 0.5f, 60}, {cm.x * 0.5f, cm.y * 0.5f}, std::sin(ctx.time * 6.f) * 3.f, 20 * pulse, 1, GOLD);
        const float barW = 120.f * (w.comboTimer() / w.config().comboWindow);
        DrawRectangle(static_cast<int>(ctx.vw * 0.5f - 60), 72, static_cast<int>(barW), 3, Fade(GOLD, 0.7f));
    }
    DrawTextEx(a.atlas, TextFormat("WAVE %d  KILLS %d  %s", w.wave(), w.kills(), FormatClock(w.elapsed()).c_str()), {16, 40}, 20, 1, t.muted);

    // wave banner (backOut ease-in, fade-out)
    if (waveBannerT > 0.f && waveBannerT < 1.f) {
        const float in = ease::backOut(std::min(1.f, waveBannerT * 4.f));
        const float alpha = 1.f - ease::quadIn(std::max(0.f, (waveBannerT - 0.6f) / 0.4f));
        const char* text = TextFormat("WAVE %d", w.wave());
        const float size = 64.f * in;
        const Vector2 m = MeasureTextEx(a.title, text, size, 2);
        DrawTextPro(a.title, text, {ctx.vw * 0.5f, ctx.vh * 0.38f}, {m.x * 0.5f, m.y * 0.5f}, 0.f, size, 2, Fade(t.accent, alpha));
    }

    // minimap (render texture drawn flipped)
    if (IsRenderTextureValid(minimap_)) {
        const float mw = static_cast<float>(minimap_.texture.width), mh = static_cast<float>(minimap_.texture.height);
        const Vector2 pos{ctx.vw - mw - 12, ctx.vh - mh - 12};
        DrawTextureRec(minimap_.texture, {0, 0, mw, -mh}, pos, WHITE);
        DrawRectangleLinesEx({pos.x - 1, pos.y - 1, mw + 2, mh + 2}, 1.f, Fade(t.accent, 0.7f));
    }

    // VU meter from the audio mixed processor
    if (ctx.audio && ctx.audio->ready()) {
        const float vu = std::min(1.f, ctx.audio->vuLevel() * 3.f);
        const int bars = 10;
        for (int i = 0; i < bars; ++i) {
            const bool lit = static_cast<float>(i) / bars < vu;
            DrawRectangleGradientV(12 + i * 6, ctx.vh - 20, 4, 8, lit ? LIME : Fade(DARKGRAY, 0.5f), lit ? (i > 7 ? RED : GREEN) : Fade(DARKGRAY, 0.3f));
        }
    }

    // crosshair
    if (showCrosshair && IsCursorOnScreen() && p.alive) {
        DrawCircleLinesV(aimScreen, 9, Fade(WHITE, 0.8f));
        DrawLineEx({aimScreen.x - 14, aimScreen.y}, {aimScreen.x - 5, aimScreen.y}, 2, WHITE);
        DrawLineEx({aimScreen.x + 5, aimScreen.y}, {aimScreen.x + 14, aimScreen.y}, 2, WHITE);
        DrawLineEx({aimScreen.x, aimScreen.y - 14}, {aimScreen.x, aimScreen.y - 5}, 2, WHITE);
        DrawLineEx({aimScreen.x, aimScreen.y + 5}, {aimScreen.x, aimScreen.y + 14}, 2, WHITE);
        DrawPixelV(aimScreen, RED);
    }
}

}  // namespace rd
