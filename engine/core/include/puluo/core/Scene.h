#pragma once

#include "puluo/core/Math.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <optional>

namespace Puluo {

class Model; // Forward declaration

// Light data stored in scene (no renderer dependency)
struct SceneLightData {
    int type = 0;  // 0=Directional, 1=Point, 2=Spot
    Vec3 color{1.0f};
    float intensity = 1.0f;
    // Attenuation (point/spot)
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    // Spot
    float innerCutoffDeg = 12.5f;
    float outerCutoffDeg = 17.5f;
};

// Instance transform data for instanced static mesh (no renderer dependency)
struct InstanceTransformData {
    Vec3 position{0.0f};
    Vec3 rotation{0.0f};  // Euler degrees
    Vec3 scale{1.0f};
};

// Instanced static mesh data stored in scene (no renderer dependency)
struct SceneInstancedMeshData {
    std::string modelPath;
    std::vector<InstanceTransformData> instances;
    // Scatter generation params (for editor quick setup)
    float scatterRadius = 50.0f;
    int scatterCount = 100;
    float scaleMin = 0.8f;
    float scaleMax = 1.2f;
    float rotationRandomY = 360.0f;
};

// Terrain data stored in scene (no renderer dependency)
struct SceneTerrainLayerPaths {
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
    float normalStrength = 1.0f;
};

struct SceneTerrainData {
    bool created = false;
    float worldSize = 500.0f;
    int heightmapRes = 257;
    int patchCount = 64;
    float heightScale = 80.0f;
    float uvScale = 50.0f;
    float noiseFreq = 3.0f;
    int noiseOctaves = 6;
    std::string heightmapPath; // empty = use noise, non-empty = load from image
    // Three-layer material paths (GL IDs are runtime-only, rebuilt on load)
    SceneTerrainLayerPaths lower;
    SceneTerrainLayerPaths upper;
    SceneTerrainLayerPaths slope;
    // Blend parameters
    float heightThreshold = 0.5f;
    float slopeThreshold  = 0.6f;
    float blendSharpness  = 8.0f;
    // Splat map
    std::string splatMapPath; // empty = no splat map (use procedural), non-empty = load from PNG
};

// Water plane data stored in scene (no renderer dependency)
struct SceneWaterData {
    float planeSize = 50.0f;
    Vec3 tintColor{0.1f, 0.3f, 0.5f};
    float waveSpeed = 1.0f;
    float waveScale = 0.5f;
    float reflectionStrength = 0.8f;
    float opacity = 0.85f;
    float fresnelPower = 3.0f;
};

// Particle emitter data stored in scene (no renderer dependency)
struct SceneParticleData {
    bool enabled = true;
    float emissionRate = 50.0f;
    int maxParticles = 1000;
    Vec3 velocityMin{-0.5f, 1.0f, -0.5f};
    Vec3 velocityMax{ 0.5f, 3.0f,  0.5f};
    float lifetimeMin = 1.0f;
    float lifetimeMax = 3.0f;
    float sizeStart = 0.2f;
    float sizeEnd = 0.05f;
    Vec4 colorStart{1.0f, 0.8f, 0.3f, 1.0f};
    Vec4 colorEnd{0.5f, 0.1f, 0.0f, 0.0f};
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    float drag = 0.5f;
    int blendMode = 1; // 0=Alpha, 1=Additive
    std::string presetName;
};

struct Transform {
    Vec3 position{0.0f};
    Quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion (w,x,y,z)
    Vec3 scale{1.0f};

    // Euler angles hint for UI display (degrees, Y-X-Z order)
    Vec3 eulerHint{0.0f};

    Mat4 ToMatrix() const {
        Mat4 m(1.0f);
        m = glm::translate(m, position);
        m *= glm::toMat4(orientation);
        m = glm::scale(m, scale);
        return m;
    }

    void FromMatrix(const Mat4& matrix) {
        Vec3 skew;
        Vec4 perspective;
        glm::decompose(matrix, scale, orientation, position, skew, perspective);
        // Update euler hint from quaternion
        eulerHint = glm::degrees(glm::eulerAngles(orientation));
    }

    // Set rotation from euler angles in degrees (Y-X-Z order, for UI/serialization)
    void SetEulerDegrees(const Vec3& eulerDeg) {
        eulerHint = eulerDeg;
        Vec3 rad = glm::radians(eulerDeg);
        orientation = Quat(rad); // glm constructs from euler (pitch, yaw, roll)
    }

    Vec3 GetEulerDegrees() const {
        return eulerHint;
    }
};

// Per-material-slot override data (texture .passet paths + PBR parameters)
struct MaterialOverrideData {
    // Texture override paths (empty = use model default)
    std::string albedoMapPath;
    std::string normalMapPath;
    std::string metallicMapPath;
    std::string roughnessMapPath;
    std::string aoMapPath;
    std::string maskMapPath;

    // PBR scalar parameters
    Vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float alphaCutoff = 0.5f;
    bool useAlphaMask = false;
    bool useSSS = false;
    Vec3 sssColor{1.0f, 1.0f, 1.0f};
    float sssStrength = 0.5f;
};

struct SceneObject {
    std::string name;
    std::string modelPath;
    std::shared_ptr<Model> model;
    Transform transform;
    std::optional<SceneLightData> light;       // If present, this object is a light source
    std::optional<SceneParticleData> particle;  // If present, this object is a particle emitter
    std::optional<SceneInstancedMeshData> instancedMesh; // If present, this object is an instanced mesh group
    std::optional<SceneTerrainData> terrain;    // If present, this object is terrain
    std::optional<SceneWaterData> water;        // If present, this object is a water plane
    std::map<int, MaterialOverrideData> materialOverrides; // Per-slot material overrides
};

struct SceneObjectData {
    std::string name;
    std::string modelPath;
    Transform transform;
    std::optional<SceneLightData> light;
    std::optional<SceneParticleData> particle;
    std::optional<SceneInstancedMeshData> instancedMesh;
    std::optional<SceneTerrainData> terrain;
    std::optional<SceneWaterData> water;
    std::map<int, MaterialOverrideData> materialOverrides;
};

class Scene {
public:
    SceneObject& AddObject(const std::string& name, std::shared_ptr<Model> model) {
        m_Objects.push_back({name, "", model, Transform{}});
        return m_Objects.back();
    }

    void RemoveObject(size_t index) {
        if (index < m_Objects.size()) {
            if (m_SelectedIndex == static_cast<int>(index))
                m_SelectedIndex = -1;
            else if (m_SelectedIndex > static_cast<int>(index))
                m_SelectedIndex--;
            m_Objects.erase(m_Objects.begin() + index);
        }
    }

    // Insert object at specific index (used by undo system)
    void InsertObject(size_t index, SceneObject obj) {
        if (index > m_Objects.size()) index = m_Objects.size();
        m_Objects.insert(m_Objects.begin() + static_cast<ptrdiff_t>(index), std::move(obj));
        // Adjust selection if it shifted
        if (m_SelectedIndex >= 0 && static_cast<size_t>(m_SelectedIndex) >= index)
            m_SelectedIndex++;
    }

    void ClearSelection() { m_SelectedIndex = -1; }

    void Select(size_t index) {
        if (index < m_Objects.size())
            m_SelectedIndex = static_cast<int>(index);
    }

    SceneObject* GetSelected() {
        if (m_SelectedIndex >= 0 && m_SelectedIndex < static_cast<int>(m_Objects.size()))
            return &m_Objects[m_SelectedIndex];
        return nullptr;
    }

    int GetSelectedIndex() const { return m_SelectedIndex; }

    std::vector<SceneObject>& GetObjects() { return m_Objects; }
    const std::vector<SceneObject>& GetObjects() const { return m_Objects; }

    // Save scene to JSON file
    bool SaveToFile(const std::string& filepath) const {
        nlohmann::json j = ToJson();
        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    // Serialize objects to JSON (for use by external save logic)
    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["version"] = 2;
        j["objects"] = nlohmann::json::array();
        for (const auto& obj : m_Objects) {
            nlohmann::json jObj;
            jObj["name"] = obj.name;
            jObj["modelPath"] = obj.modelPath;
            jObj["transform"]["position"] = {obj.transform.position.x, obj.transform.position.y, obj.transform.position.z};
            auto euler = obj.transform.GetEulerDegrees();
            jObj["transform"]["rotation"] = {euler.x, euler.y, euler.z};
            jObj["transform"]["scale"] = {obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z};
            if (obj.light.has_value()) {
                auto& l = obj.light.value();
                nlohmann::json jLight;
                jLight["type"] = l.type;
                jLight["color"] = {l.color.x, l.color.y, l.color.z};
                jLight["intensity"] = l.intensity;
                jLight["constant"] = l.constant;
                jLight["linear"] = l.linear;
                jLight["quadratic"] = l.quadratic;
                jLight["innerCutoffDeg"] = l.innerCutoffDeg;
                jLight["outerCutoffDeg"] = l.outerCutoffDeg;
                jObj["light"] = jLight;
            }
            if (obj.particle.has_value()) {
                auto& p = obj.particle.value();
                nlohmann::json jPart;
                jPart["enabled"] = p.enabled;
                jPart["emissionRate"] = p.emissionRate;
                jPart["maxParticles"] = p.maxParticles;
                jPart["velocityMin"] = {p.velocityMin.x, p.velocityMin.y, p.velocityMin.z};
                jPart["velocityMax"] = {p.velocityMax.x, p.velocityMax.y, p.velocityMax.z};
                jPart["lifetimeMin"] = p.lifetimeMin;
                jPart["lifetimeMax"] = p.lifetimeMax;
                jPart["sizeStart"] = p.sizeStart;
                jPart["sizeEnd"] = p.sizeEnd;
                jPart["colorStart"] = {p.colorStart.x, p.colorStart.y, p.colorStart.z, p.colorStart.w};
                jPart["colorEnd"] = {p.colorEnd.x, p.colorEnd.y, p.colorEnd.z, p.colorEnd.w};
                jPart["gravity"] = {p.gravity.x, p.gravity.y, p.gravity.z};
                jPart["drag"] = p.drag;
                jPart["blendMode"] = p.blendMode;
                jPart["presetName"] = p.presetName;
                jObj["particle"] = jPart;
            }
            if (obj.instancedMesh.has_value()) {
                auto& im = obj.instancedMesh.value();
                nlohmann::json jIM;
                jIM["modelPath"] = im.modelPath;
                jIM["scatterRadius"] = im.scatterRadius;
                jIM["scatterCount"] = im.scatterCount;
                jIM["scaleMin"] = im.scaleMin;
                jIM["scaleMax"] = im.scaleMax;
                jIM["rotationRandomY"] = im.rotationRandomY;
                jIM["instances"] = nlohmann::json::array();
                for (const auto& inst : im.instances) {
                    nlohmann::json jInst;
                    jInst["position"] = {inst.position.x, inst.position.y, inst.position.z};
                    jInst["rotation"] = {inst.rotation.x, inst.rotation.y, inst.rotation.z};
                    jInst["scale"] = {inst.scale.x, inst.scale.y, inst.scale.z};
                    jIM["instances"].push_back(jInst);
                }
                jObj["instancedMesh"] = jIM;
            }
            if (obj.terrain.has_value()) {
                auto& td = obj.terrain.value();
                nlohmann::json jT;
                jT["created"] = td.created;
                jT["worldSize"] = td.worldSize;
                jT["heightmapRes"] = td.heightmapRes;
                jT["patchCount"] = td.patchCount;
                jT["heightScale"] = td.heightScale;
                jT["uvScale"] = td.uvScale;
                jT["noiseFreq"] = td.noiseFreq;
                jT["noiseOctaves"] = td.noiseOctaves;
                jT["heightmapPath"] = td.heightmapPath;
                jT["heightThreshold"] = td.heightThreshold;
                jT["slopeThreshold"] = td.slopeThreshold;
                jT["blendSharpness"] = td.blendSharpness;
                jT["splatMapPath"] = td.splatMapPath;
                // Three-layer material paths
                jT["lower"] = {{"albedo", td.lower.albedoPath}, {"normal", td.lower.normalPath}, {"roughness", td.lower.roughnessPath}, {"normalStrength", td.lower.normalStrength}};
                jT["upper"] = {{"albedo", td.upper.albedoPath}, {"normal", td.upper.normalPath}, {"roughness", td.upper.roughnessPath}, {"normalStrength", td.upper.normalStrength}};
                jT["slope"] = {{"albedo", td.slope.albedoPath}, {"normal", td.slope.normalPath}, {"roughness", td.slope.roughnessPath}, {"normalStrength", td.slope.normalStrength}};
                jObj["terrain"] = jT;
            }
            if (obj.water.has_value()) {
                auto& w = obj.water.value();
                nlohmann::json jW;
                jW["planeSize"] = w.planeSize;
                jW["tintColor"] = {w.tintColor.x, w.tintColor.y, w.tintColor.z};
                jW["waveSpeed"] = w.waveSpeed;
                jW["waveScale"] = w.waveScale;
                jW["reflectionStrength"] = w.reflectionStrength;
                jW["opacity"] = w.opacity;
                jW["fresnelPower"] = w.fresnelPower;
                jObj["water"] = jW;
            }
            if (!obj.materialOverrides.empty()) {
                nlohmann::json jOverrides = nlohmann::json::object();
                for (const auto& [slot, ovr] : obj.materialOverrides) {
                    nlohmann::json jMat;
                    if (!ovr.albedoMapPath.empty()) jMat["albedoMapPath"] = ovr.albedoMapPath;
                    if (!ovr.normalMapPath.empty()) jMat["normalMapPath"] = ovr.normalMapPath;
                    if (!ovr.metallicMapPath.empty()) jMat["metallicMapPath"] = ovr.metallicMapPath;
                    if (!ovr.roughnessMapPath.empty()) jMat["roughnessMapPath"] = ovr.roughnessMapPath;
                    if (!ovr.aoMapPath.empty()) jMat["aoMapPath"] = ovr.aoMapPath;
                    if (!ovr.maskMapPath.empty()) jMat["maskMapPath"] = ovr.maskMapPath;
                    jMat["albedo"] = {ovr.albedo.x, ovr.albedo.y, ovr.albedo.z};
                    jMat["metallic"] = ovr.metallic;
                    jMat["roughness"] = ovr.roughness;
                    jMat["ao"] = ovr.ao;
                    jMat["alphaCutoff"] = ovr.alphaCutoff;
                    jMat["useAlphaMask"] = ovr.useAlphaMask;
                    jMat["useSSS"] = ovr.useSSS;
                    jMat["sssColor"] = {ovr.sssColor.x, ovr.sssColor.y, ovr.sssColor.z};
                    jMat["sssStrength"] = ovr.sssStrength;
                    jOverrides[std::to_string(slot)] = jMat;
                }
                jObj["materialOverrides"] = jOverrides;
            }
            j["objects"].push_back(jObj);
        }
        return j;
    }

    // Load scene data from JSON file (returns object data; caller loads Model instances)
    static std::vector<SceneObjectData> LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return {};
        nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (j.is_discarded()) return {};
        return FromJson(j);
    }

    // Load full JSON from file (for external load logic that also reads environment)
    static nlohmann::json LoadJsonFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return nlohmann::json();
        nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (j.is_discarded()) return nlohmann::json();
        return j;
    }

    // Deserialize objects from JSON
    static std::vector<SceneObjectData> FromJson(const nlohmann::json& j) {
        std::vector<SceneObjectData> result;
        if (!j.contains("objects")) return result;
        for (auto& jObj : j["objects"]) {
            SceneObjectData data;
            data.name = jObj.value("name", "Unnamed");
            data.modelPath = jObj.value("modelPath", "");
            if (jObj.contains("transform")) {
                auto& t = jObj["transform"];
                if (t.contains("position")) {
                    auto& p = t["position"];
                    data.transform.position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                }
                if (t.contains("rotation")) {
                    auto& r = t["rotation"];
                    Vec3 eulerDeg = {r[0].get<float>(), r[1].get<float>(), r[2].get<float>()};
                    data.transform.SetEulerDegrees(eulerDeg);
                }
                if (t.contains("scale")) {
                    auto& s = t["scale"];
                    data.transform.scale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
                }
            }
            if (jObj.contains("light")) {
                auto& jL = jObj["light"];
                SceneLightData l;
                l.type = jL.value("type", 0);
                if (jL.contains("color")) {
                    auto& c = jL["color"];
                    l.color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
                }
                l.intensity = jL.value("intensity", 1.0f);
                l.constant = jL.value("constant", 1.0f);
                l.linear = jL.value("linear", 0.09f);
                l.quadratic = jL.value("quadratic", 0.032f);
                l.innerCutoffDeg = jL.value("innerCutoffDeg", 12.5f);
                l.outerCutoffDeg = jL.value("outerCutoffDeg", 17.5f);
                data.light = l;
            }
            if (jObj.contains("particle")) {
                auto& jP = jObj["particle"];
                SceneParticleData pd;
                pd.enabled = jP.value("enabled", true);
                pd.emissionRate = jP.value("emissionRate", 50.0f);
                pd.maxParticles = jP.value("maxParticles", 1000);
                if (jP.contains("velocityMin")) {
                    auto& v = jP["velocityMin"];
                    pd.velocityMin = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
                }
                if (jP.contains("velocityMax")) {
                    auto& v = jP["velocityMax"];
                    pd.velocityMax = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
                }
                pd.lifetimeMin = jP.value("lifetimeMin", 1.0f);
                pd.lifetimeMax = jP.value("lifetimeMax", 3.0f);
                pd.sizeStart = jP.value("sizeStart", 0.2f);
                pd.sizeEnd = jP.value("sizeEnd", 0.05f);
                if (jP.contains("colorStart")) {
                    auto& c = jP["colorStart"];
                    pd.colorStart = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
                }
                if (jP.contains("colorEnd")) {
                    auto& c = jP["colorEnd"];
                    pd.colorEnd = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
                }
                if (jP.contains("gravity")) {
                    auto& g = jP["gravity"];
                    pd.gravity = {g[0].get<float>(), g[1].get<float>(), g[2].get<float>()};
                }
                pd.drag = jP.value("drag", 0.5f);
                pd.blendMode = jP.value("blendMode", 1);
                pd.presetName = jP.value("presetName", std::string(""));
                data.particle = pd;
            }
            if (jObj.contains("instancedMesh")) {
                auto& jIM = jObj["instancedMesh"];
                SceneInstancedMeshData imd;
                imd.modelPath = jIM.value("modelPath", std::string(""));
                imd.scatterRadius = jIM.value("scatterRadius", 50.0f);
                imd.scatterCount = jIM.value("scatterCount", 100);
                imd.scaleMin = jIM.value("scaleMin", 0.8f);
                imd.scaleMax = jIM.value("scaleMax", 1.2f);
                imd.rotationRandomY = jIM.value("rotationRandomY", 360.0f);
                if (jIM.contains("instances")) {
                    for (auto& jInst : jIM["instances"]) {
                        InstanceTransformData itd;
                        if (jInst.contains("position")) {
                            auto& p = jInst["position"];
                            itd.position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                        }
                        if (jInst.contains("rotation")) {
                            auto& r = jInst["rotation"];
                            itd.rotation = {r[0].get<float>(), r[1].get<float>(), r[2].get<float>()};
                        }
                        if (jInst.contains("scale")) {
                            auto& s = jInst["scale"];
                            itd.scale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
                        }
                        imd.instances.push_back(itd);
                    }
                }
                data.instancedMesh = imd;
            }
            if (jObj.contains("terrain")) {
                auto& jT = jObj["terrain"];
                SceneTerrainData td;
                td.created = jT.value("created", false);
                td.worldSize = jT.value("worldSize", 500.0f);
                td.heightmapRes = jT.value("heightmapRes", 257);
                td.patchCount = jT.value("patchCount", 64);
                td.heightScale = jT.value("heightScale", 80.0f);
                td.uvScale = jT.value("uvScale", 50.0f);
                td.noiseFreq = jT.value("noiseFreq", 3.0f);
                td.noiseOctaves = jT.value("noiseOctaves", 6);
                td.heightmapPath = jT.value("heightmapPath", std::string(""));
                td.heightThreshold = jT.value("heightThreshold", 0.5f);
                td.slopeThreshold = jT.value("slopeThreshold", 0.6f);
                td.blendSharpness = jT.value("blendSharpness", 8.0f);
                td.splatMapPath = jT.value("splatMapPath", std::string(""));
                // Three-layer material paths
                auto readLayer = [](const nlohmann::json& j, SceneTerrainLayerPaths& lp) {
                    lp.albedoPath = j.value("albedo", std::string(""));
                    lp.normalPath = j.value("normal", std::string(""));
                    lp.roughnessPath = j.value("roughness", std::string(""));
                    lp.normalStrength = j.value("normalStrength", 1.0f);
                };
                if (jT.contains("lower")) readLayer(jT["lower"], td.lower);
                if (jT.contains("upper")) readLayer(jT["upper"], td.upper);
                if (jT.contains("slope")) readLayer(jT["slope"], td.slope);
                data.terrain = td;
            }
            if (jObj.contains("water")) {
                auto& jW = jObj["water"];
                SceneWaterData wd;
                wd.planeSize = jW.value("planeSize", 50.0f);
                if (jW.contains("tintColor")) {
                    auto& tc = jW["tintColor"];
                    wd.tintColor = {tc[0].get<float>(), tc[1].get<float>(), tc[2].get<float>()};
                }
                wd.waveSpeed = jW.value("waveSpeed", 1.0f);
                wd.waveScale = jW.value("waveScale", 0.5f);
                wd.reflectionStrength = jW.value("reflectionStrength", 0.8f);
                wd.opacity = jW.value("opacity", 0.85f);
                wd.fresnelPower = jW.value("fresnelPower", 3.0f);
                data.water = wd;
            }
            if (jObj.contains("materialOverrides")) {
                auto& jOverrides = jObj["materialOverrides"];
                for (auto it = jOverrides.begin(); it != jOverrides.end(); ++it) {
                    int slot = std::stoi(it.key());
                    auto& jMat = it.value();
                    MaterialOverrideData ovr;
                    ovr.albedoMapPath = jMat.value("albedoMapPath", std::string(""));
                    ovr.normalMapPath = jMat.value("normalMapPath", std::string(""));
                    ovr.metallicMapPath = jMat.value("metallicMapPath", std::string(""));
                    ovr.roughnessMapPath = jMat.value("roughnessMapPath", std::string(""));
                    ovr.aoMapPath = jMat.value("aoMapPath", std::string(""));
                    ovr.maskMapPath = jMat.value("maskMapPath", std::string(""));
                    if (jMat.contains("albedo")) {
                        auto& a = jMat["albedo"];
                        ovr.albedo = {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
                    }
                    ovr.metallic = jMat.value("metallic", 0.0f);
                    ovr.roughness = jMat.value("roughness", 0.5f);
                    ovr.ao = jMat.value("ao", 1.0f);
                    ovr.alphaCutoff = jMat.value("alphaCutoff", 0.5f);
                    ovr.useAlphaMask = jMat.value("useAlphaMask", false);
                    ovr.useSSS = jMat.value("useSSS", false);
                    if (jMat.contains("sssColor")) {
                        auto& sc = jMat["sssColor"];
                        ovr.sssColor = {sc[0].get<float>(), sc[1].get<float>(), sc[2].get<float>()};
                    }
                    ovr.sssStrength = jMat.value("sssStrength", 0.5f);
                    data.materialOverrides[slot] = ovr;
                }
            }
            result.push_back(std::move(data));
        }
        return result;
    }

private:
    std::vector<SceneObject> m_Objects;
    int m_SelectedIndex = -1;
};

} // namespace Puluo
