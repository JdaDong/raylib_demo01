// spatial_grid.hpp - uniform grid broadphase for circle/AABB queries
#pragma once
#include "raylib.h"

#include <vector>

namespace rd {

class SpatialGrid {
public:
    SpatialGrid(float worldW = 1920.f, float worldH = 1080.f, float cellSize = 96.f);

    void resize(float worldW, float worldH, float cellSize);
    void clear();
    void insert(int id, Rectangle aabb);
    // Unique ids whose cells overlap aabb, sorted ascending.
    void query(Rectangle aabb, std::vector<int>& out) const;

    int cols() const { return cols_; }
    int rows() const { return rows_; }
    float cellSize() const { return cell_; }
    size_t itemCount() const { return items_; }

private:
    void cellRange(Rectangle r, int& x0, int& y0, int& x1, int& y1) const;
    float cell_ = 96.f;
    int cols_ = 1, rows_ = 1;
    size_t items_ = 0;
    std::vector<std::vector<int>> cells_;
};

}  // namespace rd
