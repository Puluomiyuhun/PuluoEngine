#pragma once
// 3D material preview.
//
// This is the only file in the designer that touches PuluoEngine, and it only
// consumes it: the engine has no knowledge of the designer, and nothing here
// requires engine changes. Graph results reach pbr.frag by binding raw GL
// texture names to the slots Renderer::BindPBRMaterial would have used
// (0 albedo, 1 normal, 2 metallic, 3 roughness, 4 AO), which lets us skip
// Texture2D entirely -- it cannot adopt an externally created texture.

#include <memory>
#include <string>

#include "puluo/renderer/Camera.h"
#include "puluo/renderer/Framebuffer.h"
#include "puluo/renderer/IBL.h"
#include "puluo/renderer/Light.h"
#include "puluo/renderer/Mesh.h"
#include "puluo/renderer/Shader.h"

#include "../graph/Evaluator.h"

namespace sd {

class PreviewScene {
public:
    bool init(const std::string& assetDir);
    void shutdown();

    void render(const EvalResult& maps, int width, int height);

    uint32_t colorTexture() const;

    void orbit(float dYaw, float dPitch);
    void zoom(float delta);

    // Fallback scalars used for any channel the graph leaves unconnected.
    float  fallbackMetallic  = 0.0f;
    float  fallbackRoughness = 0.5f;
    bool   showSkybox        = true;

private:
    std::unique_ptr<Puluo::Framebuffer> fbo_;
    std::shared_ptr<Puluo::Shader>      pbr_;
    std::shared_ptr<Puluo::Shader>      skybox_;
    Puluo::Mesh                         sphere_;
    Puluo::CameraController             cam_;
    Puluo::LightManager                 lights_;
    Puluo::IBLMaps                      ibl_;
    bool                                hasIbl_ = false;

    float yaw_   = -90.0f;
    float pitch_ = 0.0f;
    float dist_  = 3.0f;

    int width_  = 0;
    int height_ = 0;

    void applyCamera_();
};

}  // namespace sd
