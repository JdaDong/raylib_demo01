#include "core/spatial_grid.hpp"

#include <algorithm>
#include <cmath>

namespace rd {

SpatialGrid::SpatialGrid(float worldW, float worldH, float cellSize) { resize(worldW, worldH, cellSize); }

void SpatialGrid::resize(float worldW, float worldH, float cellSize) {
    cell_ = cellSize > 1.f ? cellSize : 1.f;
    cols_ = std::max(1, static_cast<int>(std::ceil(worldW / cell_)));
    rows_ = std::max(1, static_cast<int>(std::ceil(worldH / cell_)));
    cells_.assign(static_cast<size_t>(cols_ * rows_), {});
    items_ = 0;
}

void SpatialGrid::clear() {
    for (auto& c : cells_) c.clear();
    items_ = 0;
}

void SpatialGrid::cellRange(Rectangle r, int& x0, int& y0, int& x1, int& y1) const {
    x0 = static_cast<int>(std::floor(r.x / cell_));
    y0 = static_cast<int>(std::floor(r.y / cell_));
    x1 = static_cast<int>(std::floor((r.x + r.width) / cell_));
    y1 = static_cast<int>(std::floor((r.y + r.height) / cell_));
    x0 = std::clamp(x0, 0, cols_ - 1);
    x1 = std::clamp(x1, 0, cols_ - 1);
    y0 = std::clamp(y0, 0, rows_ - 1);
    y1 = std::clamp(y1, 0, rows_ - 1);
}

void SpatialGrid::insert(int id, Rectangle aabb) {
    int x0, y0, x1, y1;
    cellRange(aabb, x0, y0, x1, y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) cells_[static_cast<size_t>(y * cols_ + x)].push_back(id);
    ++items_;
}

void SpatialGrid::query(Rectangle aabb, std::vector<int>& out) const {
    out.clear();
    int x0, y0, x1, y1;
    cellRange(aabb, x0, y0, x1, y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const auto& c = cells_[static_cast<size_t>(y * cols_ + x)];
            out.insert(out.end(), c.begin(), c.end());
        }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

}  // namespace rd
