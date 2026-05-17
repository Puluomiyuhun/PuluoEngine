#include "puluo/renderer/IBL.h"
#include "puluo/core/Log.h"
#include "puluo/core/Math.h"

#include <glad/gl.h>
#include "stb/stb_image.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Puluo {

// Cube vertex data for rendering into cubemap faces
static float s_CubeVertices[] = {
    // positions
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

IBLPreprocessor::IBLPreprocessor() {
    SetupFramebuffer();

    // Create cube VAO
    glGenVertexArrays(1, &m_CubeVAO);
    glGenBuffers(1, &m_CubeVBO);
    glBindVertexArray(m_CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_CubeVertices), s_CubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);

    // Load shaders
    m_EquirectToCubeShader = Shader::CreateFromFile("assets/shaders/ibl/equirect_to_cube.vert",
                                                     "assets/shaders/ibl/equirect_to_cube.frag");
    m_IrradianceShader = Shader::CreateFromFile("assets/shaders/ibl/irradiance.vert",
                                                 "assets/shaders/ibl/irradiance.frag");
    m_PrefilterShader = Shader::CreateFromFile("assets/shaders/ibl/prefilter.vert",
                                                "assets/shaders/ibl/prefilter.frag");
    m_BRDFShader = Shader::CreateFromFile("assets/shaders/ibl/brdf.vert",
                                           "assets/shaders/ibl/brdf.frag");
}

IBLPreprocessor::~IBLPreprocessor() {
    if (m_CaptureFBO) glDeleteFramebuffers(1, &m_CaptureFBO);
    if (m_CaptureRBO) glDeleteRenderbuffers(1, &m_CaptureRBO);
    if (m_CubeVAO) glDeleteVertexArrays(1, &m_CubeVAO);
    if (m_CubeVBO) glDeleteBuffers(1, &m_CubeVBO);
}

void IBLPreprocessor::SetupFramebuffer() {
    glGenFramebuffers(1, &m_CaptureFBO);
    glGenRenderbuffers(1, &m_CaptureRBO);
}

void IBLPreprocessor::RenderCube() {
    glBindVertexArray(m_CubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

uint32_t IBLPreprocessor::LoadHDR(const std::string& path) {
    stbi_set_flip_vertically_on_load(1);
    int width, height, channels;
    float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        PULUO_CORE_ERROR("Failed to load HDR: {0}", path);
        return 0;
    }

    uint32_t hdrTexture;
    glCreateTextures(GL_TEXTURE_2D, 1, &hdrTexture);
    glTextureStorage2D(hdrTexture, 1, GL_RGB16F, width, height);
    glTextureSubImage2D(hdrTexture, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, data);
    glTextureParameteri(hdrTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdrTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdrTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(hdrTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    PULUO_CORE_INFO("HDR loaded: {0} ({1}x{2})", path, width, height);
    return hdrTexture;
}

std::shared_ptr<Cubemap> IBLPreprocessor::EquirectToCubemap(uint32_t hdrTexture, uint32_t size) {
    auto cubemap = std::make_shared<Cubemap>(size, true, false);

    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_CaptureRBO);

    m_EquirectToCubeShader->Bind();
    m_EquirectToCubeShader->SetInt("uEquirectangularMap", 0);
    m_EquirectToCubeShader->SetMat4("uProjection", s_CaptureProjection);
    glBindTextureUnit(0, hdrTexture);

    glViewport(0, 0, size, size);
    for (uint32_t i = 0; i < 6; i++) {
        m_EquirectToCubeShader->SetMat4("uView", s_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap->GetRendererID(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate mipmaps for prefilter step
    cubemap->GenerateMipmaps();

    PULUO_CORE_INFO("IBL: Environment cubemap generated ({0}x{0})", size);
    return cubemap;
}

std::shared_ptr<Cubemap> IBLPreprocessor::ConvolveIrradiance(const Cubemap& envMap, uint32_t size) {
    auto irradianceMap = std::make_shared<Cubemap>(size, true, false);

    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);

    m_IrradianceShader->Bind();
    m_IrradianceShader->SetInt("uEnvironmentMap", 0);
    m_IrradianceShader->SetMat4("uProjection", s_CaptureProjection);
    glBindTextureUnit(0, envMap.GetRendererID());

    glViewport(0, 0, size, size);
    for (uint32_t i = 0; i < 6; i++) {
        m_IrradianceShader->SetMat4("uView", s_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap->GetRendererID(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    PULUO_CORE_INFO("IBL: Irradiance map generated ({0}x{0})", size);
    return irradianceMap;
}

std::shared_ptr<Cubemap> IBLPreprocessor::PrefilterEnvironment(const Cubemap& envMap, uint32_t size) {
    auto prefilterMap = std::make_shared<Cubemap>(size, true, true);

    m_PrefilterShader->Bind();
    m_PrefilterShader->SetInt("uEnvironmentMap", 0);
    m_PrefilterShader->SetMat4("uProjection", s_CaptureProjection);
    glBindTextureUnit(0, envMap.GetRendererID());

    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    uint32_t maxMipLevels = 5;
    for (uint32_t mip = 0; mip < maxMipLevels; mip++) {
        uint32_t mipSize = static_cast<uint32_t>(size * std::pow(0.5f, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);

        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        m_PrefilterShader->SetFloat("uRoughness", roughness);

        for (uint32_t i = 0; i < 6; i++) {
            m_PrefilterShader->SetMat4("uView", s_CaptureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap->GetRendererID(), mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    PULUO_CORE_INFO("IBL: Pre-filtered environment map generated ({0}x{0}, {1} mips)", size, maxMipLevels);
    return prefilterMap;
}

uint32_t IBLPreprocessor::GenerateBRDFLUT() {
    const uint32_t LUT_SIZE = 512;

    // Create the LUT texture
    uint32_t brdfLUT;
    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT);
    glTextureStorage2D(brdfLUT, 1, GL_RG16F, LUT_SIZE, LUT_SIZE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load and compile compute shader
    std::ifstream file("assets/shaders/ibl/brdf_compute.comp");
    if (!file.is_open()) {
        PULUO_CORE_ERROR("Failed to open BRDF compute shader");
        glDeleteTextures(1, &brdfLUT);
        return 0;
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
        PULUO_CORE_ERROR("BRDF compute shader compile error: {0}", infoLog);
        glDeleteShader(shader);
        glDeleteTextures(1, &brdfLUT);
        return 0;
    }

    uint32_t program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        PULUO_CORE_ERROR("BRDF compute shader link error: {0}", infoLog);
        glDeleteProgram(program);
        glDeleteShader(shader);
        glDeleteTextures(1, &brdfLUT);
        return 0;
    }
    glDeleteShader(shader);

    // Dispatch compute shader
    glUseProgram(program);
    glBindImageTexture(0, brdfLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    glDispatchCompute((LUT_SIZE + 15) / 16, (LUT_SIZE + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUseProgram(0);
    glDeleteProgram(program);

    PULUO_CORE_INFO("IBL: BRDF LUT generated (GPU compute, {0}x{0})", LUT_SIZE);
    return brdfLUT;
}

IBLMaps IBLPreprocessor::Process(const std::string& hdrPath) {
    IBLMaps maps;

    uint32_t hdrTexture = LoadHDR(hdrPath);
    if (!hdrTexture) return maps;

    // Save/restore viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    maps.envCubemap = EquirectToCubemap(hdrTexture, 512);
    maps.irradianceMap = ConvolveIrradiance(*maps.envCubemap, 32);
    maps.prefilterMap = PrefilterEnvironment(*maps.envCubemap, 128);
    maps.brdfLUT = GenerateBRDFLUT();

    // Cleanup HDR texture
    glDeleteTextures(1, &hdrTexture);

    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    PULUO_CORE_INFO("IBL pipeline complete");
    return maps;
}

} // namespace Puluo
