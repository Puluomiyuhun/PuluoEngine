#include "puluo/renderer/SkyLight.h"
#include "puluo/core/Log.h"
#include "puluo/core/Math.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Puluo {

// Cube vertex data (positions only, same as IBL.cpp)
static float s_SkyLightCubeVertices[] = {
    -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
};

static Mat4 s_CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
static Mat4 s_CaptureViews[] = {
    glm::lookAt(Vec3(0), Vec3( 1, 0, 0), Vec3(0,-1, 0)),
    glm::lookAt(Vec3(0), Vec3(-1, 0, 0), Vec3(0,-1, 0)),
    glm::lookAt(Vec3(0), Vec3( 0, 1, 0), Vec3(0, 0, 1)),
    glm::lookAt(Vec3(0), Vec3( 0,-1, 0), Vec3(0, 0,-1)),
    glm::lookAt(Vec3(0), Vec3( 0, 0, 1), Vec3(0,-1, 0)),
    glm::lookAt(Vec3(0), Vec3( 0, 0,-1), Vec3(0,-1, 0))
};

// Cubemap sizes for sky light (lower res than HDR IBL since atmosphere is smooth)
static constexpr uint32_t SKY_ENV_SIZE = 64;
static constexpr uint32_t SKY_IRRADIANCE_SIZE = 32;
static constexpr uint32_t SKY_PREFILTER_SIZE = 64;

SkyLight::SkyLight() {
    // Create FBO
    glGenFramebuffers(1, &m_CaptureFBO);
    glGenRenderbuffers(1, &m_CaptureRBO);

    // Create cube VAO
    glGenVertexArrays(1, &m_CubeVAO);
    glGenBuffers(1, &m_CubeVBO);
    glBindVertexArray(m_CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_SkyLightCubeVertices), s_SkyLightCubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);

    // Load shaders
    m_AtmosphereShader = Shader::CreateFromFile("assets/shaders/atmosphere.vert",
                                                 "assets/shaders/atmosphere.frag");
    m_IrradianceShader = Shader::CreateFromFile("assets/shaders/ibl/irradiance.vert",
                                                 "assets/shaders/ibl/irradiance.frag");
    m_PrefilterShader = Shader::CreateFromFile("assets/shaders/ibl/prefilter.vert",
                                                "assets/shaders/ibl/prefilter.frag");

    // Pre-allocate cubemaps (reused across updates, avoids GPU resource churn)
    m_Maps.envCubemap = std::make_shared<Cubemap>(SKY_ENV_SIZE, true, false);
    m_Maps.irradianceMap = std::make_shared<Cubemap>(SKY_IRRADIANCE_SIZE, true, false);
    m_Maps.prefilterMap = std::make_shared<Cubemap>(SKY_PREFILTER_SIZE, true, true);

    // Generate BRDF LUT (constant, only needs to be done once)
    GenerateBRDFLUT();

    PULUO_CORE_INFO("SkyLight initialized");
}

SkyLight::~SkyLight() {
    if (m_CaptureFBO) glDeleteFramebuffers(1, &m_CaptureFBO);
    if (m_CaptureRBO) glDeleteRenderbuffers(1, &m_CaptureRBO);
    if (m_CubeVAO) glDeleteVertexArrays(1, &m_CubeVAO);
    if (m_CubeVBO) glDeleteBuffers(1, &m_CubeVBO);
    if (m_Maps.brdfLUT) glDeleteTextures(1, &m_Maps.brdfLUT);
}

void SkyLight::RenderCube() {
    glBindVertexArray(m_CubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

bool SkyLight::ParamsChanged(const AtmosphereParams& params) const {
    constexpr float eps = 1e-5f;
    return glm::length(params.sunDirection - m_LastParams.sunDirection) > eps
        || std::abs(params.sunIntensity - m_LastParams.sunIntensity) > eps
        || std::abs(params.turbidity - m_LastParams.turbidity) > eps
        || glm::length(params.rayleighCoeff - m_LastParams.rayleighCoeff) > eps
        || std::abs(params.mieCoeff - m_LastParams.mieCoeff) > eps
        || std::abs(params.mieDirectionG - m_LastParams.mieDirectionG) > eps;
}

void SkyLight::Update(const AtmosphereParams& params) {
    if (!m_Dirty && !ParamsChanged(params)) return;

    // Save GL state (critical: we're called mid-frame while scene FBO is bound)
    GLint prevFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);

    CaptureAtmosphereCubemap(params);
    ConvolveIrradiance();
    PrefilterEnvironment();

    // Restore GL state to scene FBO
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDepthFunc(prevDepthFunc);
    // Clear texture unit 0 — PrefilterEnvironment leaves a cubemap bound via DSA,
    // which conflicts with legacy glBindTexture(GL_TEXTURE_2D, ...) in terrain rendering.
    glBindTextureUnit(0, 0);

    m_LastParams = params;
    m_Dirty = false;
}

void SkyLight::CaptureAtmosphereCubemap(const AtmosphereParams& params) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SKY_ENV_SIZE, SKY_ENV_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_CaptureRBO);

    // Atmosphere vertex shader outputs gl_Position.z = 0 (reversed-Z far plane),
    // so we need GL_GEQUAL to pass depth test.
    glDepthFunc(GL_GEQUAL);

    m_AtmosphereShader->Bind();
    m_AtmosphereShader->SetMat4("uProjection", s_CaptureProjection);
    m_AtmosphereShader->SetVec3("uSunDirection", params.sunDirection);
    m_AtmosphereShader->SetFloat("uSunIntensity", params.sunIntensity);
    m_AtmosphereShader->SetFloat("uTurbidity", params.turbidity);
    m_AtmosphereShader->SetVec3("uRayleighCoeff", params.rayleighCoeff);
    m_AtmosphereShader->SetFloat("uMieCoeff", params.mieCoeff);
    m_AtmosphereShader->SetFloat("uMieDirectionG", params.mieDirectionG);
    m_AtmosphereShader->SetInt("uLinearOutput", 1);
    m_AtmosphereShader->SetInt("uFogEnabled", 0);

    glViewport(0, 0, SKY_ENV_SIZE, SKY_ENV_SIZE);
    for (uint32_t i = 0; i < 6; i++) {
        m_AtmosphereShader->SetMat4("uView", s_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_Maps.envCubemap->GetRendererID(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube();
    }

    m_Maps.envCubemap->GenerateMipmaps();
}

void SkyLight::ConvolveIrradiance() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SKY_IRRADIANCE_SIZE, SKY_IRRADIANCE_SIZE);

    m_IrradianceShader->Bind();
    m_IrradianceShader->SetInt("uEnvironmentMap", 0);
    m_IrradianceShader->SetMat4("uProjection", s_CaptureProjection);
    glBindTextureUnit(0, m_Maps.envCubemap->GetRendererID());

    glViewport(0, 0, SKY_IRRADIANCE_SIZE, SKY_IRRADIANCE_SIZE);
    for (uint32_t i = 0; i < 6; i++) {
        m_IrradianceShader->SetMat4("uView", s_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_Maps.irradianceMap->GetRendererID(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube();
    }
}

void SkyLight::PrefilterEnvironment() {
    m_PrefilterShader->Bind();
    m_PrefilterShader->SetInt("uEnvironmentMap", 0);
    m_PrefilterShader->SetMat4("uProjection", s_CaptureProjection);
    glBindTextureUnit(0, m_Maps.envCubemap->GetRendererID());

    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    uint32_t maxMipLevels = 5;
    for (uint32_t mip = 0; mip < maxMipLevels; mip++) {
        uint32_t mipSize = static_cast<uint32_t>(SKY_PREFILTER_SIZE * std::pow(0.5f, mip));
        if (mipSize < 1) mipSize = 1;

        glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);

        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        m_PrefilterShader->SetFloat("uRoughness", roughness);

        for (uint32_t i = 0; i < 6; i++) {
            m_PrefilterShader->SetMat4("uView", s_CaptureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_Maps.prefilterMap->GetRendererID(), mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCube();
        }
    }
}

void SkyLight::GenerateBRDFLUT() {
    const uint32_t LUT_SIZE = 512;

    glCreateTextures(GL_TEXTURE_2D, 1, &m_Maps.brdfLUT);
    glTextureStorage2D(m_Maps.brdfLUT, 1, GL_RG16F, LUT_SIZE, LUT_SIZE);
    glTextureParameteri(m_Maps.brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_Maps.brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_Maps.brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_Maps.brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::ifstream file("assets/shaders/ibl/brdf_compute.comp");
    if (!file.is_open()) {
        PULUO_CORE_ERROR("SkyLight: Failed to open BRDF compute shader");
        glDeleteTextures(1, &m_Maps.brdfLUT);
        m_Maps.brdfLUT = 0;
        return;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    const char* src = source.c_str();

    uint32_t shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        PULUO_CORE_ERROR("SkyLight: BRDF compute compile error: {0}", infoLog);
        glDeleteShader(shader);
        glDeleteTextures(1, &m_Maps.brdfLUT);
        m_Maps.brdfLUT = 0;
        return;
    }

    uint32_t program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram(program);
        glDeleteShader(shader);
        glDeleteTextures(1, &m_Maps.brdfLUT);
        m_Maps.brdfLUT = 0;
        return;
    }
    glDeleteShader(shader);

    glUseProgram(program);
    glBindImageTexture(0, m_Maps.brdfLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    glDispatchCompute((LUT_SIZE + 15) / 16, (LUT_SIZE + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUseProgram(0);
    glDeleteProgram(program);

    PULUO_CORE_INFO("SkyLight: BRDF LUT generated ({0}x{0})", LUT_SIZE);
}

} // namespace Puluo
