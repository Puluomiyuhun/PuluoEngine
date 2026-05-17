#include "puluo/renderer/Light.h"
#include "puluo/core/Log.h"

namespace Puluo {

void LightManager::Clear() {
    m_Lights.clear();
}

void LightManager::AddLight(const Light& light) {
    if (m_Lights.size() >= MAX_LIGHTS) {
        PULUO_CORE_WARN("Max lights ({0}) reached, ignoring", MAX_LIGHTS);
        return;
    }
    m_Lights.push_back(light);
}

LightBufferData LightManager::GetBufferData() const {
    LightBufferData data{};

    for (uint32_t i = 0; i < m_Lights.size(); i++) {
        const auto& l = m_Lights[i];
        auto& d = data.lights[i];

        d.positionAndType = Vec4(l.position, static_cast<float>(l.type));
        d.directionAndIntensity = Vec4(l.direction, l.intensity);
        d.color = Vec4(l.color, 0.0f);
        d.attenuation = Vec4(l.constant, l.linear, l.quadratic, 0.0f);
        d.cutoff = Vec4(l.innerCutoff, l.outerCutoff, 0.0f, 0.0f);
    }

    data.ambientAndCount = Vec4(m_Ambient, static_cast<float>(m_Lights.size()));

    return data;
}

} // namespace Puluo
