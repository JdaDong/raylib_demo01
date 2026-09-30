#include "gfx/backdrop3d.hpp"

#include "gfx/procgen.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>
#include <vector>

namespace rd {

namespace {
constexpr int kRibbonSegments = 48;
std::vector<Model> g_shelf;  // one model per GenMesh* generator + an OBJ round-trip

BoundingBox Translate(BoundingBox b, Vector3 d) { return {Vector3Add(b.min, d), Vector3Add(b.max, d)}; }
}  // namespace

bool Backdrop3D::init(const Assets& assets, Storage& storage) {
    (void)assets;
    {
        Image ic = GenIconImage(64);
        icon_ = LoadTextureFromImage(ic);
        UnloadImage(ic);
        Image sheet = GenExplosionSheet(64, 8);
        explosion_ = LoadTextureFromImage(sheet);
        UnloadImage(sheet);
        // cubemap from a 4x3 cross layout (probe: validates the upload path, then released)
        Image cross = GenImageColor(64 * 4, 64 * 3, BLANK);
        const Color faces[6] = {RED, GREEN, BLUE, GOLD, VIOLET, SKYBLUE};
        const int fx[6] = {1, 0, 1, 2, 3, 1}, fy[6] = {0, 1, 1, 1, 1, 2};
        for (int i = 0; i < 6; ++i) ImageDrawRectangle(&cross, fx[i] * 64, fy[i] * 64, 64, 64, faces[i]);
        TextureCubemap cube = LoadTextureCubemap(cross, CUBEMAP_LAYOUT_AUTO_DETECT);
        TraceLog(LOG_INFO, "BACKDROP3D: cubemap id=%u %dx%d", cube.id, cube.width, cube.height);
        if (IsTextureValid(cube)) UnloadTexture(cube);
        UnloadImage(cross);
    }

    // textured cube with tangents, exported to OBJ + C header and re-imported
    Mesh cubeMesh = GenMeshCube(1.4f, 1.4f, 1.4f);
    GenMeshTangents(&cubeMesh);
    cubeBox_ = Translate(GetMeshBoundingBox(cubeMesh), {0, 1.2f, 0});
    const std::string obj = storage.path("exports/cube.obj");
    ExportMesh(cubeMesh, obj.c_str());
    ExportMeshAsCode(cubeMesh, storage.path("exports/cube_mesh.h").c_str());
    cube_ = LoadModelFromMesh(cubeMesh);
    SetMaterialTexture(&cube_.materials[0], MATERIAL_MAP_DIFFUSE, icon_);
    SetModelMeshMaterial(&cube_, 0, 0);

    torus_ = LoadModelFromMesh(GenMeshTorus(0.35f, 2.2f, 16, 48));
    torus_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GetColor(0xFF4D6DFF);
    knot_ = LoadModelFromMesh(GenMeshKnot(0.6f, 1.6f, 24, 96));

    // terrain from a Perlin heightmap, blocks from a cubicmap image
    {
        Image hm = GenImagePerlinNoise(64, 64, 7, 3, 3.0f);
        terrain_ = LoadModelFromMesh(GenMeshHeightmap(hm, {24.f, 1.5f, 24.f}));
        terrain_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GetColor(0x2A3A7AFF);
        UnloadImage(hm);
        Image cm = GenImageColor(12, 12, BLACK);
        for (int i = 0; i < 12; ++i) {
            ImageDrawPixel(&cm, i, 0, WHITE);
            ImageDrawPixel(&cm, 0, i, WHITE);
            if (i % 3 == 0) ImageDrawPixel(&cm, 11, i, WHITE);
        }
        blocks_ = LoadModelFromMesh(GenMeshCubicmap(cm, {0.5f, 0.8f, 0.5f}));
        blocks_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GetColor(0x5AD7FFFF);
        UnloadImage(cm);
    }

    // a shelf with every remaining generator
    g_shelf.clear();
    g_shelf.push_back(LoadModelFromMesh(GenMeshPlane(1.f, 1.f, 4, 4)));
    g_shelf.push_back(LoadModelFromMesh(GenMeshSphere(0.5f, 12, 16)));
    g_shelf.push_back(LoadModelFromMesh(GenMeshHemiSphere(0.5f, 10, 16)));
    g_shelf.push_back(LoadModelFromMesh(GenMeshCylinder(0.4f, 1.f, 12)));
    g_shelf.push_back(LoadModelFromMesh(GenMeshCone(0.5f, 1.f, 12)));
    Model fromObj = LoadModel(obj.c_str());
    if (IsModelValid(fromObj)) g_shelf.push_back(fromObj);
    int matCount = 0;
    Material* mats = LoadMaterials(obj.c_str(), &matCount);  // OBJ without .mtl -> none/default
    for (int i = 0; i < matCount; ++i)
        if (IsMaterialValid(mats[i])) UnloadMaterial(mats[i]);
    if (mats) MemFree(mats);

    poly_ = GenMeshPoly(6, 0.8f);  // drawn with DrawMesh + a default material
    mat_ = LoadMaterialDefault();
    mat_.maps[MATERIAL_MAP_DIFFUSE].color = GetColor(0x7CFF6BFF);

    // hand-built dynamic mesh: CPU arrays (MemAlloc) -> UploadMesh(dynamic) -> UpdateMeshBuffer each frame
    ribbon_ = Mesh{};
    ribbon_.triangleCount = kRibbonSegments * 2;
    ribbon_.vertexCount = ribbon_.triangleCount * 3;
    ribbon_.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(ribbon_.vertexCount * 3 * sizeof(float))));
    ribbon_.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(ribbon_.vertexCount * 4)));
    for (int i = 0; i < ribbon_.vertexCount; ++i) {
        const Color c = ColorFromHSV(360.f * static_cast<float>(i) / ribbon_.vertexCount, 0.7f, 1.f);
        ribbon_.colors[i * 4 + 0] = c.r;
        ribbon_.colors[i * 4 + 1] = c.g;
        ribbon_.colors[i * 4 + 2] = c.b;
        ribbon_.colors[i * 4 + 3] = 200;
    }
    updateRibbon();
    UploadMesh(&ribbon_, true);

    const BoundingBox kb = GetModelBoundingBox(knot_);
    TraceLog(LOG_INFO, "BACKDROP3D: shelf=%d models, knot bbox %.1fx%.1f, obj materials=%d, material valid=%d", static_cast<int>(g_shelf.size()),
             kb.max.x - kb.min.x, kb.max.y - kb.min.y, matCount, IsMaterialValid(mat_));
    ready_ = IsModelValid(cube_) && IsModelValid(torus_);
    return ready_;
}

void Backdrop3D::updateRibbon() {
    float* v = ribbon_.vertices;
    int k = 0;
    auto put = [&](Vector3 p) { v[k++] = p.x; v[k++] = p.y; v[k++] = p.z; };
    for (int i = 0; i < kRibbonSegments; ++i) {
        const float a0 = static_cast<float>(i) / kRibbonSegments * 2.f * PI, a1 = static_cast<float>(i + 1) / kRibbonSegments * 2.f * PI;
        const float r = 5.f;
        auto at = [&](float a, float h) {
            return Vector3{std::cos(a) * r, 0.6f + std::sin(a * 3.f + t_ * 2.f) * 0.5f + h, std::sin(a) * r};
        };
        const Vector3 p0 = at(a0, 0.f), p1 = at(a1, 0.f), q0 = at(a0, 0.35f), q1 = at(a1, 0.35f);
        put(p0); put(q0); put(p1);
        put(p1); put(q0); put(q1);
    }
}

void Backdrop3D::update(float dt) {
    if (!ready_) return;
    t_ += dt;
    updateRibbon();
    UpdateMeshBuffer(ribbon_, 0, ribbon_.vertices, ribbon_.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
}

void Backdrop3D::draw(const Camera3D& cam, Vector2 mouse, int vw, int vh) {
    if (!ready_) return;
    // --- picking (before drawing, so hover colors apply this frame) ---------------------
    const Ray ray = GetScreenToWorldRayEx(mouse, cam, vw, vh);
    const Matrix torusXf = MatrixMultiply(MatrixRotateX(t_ * 0.6f), MatrixTranslate(0, 1.2f, 0));
    const Vector3 sphereC{-3.5f, 1.f + std::sin(t_) * 0.4f, 2.5f};
    const Vector3 t0{3.f, 0.3f, 3.f}, t1{4.5f, 0.3f, 3.f}, t2{3.75f, 1.8f, 3.f};
    const Vector3 qa{-12, 0, -12}, qb{-12, 0, 12}, qc{12, 0, 12}, qd{12, 0, -12};
    hover_.clear();
    RayCollision best{};
    auto consider = [&](RayCollision c, const char* what) {
        if (c.hit && (!best.hit || c.distance < best.distance)) {
            best = c;
            hover_ = TextFormat("%s  d=%.2f  p=(%.1f, %.1f, %.1f)", what, c.distance, c.point.x, c.point.y, c.point.z);
        }
    };
    consider(GetRayCollisionBox(ray, cubeBox_), "GetRayCollisionBox: cube");
    consider(GetRayCollisionSphere(ray, sphereC, 0.8f), "GetRayCollisionSphere");
    consider(GetRayCollisionMesh(ray, torus_.meshes[0], torusXf), "GetRayCollisionMesh: torus");
    consider(GetRayCollisionTriangle(ray, t0, t1, t2), "GetRayCollisionTriangle");
    if (!best.hit) consider(GetRayCollisionQuad(ray, qa, qb, qc, qd), "GetRayCollisionQuad: ground");

    // orbiting probe vs the cube: 3D collision checks
    const Vector3 orbit{std::cos(t_ * 0.8f) * 2.2f, 1.2f, std::sin(t_ * 0.8f) * 2.2f};
    const bool sphereHitsCube = CheckCollisionBoxSphere(cubeBox_, orbit, 0.45f);
    const bool sphereHitsSphere = CheckCollisionSpheres(orbit, 0.45f, sphereC, 0.8f);
    const BoundingBox probeBox{Vector3Subtract(orbit, {0.3f, 0.3f, 0.3f}), Vector3Add(orbit, {0.3f, 0.3f, 0.3f})};
    const bool boxesHit = CheckCollisionBoxes(probeBox, cubeBox_);

    // --- scene ------------------------------------------------------------------------------
    DrawModel(terrain_, {-12.f, -1.6f, -12.f}, 1.f, WHITE);
    DrawModelWires(blocks_, {-9.f, 0.f, -9.f}, 1.f, Fade(WHITE, 0.25f));
    DrawPlane({0, 0.01f, 0}, {6, 6}, Fade(GetColor(0x101830FF), 0.6f));
    DrawModel(cube_, {0, 1.2f, 0}, 1.f, WHITE);
    DrawBoundingBox(cubeBox_, best.hit && hover_.find("cube") != std::string::npos ? GOLD : Fade(SKYBLUE, 0.4f));
    DrawModelEx(torus_, {0, 1.2f, 0}, {1, 0, 0}, t_ * 0.6f * RAD2DEG, {1, 1, 1}, WHITE);
    DrawModelWiresEx(knot_, {4.5f, 1.8f, -3.f}, {0, 1, 0}, t_ * 40.f, {0.8f, 0.8f, 0.8f}, Fade(VIOLET, 0.7f));
    DrawMesh(poly_, mat_, MatrixMultiply(MatrixRotateY(t_), MatrixTranslate(-4.5f, 0.05f, -2.f)));
    rlDisableBackfaceCulling();
    DrawMesh(ribbon_, mat_, MatrixIdentity());  // vertex colors * material color
    rlEnableBackfaceCulling();

    for (size_t i = 0; i < g_shelf.size(); ++i)
        DrawModelWires(g_shelf[i], {-5.f + static_cast<float>(i) * 1.6f, 0.5f, 5.5f}, 1.f, Fade(GOLD, 0.6f));

    DrawSphere(sphereC, 0.8f, sphereHitsSphere ? RED : Fade(ORANGE, 0.7f));
    DrawSphereEx(orbit, 0.45f, 8, 8, sphereHitsCube ? RED : LIME);
    DrawCubeWiresV(orbit, {0.6f, 0.6f, 0.6f}, boxesHit ? RED : Fade(WHITE, 0.5f));
    DrawCubeV({3.75f, 0.1f, 3.f}, {1.6f, 0.2f, 0.2f}, DARKGRAY);
    DrawTriangle3D(t0, t1, t2, Fade(SKYBLUE, 0.7f));
    DrawTriangle3D(t0, t2, t1, Fade(SKYBLUE, 0.4f));  // back face
    const Vector3 strip[6] = {{-6, 0.2f, 0}, {-6, 1.0f, 0}, {-5, 0.2f, 0.5f}, {-5, 1.0f, 0.5f}, {-4, 0.2f, 0}, {-4, 1.0f, 0}};
    DrawTriangleStrip3D(strip, 6, Fade(PINK, 0.6f));
    DrawCylinder({6, 0, 0}, 0.2f, 0.5f, 1.2f, 10, Fade(DARKPURPLE, 0.8f));
    DrawCylinderEx({6, 1.2f, 0}, {6.5f, 2.2f, 0.5f}, 0.2f, 0.05f, 8, PURPLE);
    DrawCylinderWiresEx({-6, 0, -4}, {-6, 2, -4}, 0.4f, 0.1f, 8, Fade(PURPLE, 0.6f));
    DrawCapsule({6, 0.5f, 3}, {6, 1.8f, 3}, 0.35f, 8, 8, Fade(GOLD, 0.7f));
    DrawCapsuleWires({-3, 0.5f, -5}, {-1.5f, 1.2f, -5}, 0.3f, 6, 6, Fade(GOLD, 0.5f));
    DrawCircle3D({0, 0.02f, 0}, 3.2f, {1, 0, 0}, 90.f, Fade(SKYBLUE, 0.6f));
    DrawLine3D({-6, 0.02f, 0}, {6, 0.02f, 0}, Fade(RED, 0.5f));
    DrawLine3D({0, 0.02f, -6}, {0, 0.02f, 6}, Fade(BLUE, 0.5f));
    DrawPoint3D({0, 3.2f, 0}, WHITE);
    DrawRay(ray, Fade(WHITE, 0.15f));
    if (best.hit) {
        DrawSphere(best.point, 0.08f, YELLOW);
        DrawLine3D(best.point, Vector3Add(best.point, best.normal), YELLOW);
    }

    // billboards: static icon, animated sprite-sheet frame, rotated/scaled pro
    DrawBillboard(cam, icon_, {-2.5f, 2.6f, -1.5f}, 0.8f, WHITE);
    const int frame = static_cast<int>(t_ * 10.f) % 8;
    DrawBillboardRec(cam, explosion_, {frame * 64.f, 0, 64, 64}, {2.5f, 2.4f, -1.5f}, {1.2f, 1.2f}, WHITE);
    DrawBillboardPro(cam, icon_, {0, 0, 64, 64}, {0, 3.6f, 0}, {0, 1, 0}, {0.6f, 0.6f}, {0.3f, 0.3f}, t_ * 90.f, Fade(WHITE, 0.8f));
}

void Backdrop3D::shutdown() {
    if (!ready_) return;
    UnloadModel(cube_);
    UnloadModel(torus_);
    UnloadModel(knot_);
    UnloadModel(terrain_);
    UnloadModel(blocks_);
    for (Model& m : g_shelf) UnloadModel(m);
    g_shelf.clear();
    UnloadMesh(poly_);
    UnloadMesh(ribbon_);
    UnloadMaterial(mat_);  // default material: shader/texture are shared defaults, only maps are freed
    UnloadTexture(icon_);
    UnloadTexture(explosion_);
    ready_ = false;
}

}  // namespace rd
