#pragma once

#include "puluo/renderer/Cubemap.h"
#include "puluo/renderer/Shader.h"
#include <memory>
#include <string>

namespace Puluo {

struct IBLMaps {
    std::shared_ptr<Cubemap> envCubemap;         // 512x512 HDR cubemap
    std::shared_ptr<Cubemap> irradianceMap;       // 32x32 diffuse irradiance
    std::shared_ptr<Cubemap> prefilterMap;        // 128x128 specular prefilter (mipmapped)
    uint32_t brdfLUT = 0;                         // 512x512 BRDF integration LUT (2D texture)
};

class IBLPreprocessor {
public:
    IBLPreprocessor();
    ~IBLPreprocessor();

    // Full pipeline: HDR equirect -> env cubemap -> irradiance + prefilter + BRDF LUT
    IBLMaps Process(const std::string& hdrPath);

    // Generate BRDF LUT (only needs to be done once)
    uint32_t GenerateBRDFLUT();

private:
    std::shared_ptr<Shader> m_EquirectToCubeShader;
    std::shared_ptr<Shader> m_IrradianceShader;
    std::shared_ptr<Shader> m_PrefilterShader;
    std::shared_ptr<Shader> m_BRDFShader;

    uint32_t m_CaptureFBO = 0;
    uint32_t m_CaptureRBO = 0;

    void SetupFramebuffer();

    std::shared_ptr<Cubemap> EquirectToCubemap(uint32_t hdrTexture, uint32_t size);
    std::shared_ptr<Cubemap> ConvolveIrradiance(const Cubemap& envMap, uint32_t size);
    std::shared_ptr<Cubemap> PrefilterEnvironment(const Cubemap& envMap, uint32_t size);

    uint32_t LoadHDR(const std::string& path);
    void RenderCube();

    uint32_t m_CubeVAO = 0;
    uint32_t m_CubeVBO = 0;
};

} // namespace Puluo
