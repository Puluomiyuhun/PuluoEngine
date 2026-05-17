#pragma once

#include "puluo/core/Scene.h"
#include "puluo/core/CommandHistory.h"
#include "puluo/core/Math.h"
#include "puluo/renderer/Camera.h"
#include "puluo/renderer/Atmosphere.h"
#include "puluo/renderer/Fog.h"
#include "puluo/renderer/Cloud.h"
#include "puluo/renderer/Terrain.h"
#include "puluo/renderer/SSAO.h"
#include "puluo/renderer/SSR.h"
#include "puluo/renderer/InstancedMesh.h"
#include "puluo/renderer/WeatherSystem.h"

#include <string>
#include <unordered_map>

namespace Puluo {

enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};

void DrawSceneHierarchy(Scene& scene, CommandHistory& history);
void DrawInspector(Scene& scene, CommandHistory& history,
                   Terrain& terrain, TerrainParams& terrainParams,
                   TerrainMaterial& terrainMaterial,
                   std::unordered_map<size_t, std::unique_ptr<InstancedMesh>>& instancedMeshes,
                   float& instancedMaxDrawDistance);
void DrawToolbar(GizmoMode& mode, bool& wantsImport, CameraController& camera,
                 bool& useAtmosphere, AtmosphereParams& atmosphereParams,
                 FogParams& fogParams, CloudParams& cloudParams,
                 bool& fxaaEnabled, float& saturation, float& contrast,
                 SSAOConfig& ssaoConfig, SSRConfig& ssrConfig,
                 WeatherConfig& weatherConfig);
bool DrawGizmo(SceneObject& object, const Mat4& view, const Mat4& projection,
               GizmoMode mode, float viewportX, float viewportY, float viewportW, float viewportH);
void DrawAssetBrowser(std::string& importPath, std::string& scenePath);
void DrawStatsOverlay(bool* open, float vpX, float vpY, float vpW, float vpH);

// Instanced mesh rebuild signal (set by editor, consumed by app)
bool ConsumeInstancedMeshRebuildFlag();

// Terrain rebuild signals (set by inspector, consumed by app)
bool ConsumeTerrainCreateFlag();
bool ConsumeTerrainRegenerateFlag();
std::string ConsumeTerrainHeightmapLoadPath();

// Terrain splat map brush state (read by app for painting)
struct SplatBrushState {
    bool enabled = false;
    int layer = 0;       // 0=Lower, 1=Upper, 2=Slope
    float radius = 10.0f;
    float strength = 0.5f;
    bool eraseMode = false;
};

SplatBrushState& GetSplatBrushState();
bool ConsumeSplatGenerateFlag();

} // namespace Puluo
