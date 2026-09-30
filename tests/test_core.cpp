// core: rng, easing, timers, dyn_array, math2d, camera rig
#include "core/camera_rig.hpp"
#include "core/dyn_array.hpp"
#include "core/easing.hpp"
#include "core/math2d.hpp"
#include "core/rng.hpp"
#include "core/timer.hpp"
#include "testing.hpp"

#include <set>

using namespace rd;

TEST(core, rng_is_deterministic_per_seed) {
    Rng a(42), b(42), c(43);
    bool anyDiff = false;
    for (int i = 0; i < 100; ++i) {
        const auto va = a.nextU64();
        CHECK_EQ(va, b.nextU64());
        if (va != c.nextU64()) anyDiff = true;
    }
    CHECK(anyDiff);
}

TEST(core, rng_ranges_are_respected) {
    Rng r(7);
    std::set<int> seen;
    for (int i = 0; i < 2000; ++i) {
        const float f = r.next01();
        CHECK(f >= 0.f && f < 1.f);
        const float g = r.range(-3.f, 5.f);
        CHECK(g >= -3.f && g < 5.f);
        const int k = r.rangeInt(2, 6);
        CHECK(k >= 2 && k <= 6);
        seen.insert(k);
    }
    CHECK_EQ(seen.size(), static_cast<size_t>(5));  // all values reachable
    CHECK_EQ(r.rangeInt(9, 9), 9);
    const int swapped = r.rangeInt(5, 1);
    CHECK(swapped >= 1 && swapped <= 5);
}

TEST(core, rng_zero_seed_is_remapped) {
    Rng r(0);
    CHECK(r.state() != 0u);
}

TEST(core, rng_unit_circle) {
    Rng r(3);
    for (int i = 0; i < 200; ++i) {
        CHECK_NEAR(Vector2Length(r.onUnitCircle()), 1.0, 1e-4);
        CHECK(Vector2Length(r.insideUnitCircle()) <= 1.0001f);
    }
}

TEST(core, easing_endpoints) {
    float (*fns[])(float) = {ease::linear, ease::quadIn, ease::quadOut, ease::quadInOut, ease::cubicOut,
                             ease::backOut, ease::elasticOut, ease::bounceOut};
    for (auto f : fns) {
        CHECK_NEAR(f(0.f), 0.0, 1e-4);
        CHECK_NEAR(f(1.f), 1.0, 1e-4);
        CHECK_NEAR(f(-5.f), f(0.f), 1e-6);  // clamped
        CHECK_NEAR(f(5.f), f(1.f), 1e-6);
    }
    CHECK(ease::backOut(0.7f) > 1.0f);  // overshoot
    CHECK_NEAR(ease::pingPong(0.5f), 1.0, 1e-6);
    CHECK_NEAR(ease::pingPong(1.0f), 0.0, 1e-6);
}

TEST(core, cooldown_lifecycle) {
    Cooldown c(0.5f);
    CHECK(c.ready());
    CHECK(c.tryTrigger());
    CHECK(!c.ready());
    CHECK(!c.tryTrigger());
    CHECK_NEAR(c.progress(), 0.0, 1e-6);
    c.update(0.25f);
    CHECK_NEAR(c.progress(), 0.5, 1e-5);
    c.update(1.0f);  // overshoot clamps at 0
    CHECK(c.ready());
    CHECK_NEAR(c.remaining, 0.0, 1e-6);
}

TEST(core, fixed_step_accumulates) {
    FixedStep fs(0.01f, 8);
    CHECK_EQ(fs.advance(0.005f), 0);
    CHECK_EQ(fs.advance(0.006f), 1);
    CHECK_NEAR(fs.alpha(), 0.1, 1e-3);
    CHECK_EQ(fs.advance(0.03f), 3);
}

TEST(core, fixed_step_spiral_of_death_guard) {
    FixedStep fs(0.01f, 4);
    CHECK_EQ(fs.advance(1.0f), 4);  // capped
    CHECK(fs.dropped() > 0);
    CHECK(fs.alpha() < 1.0f);
    CHECK_EQ(fs.advance(-1.f), 0);  // negative / NaN rejected
    CHECK_EQ(fs.advance(NAN), 0);
}

TEST(core, dyn_array_push_remove_grow) {
    DynArray<int> a;
    for (int i = 0; i < 100; ++i) a.push_back(i);
    CHECK_EQ(a.size(), static_cast<size_t>(100));
    CHECK(a.capacity() >= 100);
    a.removeSwap(0);  // last element moves to 0
    CHECK_EQ(a[0], 99);
    CHECK_EQ(a.size(), static_cast<size_t>(99));
    a.removeSwap(1000);  // out of range: no-op
    CHECK_EQ(a.size(), static_cast<size_t>(99));
    long sum = 0;
    for (int v : a) sum += v;
    CHECK_EQ(sum, 4950L);
    DynArray<int> b(std::move(a));
    CHECK_EQ(b.size(), static_cast<size_t>(99));
    CHECK_EQ(a.size(), static_cast<size_t>(0));
    b.clear();
    CHECK(b.empty());
}

TEST(core, deadzone) {
    CHECK_NEAR(Vector2Length(ApplyDeadzone({0.1f, 0.f}, 0.2f)), 0.0, 1e-6);
    CHECK_NEAR(Vector2Length(ApplyDeadzone({1.f, 0.f}, 0.2f)), 1.0, 1e-5);
    const Vector2 mid = ApplyDeadzone({0.6f, 0.f}, 0.2f);
    CHECK_NEAR(mid.x, 0.5, 1e-5);
    const Vector2 over = ApplyDeadzone({3.f, 4.f}, 0.2f);
    CHECK_NEAR(Vector2Length(over), 1.0, 1e-5);
    CHECK_NEAR(over.x / over.y, 0.75, 1e-4);  // direction kept
}

TEST(core, resolve_circle_rect_pushes_out) {
    Rectangle r{0, 0, 100, 100};
    Vector2 c{105, 50};
    Vector2 n{};
    CHECK(ResolveCircleRect(c, 10, r, &n));
    CHECK_NEAR(c.x, 110.0, 1e-4);
    CHECK_NEAR(n.x, 1.0, 1e-5);
    Vector2 far{200, 200};
    CHECK(!ResolveCircleRect(far, 10, r));
    // center inside the rect: push along the smallest penetration
    Vector2 inside{5, 50};
    CHECK(ResolveCircleRect(inside, 10, r, &n));
    CHECK_NEAR(inside.x, -10.0, 1e-4);
    CHECK_NEAR(n.x, -1.0, 1e-5);
    CHECK(!CheckCollisionCircleRec(inside, 9.9f, r));
}

TEST(core, letterbox_preserves_aspect) {
    const Rectangle wide = Letterbox(960, 540, 1920, 1200);
    CHECK_NEAR(wide.width / wide.height, 960.0 / 540.0, 1e-4);
    CHECK_NEAR(wide.x, 0.0, 1e-4);
    CHECK(wide.y > 0);
    const Rectangle tall = Letterbox(960, 540, 1000, 1000);
    CHECK_NEAR(tall.width, 1000.0, 1e-3);
    CHECK_NEAR(tall.y, (1000 - 562.5) / 2, 1e-2);
}

TEST(core, exp_decay_is_framerate_independent) {
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 60; ++i) a = ExpDecay(a, 10.f, 5.f, 1.f / 60.f);
    for (int i = 0; i < 240; ++i) b = ExpDecay(b, 10.f, 5.f, 1.f / 240.f);
    CHECK_NEAR(a, b, 1e-3);
}

TEST(core, saturation_matrix) {
    const Matrix id = SaturationMatrix(1.0f);
    const Matrix ref = MatrixIdentity();
    CHECK_NEAR(id.m0, ref.m0, 1e-6);
    CHECK_NEAR(id.m5, ref.m5, 1e-6);
    CHECK_NEAR(id.m4, 0.0, 1e-6);
    const Matrix gray = SaturationMatrix(0.0f);
    // every output channel equals luminance -> rows equal
    CHECK_NEAR(gray.m0, gray.m1, 1e-6);
    CHECK_NEAR(gray.m4, gray.m6, 1e-6);
    CHECK_NEAR(gray.m0 + gray.m4 + gray.m8, 1.0, 1e-4);
}

TEST(core, camera_rig_clamps_to_world) {
    CameraRig rig;
    const Rectangle world{0, 0, 1920, 1080};
    const Vector2 view{960, 540};
    rig.snap({0, 0}, world, view);
    CHECK_NEAR(rig.cam.target.x, 480.0, 1e-3);
    CHECK_NEAR(rig.cam.target.y, 270.0, 1e-3);
    const Rectangle vis = rig.visibleWorld(view);
    CHECK_NEAR(vis.x, 0.0, 1e-3);
    rig.snap({5000, 5000}, world, view);
    CHECK_NEAR(rig.cam.target.x, 1440.0, 1e-3);
    // world smaller than the view -> centered
    rig.zoomTarget = 0.4f;
    rig.snap({0, 0}, world, view);
    CHECK_NEAR(rig.cam.target.x, 960.0, 1e-3);
}

TEST(core, camera_world_screen_roundtrip) {
    // raylib GetWorldToScreen2D / GetScreenToWorld2D / GetCameraMatrix2D are pure math
    Camera2D cam{{480, 270}, {700, 300}, 15.0f, 1.5f};
    const Vector2 p{812.5f, 433.25f};
    const Vector2 s = GetWorldToScreen2D(p, cam);
    const Vector2 back = GetScreenToWorld2D(s, cam);
    CHECK_NEAR(back.x, p.x, 1e-2);
    CHECK_NEAR(back.y, p.y, 1e-2);
    const Vector2 t = GetWorldToScreen2D(cam.target, cam);
    CHECK_NEAR(t.x, cam.offset.x, 1e-3);  // target maps onto offset
    const Matrix m = GetCameraMatrix2D(cam);
    const Vector2 viaMatrix = Vector2Transform(p, m);
    CHECK_NEAR(viaMatrix.x, s.x, 1e-2);
    CHECK_NEAR(viaMatrix.y, s.y, 1e-2);
}

TEST(core, shake_decays) {
    Shake s;
    s.add(0.5f);
    s.add(0.8f);
    CHECK_NEAR(s.trauma, 1.0, 1e-6);  // clamped
    CHECK_NEAR(s.intensity(), 1.0, 1e-6);
    Rng r(1);
    const Vector2 o = s.offset(r);
    CHECK(std::fabs(o.x) <= s.maxOffset && std::fabs(o.y) <= s.maxOffset);
    for (int i = 0; i < 200; ++i) s.update(0.01f);
    CHECK_NEAR(s.trauma, 0.0, 1e-6);
    CHECK_NEAR(Vector2Length(s.offset(r)), 0.0, 1e-6);
}
