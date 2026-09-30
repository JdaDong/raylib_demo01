#include "gfx/postfx.hpp"

#include "core/math2d.hpp"
#include "rlgl.h"

namespace rd {

const char* kPostFxShaderSource = R"(#version 330
// raylib_demo01 post-processing: chromatic aberration, scanlines, film grain,
// vignette, color matrix and vertical tint. Edit saves/shaders/postfx.fs while
// the game runs: it is hot-reloaded (F3 forces a reload).
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform sampler2D noiseTex;
uniform vec2 resolution;
uniform float time;
uniform float vignette;
uniform float aberration;
uniform float scanlines;
uniform vec3 palette[3];     // top tint, bottom tint, additive flash
uniform mat4 colorMatrix;
out vec4 finalColor;

void main() {
    vec2 uv = fragTexCoord;
    vec2 d = uv - 0.5;
    float ab = aberration / resolution.x;
    vec3 c;
    c.r = texture(texture0, uv + d * ab * 2.0).r;
    c.g = texture(texture0, uv).g;
    c.b = texture(texture0, uv - d * ab * 2.0).b;
    float sl = sin(uv.y * resolution.y * 3.14159) * 0.5 + 0.5;
    c *= 1.0 - scanlines * (1.0 - sl);
    float n = texture(noiseTex, uv * 3.0 + vec2(time * 0.37, time * 0.11)).r;
    c += (n - 0.5) * 0.035;
    float v = smoothstep(0.85, 0.25, length(d) * (1.0 + vignette));
    c *= mix(1.0, v, clamp(vignette * 1.6, 0.0, 1.0));
    c = (colorMatrix * vec4(c, 1.0)).rgb;
    c *= mix(palette[0], palette[1], uv.y);
    c += palette[2];
    finalColor = vec4(c, 1.0) * colDiffuse * fragColor;
}
)";

bool PostFx::init(Storage& storage, int width, int height) {
    target_ = LoadRenderTexture(width, height);
    if (!IsRenderTextureValid(target_)) {
        status_ = "render texture failed";
        return false;
    }
    SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
    path_ = storage.path("shaders/postfx.fs");
    if (!FileExists(path_.c_str())) SaveFileText(path_.c_str(), kPostFxShaderSource);
    return reload();
}

void PostFx::cacheLocations() {
    locTime_ = GetShaderLocation(shader_, "time");
    locRes_ = GetShaderLocation(shader_, "resolution");
    locVig_ = GetShaderLocation(shader_, "vignette");
    locAb_ = GetShaderLocation(shader_, "aberration");
    locScan_ = GetShaderLocation(shader_, "scanlines");
    locPalette_ = GetShaderLocation(shader_, "palette");
    locMatrix_ = GetShaderLocation(shader_, "colorMatrix");
    locNoise_ = GetShaderLocation(shader_, "noiseTex");
    const int attrib = GetShaderLocationAttrib(shader_, "vertexPosition");
    TraceLog(LOG_INFO, "POSTFX: locations time=%d palette=%d matrix=%d vertexPosition(attrib)=%d", locTime_, locPalette_, locMatrix_, attrib);
}

bool PostFx::reload() {
    Shader next = LoadShader(nullptr, path_.c_str());  // nullptr -> raylib default vertex shader
    // compile failure falls back to the *default* shader id, which IsShaderValid() accepts
    const bool fileOk = IsShaderValid(next) && next.id != rlGetShaderIdDefault();
    if (!fileOk) {
        if (IsShaderValid(next) && next.id != rlGetShaderIdDefault()) UnloadShader(next);
        if (IsShaderValid(shader_) && shader_.id != rlGetShaderIdDefault()) {
            status_ = "compile error, kept previous shader";
            return false;
        }
        next = LoadShaderFromMemory(nullptr, kPostFxShaderSource);  // embedded fallback
        fromFile_ = false;
        status_ = "file shader invalid, using embedded";
    } else {
        fromFile_ = true;
        status_ = "loaded " + path_;
    }
    if (IsShaderValid(shader_) && shader_.id != rlGetShaderIdDefault()) UnloadShader(shader_);
    shader_ = next;
    modTime_ = GetFileModTime(path_.c_str());
    cacheLocations();
    return IsShaderValid(shader_);
}

void PostFx::pollHotReload(float dt) {
    pollTimer_ += dt;
    if (pollTimer_ < 1.0f) return;
    pollTimer_ = 0.f;
    const long m = GetFileModTime(path_.c_str());
    if (m != 0 && m != modTime_) {
        TraceLog(LOG_INFO, "POSTFX: %s changed, hot reloading", path_.c_str());
        reload();
    }
}

void PostFx::beginScene() { BeginTextureMode(target_); }
void PostFx::endScene() { EndTextureMode(); }

void PostFx::present(Rectangle dest, float time, bool enabled, Texture2D noise) {
    // render textures are stored upside-down (OpenGL) -> negative source height
    const Rectangle src{0, 0, static_cast<float>(target_.texture.width), -static_cast<float>(target_.texture.height)};
    if (enabled && IsShaderValid(shader_)) {
        BeginShaderMode(shader_);
        const Vector2 res{static_cast<float>(target_.texture.width), static_cast<float>(target_.texture.height)};
        SetShaderValue(shader_, locTime_, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, locRes_, &res, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_, locVig_, &vignette, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, locAb_, &aberration, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, locScan_, &scanlines, SHADER_UNIFORM_FLOAT);
        const Vector3 palette[3] = {tintTop, tintBottom, flash};
        SetShaderValueV(shader_, locPalette_, palette, SHADER_UNIFORM_VEC3, 3);
        SetShaderValueMatrix(shader_, locMatrix_, SaturationMatrix(saturation));
        SetShaderValueTexture(shader_, locNoise_, noise);
        DrawTexturePro(target_.texture, src, dest, {0, 0}, 0.f, WHITE);
        EndShaderMode();
    } else {
        DrawTexturePro(target_.texture, src, dest, {0, 0}, 0.f, WHITE);
    }
}

void PostFx::shutdown() {
    if (IsShaderValid(shader_) && shader_.id != rlGetShaderIdDefault()) UnloadShader(shader_);
    if (IsRenderTextureValid(target_)) UnloadRenderTexture(target_);
    shader_ = {};
    target_ = {};
}

}  // namespace rd
