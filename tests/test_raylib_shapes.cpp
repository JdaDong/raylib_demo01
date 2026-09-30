// raylib_shapes: collision + spline math used by World and the Showcase page
#include "testing.hpp"

#include "raylib.h"
#include "raymath.h"

TEST(raylib_shapes, rect_and_circle_collisions) {
    const Rectangle a{0, 0, 10, 10}, b{5, 5, 10, 10}, c{20, 20, 5, 5};
    CHECK(CheckCollisionRecs(a, b));
    CHECK(!CheckCollisionRecs(a, c));
    const Rectangle o = GetCollisionRec(a, b);
    CHECK_NEAR(o.x, 5.0, 1e-6);
    CHECK_NEAR(o.width, 5.0, 1e-6);
    const Rectangle none = GetCollisionRec(a, c);
    CHECK_NEAR(none.width, 0.0, 1e-6);
    CHECK(CheckCollisionCircles({0, 0}, 5, {9, 0}, 5));
    CHECK(!CheckCollisionCircles({0, 0}, 5, {11, 0}, 5));
    CHECK(CheckCollisionCircleRec({15, 5}, 6, a));
    CHECK(!CheckCollisionCircleRec({17, 5}, 6, a));
}

TEST(raylib_shapes, point_tests) {
    CHECK(CheckCollisionPointRec({5, 5}, {0, 0, 10, 10}));
    CHECK(!CheckCollisionPointRec({10.5f, 5}, {0, 0, 10, 10}));
    CHECK(CheckCollisionPointCircle({3, 4}, {0, 0}, 5.01f));
    CHECK(CheckCollisionPointTriangle({1, 1}, {0, 0}, {10, 0}, {0, 10}));
    CHECK(!CheckCollisionPointTriangle({9, 9}, {0, 0}, {10, 0}, {0, 10}));
    CHECK(CheckCollisionPointLine({5, 1}, {0, 0}, {10, 0}, 2));
    CHECK(!CheckCollisionPointLine({5, 5}, {0, 0}, {10, 0}, 2));
    const Vector2 poly[5] = {{0, 0}, {10, 0}, {10, 10}, {5, 15}, {0, 10}};
    CHECK(CheckCollisionPointPoly({5, 12}, poly, 5));
    CHECK(!CheckCollisionPointPoly({1, 14}, poly, 5));
}

TEST(raylib_shapes, line_intersections) {
    Vector2 hit{};
    CHECK(CheckCollisionLines({0, 0}, {10, 10}, {0, 10}, {10, 0}, &hit));
    CHECK_NEAR(hit.x, 5.0, 1e-4);
    CHECK_NEAR(hit.y, 5.0, 1e-4);
    CHECK(!CheckCollisionLines({0, 0}, {10, 0}, {0, 5}, {10, 5}, &hit));  // parallel
    CHECK(CheckCollisionCircleLine({5, 2}, 3, {0, 0}, {10, 0}));
    CHECK(!CheckCollisionCircleLine({5, 5}, 3, {0, 0}, {10, 0}));
}

TEST(raylib_shapes, spline_endpoints) {
    const Vector2 p1{0, 0}, p2{10, 0}, p3{20, 10}, p4{30, 10};
    const Vector2 l0 = GetSplinePointLinear(p1, p2, 0.f), l1 = GetSplinePointLinear(p1, p2, 1.f);
    CHECK_NEAR(l0.x, 0.0, 1e-5);
    CHECK_NEAR(l1.x, 10.0, 1e-5);
    // Catmull-Rom passes through p2 (t=0) and p3 (t=1)
    const Vector2 c0 = GetSplinePointCatmullRom(p1, p2, p3, p4, 0.f);
    const Vector2 c1 = GetSplinePointCatmullRom(p1, p2, p3, p4, 1.f);
    CHECK_NEAR(c0.x, p2.x, 1e-4);
    CHECK_NEAR(c1.y, p3.y, 1e-4);
    // Bezier curves hit their end points
    const Vector2 q = GetSplinePointBezierQuad(p1, p2, p3, 1.f);
    CHECK_NEAR(q.x, p3.x, 1e-4);
    const Vector2 cb = GetSplinePointBezierCubic(p1, p2, p3, p4, 0.f);
    CHECK_NEAR(cb.x, p1.x, 1e-4);
    // B-spline (basis) does NOT interpolate control points but stays in the hull
    const Vector2 bs = GetSplinePointBasis(p1, p2, p3, p4, 0.5f);
    CHECK(bs.x > p1.x && bs.x < p4.x);
}

TEST(raylib_shapes, raymath_basics) {
    CHECK_NEAR(Vector2Length({3, 4}), 5.0, 1e-6);
    CHECK_NEAR(Vector2Angle({1, 0}, {0, 1}), PI / 2, 1e-5);
    const Vector2 r = Vector2Rotate({1, 0}, PI / 2);
    CHECK_NEAR(r.y, 1.0, 1e-5);
    CHECK_NEAR(Vector2ClampValue({10, 0}, 0, 2).x, 2.0, 1e-6);
    CHECK_NEAR(Lerp(0, 10, 0.3f), 3.0, 1e-5);
    CHECK_NEAR(Remap(5, 0, 10, 100, 200), 150.0, 1e-4);
    CHECK_NEAR(Wrap(370, 0, 360), 10.0, 1e-3);
    CHECK_EQ(FloatEquals(0.1f + 0.2f, 0.3f), 1);
    const Vector2 refl = Vector2Reflect({1, -1}, {0, 1});
    CHECK_NEAR(refl.y, 1.0, 1e-6);
}
