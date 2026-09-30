// postfx.hpp - virtual-resolution render target + hot-reloadable post shader
#pragma once
#include "game/storage.hpp"
#include "raylib.h"

#include <string>

namespace rd {

class PostFx {
public:
    bool init(Storage& storage, int width, int height);
    void shutdown();

    void beginScene();  // BeginTextureMode(target)
    void endScene();    // EndTextureMode()
    // draw the target letterboxed onto the backbuffer, through the shader if enabled
    void present(Rectangle dest, float time, bool enabled, Texture2D noise);

    // polls GetFileModTime() of the shader file (cheap, once per second)
    void pollHotReload(float dt);
    bool reload();

    RenderTexture2D& target() { return target_; }
    bool fileShaderActive() const { return fromFile_; }
    const std::string& status() const { return status_; }
    const std::string& shaderPath() const { return path_; }

    float vignette = 0.35f, aberration = 1.4f, scanlines = 0.16f, saturation = 1.15f;
    Vector3 tintTop{1.0f, 0.97f, 1.04f}, tintBottom{1.02f, 1.0f, 0.96f}, flash{0, 0, 0};

private:
    void cacheLocations();
    RenderTexture2D target_{};
    Shader shader_{};
    bool fromFile_ = false;
    std::string path_;
    long modTime_ = 0;
    float pollTimer_ = 0.f;
    std::string status_;
    int locTime_ = -1, locRes_ = -1, locVig_ = -1, locAb_ = -1, locScan_ = -1, locPalette_ = -1, locMatrix_ = -1, locNoise_ = -1;
};

extern const char* kPostFxShaderSource;

}  // namespace rd
