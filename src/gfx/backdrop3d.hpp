// backdrop3d.hpp - the Title screen's 3D scene: procedural meshes/models,
// a hand-built dynamic mesh (UploadMesh + UpdateMeshBuffer), mouse ray picking.
#pragma once
#include "game/storage.hpp"
#include "gfx/assets.hpp"
#include "raylib.h"

#include <string>

namespace rd {

class Backdrop3D {
public:
    bool init(const Assets& assets, Storage& storage);
    void shutdown();
    void update(float dt);
    // mouse is in virtual coordinates; rendering targets a vw x vh render texture
    void draw(const Camera3D& cam, Vector2 mouse, int vw, int vh);
    const std::string& hoverInfo() const { return hover_; }

private:
    void updateRibbon();

    Model torus_{}, cube_{}, knot_{}, terrain_{}, blocks_{};
    Mesh poly_{}, ribbon_{};
    Material mat_{};
    Texture2D icon_{}, explosion_{};
    BoundingBox cubeBox_{};
    float t_ = 0.f;
    bool ready_ = false;
    std::string hover_;
};

}  // namespace rd
