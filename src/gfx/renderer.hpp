// renderer.hpp - draws World state (world space) and the HUD (screen space)
#pragma once
#include "app/context.hpp"
#include "game/particles.hpp"
#include "game/world.hpp"

namespace rd {

struct Explosion {
    Vector2 pos;
    float t;      // 0..1
    float scale;
};

class Renderer {
public:
    void init(int minimapW, int minimapH);
    void shutdown();

    void addExplosion(Vector2 pos, float scale) { explosions_.push_back({pos, 0.f, scale}); }
    void update(float dt);

    void drawWorld(const AppContext& ctx, const World& w, const ParticleSystem& ps, const Camera2D& cam, Vector2 aimWorld);
    void drawHud(const AppContext& ctx, const World& w, float waveBannerT, bool showCrosshair, Vector2 aimScreen);
    void renderMinimap(const World& w, const Camera2D& cam, Vector2 viewSize);  // BeginTextureMode (call outside other targets)

private:
    RenderTexture2D minimap_{};
    std::vector<Explosion> explosions_;
    std::vector<Vector2> trail_;
    float trailTimer_ = 0.f;
};

}  // namespace rd
