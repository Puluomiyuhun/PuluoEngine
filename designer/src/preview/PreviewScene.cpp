#include "PreviewScene.h"

#include <glad/gl.h>

#include "puluo/core/Log.h"
#include "puluo/renderer/Renderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace sd {

bool PreviewScene::init(const std::string& assetDir) {
    pbr_ = Puluo::Shader::CreateFromFile(assetDir + "/shaders/pbr.vert",
                                         assetDir + "/shaders/pbr.frag");
    if (!pbr_) {
        PULUO_CORE_ERROR("[preview] failed to build PBR shader");
        return false;
    }

    skybox_ = Puluo::Shader::CreateFromFile(assetDir + "/shaders/skybox.vert",
                                            assetDir + "/shaders/skybox.frag");

    sphere_ = Puluo::Mesh::CreateSphere(64, 32);

    Puluo::Light sun;
    sun.type      = Puluo::LightType::Directional;
    sun.direction = Puluo::Vec3(-0.4f, -0.7f, -0.55f);
    sun.color     = Puluo::Vec3(1.0f, 0.97f, 0.92f);
    sun.intensity = 3.0f;
    lights_.Clear();
    lights_.AddLight(sun);
    lights_.SetAmbient(Puluo::Vec3(0.02f));

    // IBL dominates how a material actually reads, so it is worth the one-off
    // preprocessing cost. Absence is non-fatal -- the directional light alone
    // still shows the material, just flatter.
    const std::string hdr = assetDir + "/textures/hdr/environment.hdr";
    if (std::filesystem::exists(hdr)) {
        Puluo::IBLPreprocessor pre;
        ibl_    = pre.Process(hdr);
        hasIbl_ = ibl_.irradianceMap && ibl_.prefilterMap && ibl_.brdfLUT != 0;
        if (!hasIbl_) PULUO_CORE_WARN("[preview] IBL preprocessing incomplete");
    } else {
        PULUO_CORE_WARN("[preview] no environment.hdr at {}, IBL disabled", hdr);
    }

    cam_ = Puluo::CameraController(45.0f, 1.0f);
    cam_.SetInputEnabled(false);  // the viewport drives the orbit, not WASD
    applyCamera_();

    return true;
}

void PreviewScene::shutdown() {
    fbo_.reset();
    pbr_.reset();
    skybox_.reset();
}

void PreviewScene::orbit(float dYaw, float dPitch) {
    yaw_ += dYaw;
    pitch_ = std::clamp(pitch_ + dPitch, -89.0f, 89.0f);
    applyCamera_();
}

void PreviewScene::zoom(float delta) {
    dist_ = std::clamp(dist_ - delta, 1.2f, 20.0f);
    applyCamera_();
}

void PreviewScene::applyCamera_() {
    // CameraController builds its view matrix from yaw/pitch, so set those
    // first and then place the eye at -front * distance to orbit the origin.
    cam_.SetYaw(yaw_);
    cam_.SetPitch(pitch_);

    const float cy = std::cos(glm::radians(yaw_));
    const float sy = std::sin(glm::radians(yaw_));
    const float cp = std::cos(glm::radians(pitch_));
    const float sp = std::sin(glm::radians(pitch_));
    const Puluo::Vec3 front(cy * cp, sp, sy * cp);

    cam_.SetPosition(-front * dist_);
}

uint32_t PreviewScene::colorTexture() const {
    return fbo_ ? fbo_->GetColorAttachmentID() : 0;
}

void PreviewScene::render(const EvalResult& maps, int width, int height) {
    if (width <= 0 || height <= 0) return;

    if (!fbo_ || width != width_ || height != height_) {
        if (!fbo_) {
            Puluo::FramebufferSpec spec;
            spec.width  = static_cast<uint32_t>(width);
            spec.height = static_cast<uint32_t>(height);
            fbo_        = std::make_unique<Puluo::Framebuffer>(spec);
        } else {
            fbo_->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
        width_  = width;
        height_ = height;
        cam_.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        applyCamera_();
    }

    fbo_->Bind();
    glViewport(0, 0, width, height);
    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // sphere winding is not worth depending on here

    if (hasIbl_) {
        Puluo::Renderer::SetIBLMaps(&ibl_);
        Puluo::Renderer::SetIBLIntensity(1.0f);
    }
    Puluo::Renderer::s_ShadowEnabled = false;

    Puluo::Renderer::BeginScene(cam_, lights_);

    if (showSkybox && skybox_ && hasIbl_)
        Puluo::Renderer::RenderSkybox(skybox_, cam_);

    pbr_->Bind();
    pbr_->SetMat4("uViewProjection", cam_.GetViewProjection());
    Puluo::Renderer::BindFrameUniforms(pbr_);

    // Force off the passes this preview does not run. BindFrameUniforms leaves
    // these untouched when the corresponding pointers are null, and a stale
    // enable would sample unbound textures.
    pbr_->SetInt("uShadowEnabled", 0);
    pbr_->SetInt("uSSAOEnabled", 0);
    pbr_->SetInt("uFogEnabled", 0);
    pbr_->SetInt("uUseAlphaMask", 0);
    pbr_->SetInt("uUseSSS", 0);

    pbr_->SetMat4("uModel", Puluo::Mat4(1.0f));
    pbr_->SetMat3("uNormalMatrix", Puluo::Mat3(1.0f));
    pbr_->SetInt("uEntityID", -1);

    // Scalar fallbacks for unconnected channels.
    pbr_->SetVec3("uAlbedo", Puluo::Vec3(0.8f));
    pbr_->SetFloat("uMetallic", fallbackMetallic);
    pbr_->SetFloat("uRoughness", fallbackRoughness);
    pbr_->SetFloat("uAO", 1.0f);

    static const char* kFlags[kPbrChannelCount] = {
        "uUseAlbedoMap", "uUseNormalMap", "uUseRoughnessMap",
        "uUseMetallicMap", "uUseAOMap",
    };
    // pbr.frag slot assignment: 0 albedo, 1 normal, 2 metallic, 3 roughness,
    // 4 AO. PbrChannel order puts roughness before metallic, so the mapping is
    // deliberately not the identity.
    static const int kSlots[kPbrChannelCount] = {0, 1, 3, 2, 4};

    for (int c = 0; c < kPbrChannelCount; ++c) {
        const uint32_t tex = maps.channel[c];
        pbr_->SetInt(kFlags[c], tex ? 1 : 0);
        if (tex) glBindTextureUnit(static_cast<GLuint>(kSlots[c]), tex);
    }

    sphere_.Draw();

    fbo_->Unbind();
}

}  // namespace sd
