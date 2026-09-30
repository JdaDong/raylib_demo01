#include "core/rng.hpp"
#include "core/spatial_grid.hpp"
#include "testing.hpp"

#include <algorithm>

using namespace rd;

TEST(spatial_grid, dimensions) {
    SpatialGrid g(1000, 500, 100);
    CHECK_EQ(g.cols(), 10);
    CHECK_EQ(g.rows(), 5);
    SpatialGrid tiny(10, 10, 0.0f);  // cell size clamped to >= 1
    CHECK(tiny.cellSize() >= 1.0f);
}

TEST(spatial_grid, query_finds_overlapping_and_dedups) {
    SpatialGrid g(1000, 1000, 100);
    g.insert(1, {50, 50, 10, 10});
    g.insert(2, {90, 90, 40, 40});  // spans 4 cells
    g.insert(3, {800, 800, 10, 10});
    std::vector<int> out;
    g.query({0, 0, 200, 200}, out);
    CHECK_EQ(out.size(), static_cast<size_t>(2));
    CHECK_EQ(out[0], 1);
    CHECK_EQ(out[1], 2);  // no duplicates though 2 lives in 4 cells
    g.query({795, 795, 10, 10}, out);  // straddles cells 7 and 8
    CHECK_EQ(out.size(), static_cast<size_t>(1));
    g.query({790, 790, 5, 5}, out);    // cell 7 only: item lives in cell 8
    CHECK(out.empty());
    g.query({805, 805, 1, 1}, out);
    CHECK_EQ(out.size(), static_cast<size_t>(1));
    CHECK_EQ(g.itemCount(), static_cast<size_t>(3));
    g.clear();
    g.query({0, 0, 1000, 1000}, out);
    CHECK(out.empty());
}

TEST(spatial_grid, out_of_bounds_is_clamped) {
    SpatialGrid g(100, 100, 50);
    g.insert(7, {-500, -500, 10, 10});
    std::vector<int> out;
    g.query({-1000, -1000, 5, 5}, out);
    CHECK_EQ(out.size(), static_cast<size_t>(1));
}

TEST(spatial_grid, matches_bruteforce) {
    Rng r(99);
    SpatialGrid g(2000, 2000, 64);
    std::vector<Rectangle> rects;
    for (int i = 0; i < 300; ++i) {
        Rectangle rc{r.range(0, 1950), r.range(0, 1950), r.range(2, 50), r.range(2, 50)};
        rects.push_back(rc);
        g.insert(i, rc);
    }
    std::vector<int> out;
    for (int q = 0; q < 50; ++q) {
        Rectangle qr{r.range(0, 1800), r.range(0, 1800), r.range(10, 200), r.range(10, 200)};
        g.query(qr, out);
        for (int i = 0; i < 300; ++i) {
            if (CheckCollisionRecs(qr, rects[static_cast<size_t>(i)])) {
                // broadphase must never miss a true overlap (false positives allowed)
                CHECK(std::binary_search(out.begin(), out.end(), i));
            }
        }
    }
}
