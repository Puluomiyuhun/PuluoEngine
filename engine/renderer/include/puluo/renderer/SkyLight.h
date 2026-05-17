#pragma once

#include "puluo/renderer/IBL.h"
#include "puluo/renderer/Atmosphere.h"
#include "puluo/renderer/Shader.h"
#include "puluo/renderer/Cubemap.h"
#include <memory>

namespace Puluo {

class SkyLight {
public:
    SkyLight();
    ~SkyLight();

    // Update sky light IBL maps from atmosphere params.
    // Only re-renders when params have changed.
    void Update(const AtmosphereParams& params);

    const IBLMaps& GetIBLMaps() const { return m_Maps; }
    bool IsValid() const { return m_Maps.irradianceMap != nullptr; }

    // Force a full re-capture next Update()
    void MarkDirty() { m_Dirty = true; }

private:
    void CaptureAtmosphereCubemap(const AtmosphereParams& params);
    void ConvolveIrradiance();
    void PrefilterEnvironment();
    void GenerateBRDFLUT();
    void RenderCube();

    bool ParamsChanged(const AtmosphereParams& params) const;

    IBLMaps m_Maps;

    std::shared_ptr<Shader> m_AtmosphereShader;
    std::shared_ptr<Shader> m_IrradianceShader;
    std::shared_ptr<Shader> m_PrefilterShader;

    uint32_t m_CaptureFBO = 0;
    uint32_t m_CaptureRBO = 0;
    uint32_t m_CubeVAO = 0;
    uint32_t m_CubeVBO = 0;

    AtmosphereParams m_LastParams;
    bool m_Dirty = true;
};

} // namespace Puluo
