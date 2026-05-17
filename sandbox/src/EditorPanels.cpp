#include "EditorPanels.h"

#include "puluo/core/Log.h"
#include "puluo/renderer/Model.h"
#include "puluo/renderer/Texture2D.h"
#include "puluo/renderer/Particle.h"
#include "puluo/renderer/Renderer.h"
#include "puluo/resource/TextureCache.h"
#include "puluo/resource/AssetImporter.h"
#include "puluo/resource/AssetFormat.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <nfd.h>
#include <glad/gl.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>

namespace Puluo {

// Instanced mesh rebuild flag
static bool s_InstancedMeshNeedsRebuild = false;

bool ConsumeInstancedMeshRebuildFlag() {
    bool val = s_InstancedMeshNeedsRebuild;
    s_InstancedMeshNeedsRebuild = false;
    return val;
}

// Terrain rebuild flags
static bool s_TerrainNeedsCreate = false;
static bool s_TerrainNeedsRegenerate = false;
static std::string s_TerrainHeightmapLoadPath; // non-empty = load requested

bool ConsumeTerrainCreateFlag() {
    bool val = s_TerrainNeedsCreate;
    s_TerrainNeedsCreate = false;
    return val;
}

bool ConsumeTerrainRegenerateFlag() {
    bool val = s_TerrainNeedsRegenerate;
    s_TerrainNeedsRegenerate = false;
    return val;
}

std::string ConsumeTerrainHeightmapLoadPath() {
    std::string val = std::move(s_TerrainHeightmapLoadPath);
    s_TerrainHeightmapLoadPath.clear();
    return val;
}

// Keep loaded terrain textures alive
static std::vector<std::shared_ptr<Texture2D>> s_TerrainTextures;

// Helper: load a texture via file dialog, returns {GL texture ID, path} (0/"" on cancel)
static std::pair<uint32_t, std::string> LoadTerrainTextureDialog() {
    nfdu8char_t* outPath = nullptr;
    nfdu8filteritem_t filter = { "Image", "png,jpg,jpeg,tga,bmp,passet" };
    nfdopendialogu8args_t args = {0};
    args.filterList = &filter;
    args.filterCount = 1;
    if (NFD_OpenDialogU8_With(&outPath, &args) == NFD_OKAY) {
        std::string pathStr(outPath);
        NFD_FreePathU8(outPath);
        // External image file: import to project first, then load .passet
        std::string ext = std::filesystem::path(pathStr).extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".passet") {
            std::string passetPath = AssetImporter::ImportTextureToProject(pathStr);
            if (!passetPath.empty()) pathStr = passetPath;
        }
        auto tex = TextureCache::Load(pathStr);
        if (tex && tex->GetRendererID()) {
            s_TerrainTextures.push_back(tex);
            return {tex->GetRendererID(), pathStr};
        }
    }
    return {0, ""};
}

// Keep loaded material textures alive
static std::vector<std::shared_ptr<Texture2D>> s_MaterialTextures;

// Helper: material texture slot UI with thumbnail, browse, and clear
static void MaterialTexSlotUI(const char* label, std::shared_ptr<Texture2D>& texSlot) {
    ImGui::PushID(label);
    ImGui::Text("%s:", label);
    ImGui::SameLine();
    if (texSlot) {
        auto texID = static_cast<ImTextureID>(static_cast<uintptr_t>(texSlot->GetRendererID()));
        ImGui::Image(texID, ImVec2(32, 32));
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            texSlot = nullptr;
        }
    } else {
        ImGui::Text("None");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Browse")) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filter = { "Image", "png,jpg,jpeg,tga,bmp,passet" };
        nfdopendialogu8args_t args = {0};
        args.filterList = &filter;
        args.filterCount = 1;
        if (NFD_OpenDialogU8_With(&outPath, &args) == NFD_OKAY) {
            std::string pathStr(outPath);
            NFD_FreePathU8(outPath);
            // External image: import to project first
            std::string ext = std::filesystem::path(pathStr).extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext != ".passet") {
                std::string passetPath = AssetImporter::ImportTextureToProject(pathStr);
                if (!passetPath.empty()) pathStr = passetPath;
            }
            auto tex = TextureCache::Load(pathStr);
            if (tex && tex->GetRendererID()) {
                s_MaterialTextures.push_back(tex);
                texSlot = tex;
            }
        }
    }
    ImGui::PopID();
}

// Overload: also records the loaded texture path for serialization
static void MaterialTexSlotUI(const char* label, std::shared_ptr<Texture2D>& texSlot, std::string& pathOut) {
    ImGui::PushID(label);
    ImGui::Text("%s:", label);
    ImGui::SameLine();
    if (texSlot) {
        auto texID = static_cast<ImTextureID>(static_cast<uintptr_t>(texSlot->GetRendererID()));
        ImGui::Image(texID, ImVec2(32, 32));
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            texSlot = nullptr;
            pathOut.clear();
        }
    } else {
        ImGui::Text("None");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Browse")) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filter = { "Image", "png,jpg,jpeg,tga,bmp,passet" };
        nfdopendialogu8args_t args = {0};
        args.filterList = &filter;
        args.filterCount = 1;
        if (NFD_OpenDialogU8_With(&outPath, &args) == NFD_OKAY) {
            std::string pathStr(outPath);
            NFD_FreePathU8(outPath);
            std::string ext = std::filesystem::path(pathStr).extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext != ".passet") {
                std::string passetPath = AssetImporter::ImportTextureToProject(pathStr);
                if (!passetPath.empty()) pathStr = passetPath;
            }
            auto tex = TextureCache::Load(pathStr);
            if (tex && tex->GetRendererID()) {
                s_MaterialTextures.push_back(tex);
                texSlot = tex;
                pathOut = pathStr;
            }
        }
    }
    ImGui::PopID();
}

// Helper: show texture slot button
static void TerrainTexSlotUI(const char* label, uint32_t& texID, std::string& texPath) {
    ImGui::PushID(label);
    if (texID) {
        ImGui::Text("%s: loaded", label);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) { texID = 0; texPath.clear(); }
    } else {
        if (ImGui::Button(label)) {
            auto [loaded, path] = LoadTerrainTextureDialog();
            if (loaded) { texID = loaded; texPath = path; }
        }
    }
    ImGui::PopID();
}

// ---- Scene Hierarchy ----

void DrawSceneHierarchy(Scene& scene, CommandHistory& history) {
    ImGui::Begin("Scene Hierarchy");

    auto& objects = scene.GetObjects();
    int selectedIdx = scene.GetSelectedIndex();
    int deleteIdx = -1;

    for (size_t i = 0; i < objects.size(); i++) {
        bool isSelected = (static_cast<int>(i) == selectedIdx);

        // Icon prefix based on object type
        std::string label;
        if (objects[i].water.has_value()) {
            label = std::string("[Water] ") + objects[i].name;
        } else if (objects[i].terrain.has_value()) {
            label = std::string("[Terrain] ") + objects[i].name;
        } else if (objects[i].instancedMesh.has_value()) {
            label = std::string("[ISM] ") + objects[i].name;
        } else if (objects[i].particle.has_value()) {
            label = std::string("[FX] ") + objects[i].name;
        } else if (objects[i].light.has_value()) {
            const char* lightIcons[] = { "[Sun] ", "[Bulb] ", "[Spot] " };
            int lt = objects[i].light->type;
            label = std::string(lightIcons[glm::clamp(lt, 0, 2)]) + objects[i].name;
        } else {
            label = objects[i].name;
        }

        if (ImGui::Selectable(label.c_str(), isSelected)) {
            scene.ClearSelection();
            scene.Select(i);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                deleteIdx = static_cast<int>(i);
            }
            ImGui::EndPopup();
        }
    }

    if (deleteIdx >= 0) {
        // Any deletion can invalidate ISM indices, so always trigger rebuild
        bool hasAnyISM = false;
        for (auto& o : objects) {
            if (o.instancedMesh.has_value()) { hasAnyISM = true; break; }
        }
        if (hasAnyISM) s_InstancedMeshNeedsRebuild = true;

        auto cmd = std::make_unique<DeleteObjectCommand>(scene, static_cast<size_t>(deleteIdx));
        history.Execute(std::move(cmd));
    }

    ImGui::Separator();

    // Add light button
    if (ImGui::Button("+ Add Light")) {
        ImGui::OpenPopup("AddLightPopup");
    }
    if (ImGui::BeginPopup("AddLightPopup")) {
        if (ImGui::MenuItem("Directional Light")) {
            auto& obj = scene.AddObject("Directional Light", nullptr);
            SceneLightData ld;
            ld.type = 0;
            ld.intensity = 2.0f;
            obj.light = ld;
            obj.transform.SetEulerDegrees({-45.0f, 0.0f, 0.0f});
            scene.Select(scene.GetObjects().size() - 1);
        }
        if (ImGui::MenuItem("Point Light")) {
            auto& obj = scene.AddObject("Point Light", nullptr);
            SceneLightData ld;
            ld.type = 1;
            ld.intensity = 5.0f;
            obj.light = ld;
            obj.transform.position = {0.0f, 2.0f, 0.0f};
            scene.Select(scene.GetObjects().size() - 1);
        }
        if (ImGui::MenuItem("Spot Light")) {
            auto& obj = scene.AddObject("Spot Light", nullptr);
            SceneLightData ld;
            ld.type = 2;
            ld.intensity = 10.0f;
            obj.light = ld;
            obj.transform.position = {0.0f, 3.0f, 0.0f};
            obj.transform.SetEulerDegrees({-90.0f, 0.0f, 0.0f});
            scene.Select(scene.GetObjects().size() - 1);
        }
        ImGui::EndPopup();
    }

    // Add particle emitter button
    ImGui::SameLine();
    if (ImGui::Button("+ Add Emitter")) {
        ImGui::OpenPopup("AddEmitterPopup");
    }
    if (ImGui::BeginPopup("AddEmitterPopup")) {
        auto presetNames = GetParticlePresetNames();
        for (const auto& presetName : presetNames) {
            if (ImGui::MenuItem(presetName.c_str())) {
                auto preset = GetParticlePreset(presetName);
                auto& obj = scene.AddObject(presetName + " Emitter", nullptr);
                SceneParticleData pd;
                pd.enabled = preset.enabled;
                pd.emissionRate = preset.emissionRate;
                pd.maxParticles = preset.maxParticles;
                pd.velocityMin = preset.velocityMin;
                pd.velocityMax = preset.velocityMax;
                pd.lifetimeMin = preset.lifetimeMin;
                pd.lifetimeMax = preset.lifetimeMax;
                pd.sizeStart = preset.sizeStart;
                pd.sizeEnd = preset.sizeEnd;
                pd.colorStart = preset.colorStart;
                pd.colorEnd = preset.colorEnd;
                pd.gravity = preset.gravity;
                pd.drag = preset.drag;
                pd.blendMode = static_cast<int>(preset.blendMode);
                pd.presetName = preset.presetName;
                obj.particle = pd;
                obj.transform.position = {0.0f, 1.0f, 0.0f};
                scene.Select(scene.GetObjects().size() - 1);
            }
        }
        if (ImGui::MenuItem("Custom")) {
            auto& obj = scene.AddObject("Particle Emitter", nullptr);
            obj.particle = SceneParticleData{};
            obj.transform.position = {0.0f, 1.0f, 0.0f};
            scene.Select(scene.GetObjects().size() - 1);
        }
        ImGui::EndPopup();
    }

    // Add instanced mesh button
    ImGui::SameLine();
    if (ImGui::Button("+ Add ISM")) {
        ImGui::OpenPopup("AddISMPopup");
    }
    if (ImGui::BeginPopup("AddISMPopup")) {
        ImGui::Text("Select a model file to instance.");
        ImGui::Separator();
        if (ImGui::MenuItem("Browse Model...")) {
            nfdu8filteritem_t filters[] = {{"3D Models", "glb,gltf,fbx,obj,passet"}};
            nfdu8char_t* outPath = nullptr;
            if (NFD_OpenDialogU8(&outPath, filters, 1, nullptr) == NFD_OKAY && outPath) {
                std::string pathStr(outPath);
                NFD_FreePathU8(outPath);
                // External model: import to project first
                std::string ext = std::filesystem::path(pathStr).extension().string();
                for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (ext != ".passet") {
                    std::string passetPath = AssetImporter::ImportModelToProject(pathStr);
                    if (!passetPath.empty()) pathStr = passetPath;
                }
                std::filesystem::path p(pathStr);
                std::string name = "ISM_" + p.stem().string();
                auto& obj = scene.AddObject(name, nullptr);
                SceneInstancedMeshData imd;
                imd.modelPath = pathStr;
                obj.instancedMesh = imd;
                scene.Select(scene.GetObjects().size() - 1);
                s_InstancedMeshNeedsRebuild = true;
            }
        }
        ImGui::EndPopup();
    }

    // Add terrain button (max 1 terrain per scene)
    {
        bool hasTerrain = false;
        for (auto& o : objects) {
            if (o.terrain.has_value()) { hasTerrain = true; break; }
        }
        if (!hasTerrain) {
            ImGui::SameLine();
            if (ImGui::Button("+ Add Terrain")) {
                auto& obj = scene.AddObject("Terrain", nullptr);
                obj.terrain = SceneTerrainData{};
                scene.Select(scene.GetObjects().size() - 1);
            }
        }
    }

    // Add water plane button
    ImGui::SameLine();
    if (ImGui::Button("+ Add Water")) {
        auto& obj = scene.AddObject("Water", nullptr);
        obj.water = SceneWaterData{};
        obj.transform.position = {0.0f, 0.0f, 0.0f};
        scene.Select(scene.GetObjects().size() - 1);
    }

    ImGui::End();
}

void DrawInspector(Scene& scene, CommandHistory& history,
                   Terrain& terrain, TerrainParams& terrainParams,
                   TerrainMaterial& terrainMaterial,
                   std::unordered_map<size_t, std::unique_ptr<InstancedMesh>>& instancedMeshes,
                   float& instancedMaxDrawDistance) {
    ImGui::Begin("Inspector");

    SceneObject* selected = scene.GetSelected();
    if (!selected) {
        ImGui::TextDisabled("No object selected");
        ImGui::End();
        return;
    }

    int selIdx = scene.GetSelectedIndex();
    if (selIdx < 0) {
        ImGui::End();
        return;
    }

    // Name (with undo support)
    static std::string oldName;
    static int trackedNameIdx = -1;
    char nameBuf[256];
    strncpy(nameBuf, selected->name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        selected->name = nameBuf;
    }
    if (ImGui::IsItemActivated()) {
        oldName = selected->name;
        trackedNameIdx = selIdx;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && trackedNameIdx == selIdx && oldName != selected->name) {
        auto cmd = std::make_unique<RenameCommand>(scene, static_cast<size_t>(selIdx), oldName, selected->name);
        history.PushExecuted(std::move(cmd));
    }

    ImGui::Separator();

    // Transform (with undo support)
    static Transform oldTransform;
    static int trackedTransformIdx = -1;
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Position
        ImGui::DragFloat3("Position", glm::value_ptr(selected->transform.position), 0.1f);
        if (ImGui::IsItemActivated()) {
            oldTransform = selected->transform;
            trackedTransformIdx = selIdx;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && trackedTransformIdx == selIdx) {
            auto cmd = std::make_unique<TransformChangeCommand>(scene, static_cast<size_t>(selIdx), oldTransform, selected->transform);
            history.PushExecuted(std::move(cmd));
        }

        // Rotation (display as euler degrees, edit via SetEulerDegrees)
        Vec3 eulerDeg = selected->transform.GetEulerDegrees();
        if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerDeg), 1.0f)) {
            selected->transform.SetEulerDegrees(eulerDeg);
        }
        if (ImGui::IsItemActivated()) {
            oldTransform = selected->transform;
            trackedTransformIdx = selIdx;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && trackedTransformIdx == selIdx) {
            auto cmd = std::make_unique<TransformChangeCommand>(scene, static_cast<size_t>(selIdx), oldTransform, selected->transform);
            history.PushExecuted(std::move(cmd));
        }

        // Scale
        ImGui::DragFloat3("Scale", glm::value_ptr(selected->transform.scale), 0.01f, 0.001f, 100.0f);
        if (ImGui::IsItemActivated()) {
            oldTransform = selected->transform;
            trackedTransformIdx = selIdx;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && trackedTransformIdx == selIdx) {
            auto cmd = std::make_unique<TransformChangeCommand>(scene, static_cast<size_t>(selIdx), oldTransform, selected->transform);
            history.PushExecuted(std::move(cmd));
        }
    }

    // Material editing (syncs to materialOverrides for serialization)
    if (selected->model && ImGui::CollapsingHeader("Materials")) {
        auto& materials = selected->model->GetMutableMaterials();
        for (size_t i = 0; i < materials.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNode("Material", "Material %d", static_cast<int>(i))) {
                auto& mat = materials[i];
                auto& ovr = selected->materialOverrides[static_cast<int>(i)];

                if (ImGui::ColorEdit3("Albedo", &mat.albedo.r)) ovr.albedo = mat.albedo;
                if (ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f, "%.2f")) ovr.metallic = mat.metallic;
                if (ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f, "%.2f")) ovr.roughness = mat.roughness;
                if (ImGui::SliderFloat("AO", &mat.ao, 0.0f, 1.0f, "%.2f")) ovr.ao = mat.ao;

                ImGui::Separator();
                MaterialTexSlotUI("Albedo Map", mat.albedoMap, ovr.albedoMapPath);
                MaterialTexSlotUI("Normal Map", mat.normalMap, ovr.normalMapPath);
                MaterialTexSlotUI("Metallic Map", mat.metallicMap, ovr.metallicMapPath);
                MaterialTexSlotUI("Roughness Map", mat.roughnessMap, ovr.roughnessMapPath);
                MaterialTexSlotUI("AO Map", mat.aoMap, ovr.aoMapPath);

                ImGui::Separator();
                if (ImGui::Checkbox("Alpha Mask", &mat.useAlphaMask)) ovr.useAlphaMask = mat.useAlphaMask;
                if (mat.useAlphaMask) {
                    MaterialTexSlotUI("Mask Map", mat.maskMap, ovr.maskMapPath);
                    if (ImGui::SliderFloat("Alpha Cutoff", &mat.alphaCutoff, 0.0f, 1.0f, "%.2f")) ovr.alphaCutoff = mat.alphaCutoff;
                }

                if (ImGui::Checkbox("SSS", &mat.useSSS)) ovr.useSSS = mat.useSSS;
                if (mat.useSSS) {
                    if (ImGui::ColorEdit3("SSS Color", &mat.sssColor.r)) ovr.sssColor = mat.sssColor;
                    if (ImGui::SliderFloat("SSS Strength", &mat.sssStrength, 0.0f, 2.0f, "%.2f")) ovr.sssStrength = mat.sssStrength;
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    // Light properties
    if (selected->light.has_value() && ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& ld = selected->light.value();

        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        ImGui::Combo("Type", &ld.type, lightTypes, 3);

        ImGui::ColorEdit3("Color", glm::value_ptr(ld.color));
        ImGui::DragFloat("Intensity", &ld.intensity, 0.1f, 0.0f, 100.0f, "%.1f");

        if (ld.type == 1 || ld.type == 2) { // Point or Spot
            if (ImGui::TreeNode("Attenuation")) {
                ImGui::DragFloat("Constant", &ld.constant, 0.01f, 0.0f, 10.0f, "%.2f");
                ImGui::DragFloat("Linear", &ld.linear, 0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat("Quadratic", &ld.quadratic, 0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::TreePop();
            }
        }

        if (ld.type == 2) { // Spot
            ImGui::DragFloat("Inner Cutoff", &ld.innerCutoffDeg, 0.5f, 0.0f, 90.0f, "%.1f deg");
            ImGui::DragFloat("Outer Cutoff", &ld.outerCutoffDeg, 0.5f, 0.0f, 90.0f, "%.1f deg");
        }
    }

    // Particle emitter properties
    if (selected->particle.has_value() && ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& pd = selected->particle.value();

        ImGui::Checkbox("Enabled", &pd.enabled);

        // Preset selector
        const char* currentPreset = pd.presetName.empty() ? "Custom" : pd.presetName.c_str();
        if (ImGui::BeginCombo("Preset", currentPreset)) {
            auto presetNames = GetParticlePresetNames();
            for (const auto& name : presetNames) {
                if (ImGui::Selectable(name.c_str(), pd.presetName == name)) {
                    auto preset = GetParticlePreset(name);
                    pd.enabled = preset.enabled;
                    pd.emissionRate = preset.emissionRate;
                    pd.maxParticles = preset.maxParticles;
                    pd.velocityMin = preset.velocityMin;
                    pd.velocityMax = preset.velocityMax;
                    pd.lifetimeMin = preset.lifetimeMin;
                    pd.lifetimeMax = preset.lifetimeMax;
                    pd.sizeStart = preset.sizeStart;
                    pd.sizeEnd = preset.sizeEnd;
                    pd.colorStart = preset.colorStart;
                    pd.colorEnd = preset.colorEnd;
                    pd.gravity = preset.gravity;
                    pd.drag = preset.drag;
                    pd.blendMode = static_cast<int>(preset.blendMode);
                    pd.presetName = preset.presetName;
                }
            }
            if (ImGui::Selectable("Custom", pd.presetName.empty())) {
                pd.presetName.clear();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        ImGui::DragFloat("Emission Rate", &pd.emissionRate, 1.0f, 0.0f, 1000.0f, "%.0f/s");
        ImGui::DragInt("Max Particles", &pd.maxParticles, 10, 1, 10000);

        ImGui::Separator();
        ImGui::Text("Velocity");
        ImGui::DragFloat3("Vel Min", glm::value_ptr(pd.velocityMin), 0.1f);
        ImGui::DragFloat3("Vel Max", glm::value_ptr(pd.velocityMax), 0.1f);

        ImGui::Separator();
        ImGui::Text("Lifetime");
        ImGui::DragFloat("Life Min", &pd.lifetimeMin, 0.1f, 0.01f, 30.0f, "%.2f s");
        ImGui::DragFloat("Life Max", &pd.lifetimeMax, 0.1f, 0.01f, 30.0f, "%.2f s");

        ImGui::Separator();
        ImGui::Text("Size");
        ImGui::DragFloat("Size Start", &pd.sizeStart, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Size End", &pd.sizeEnd, 0.01f, 0.001f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Color");
        ImGui::ColorEdit4("Color Start", glm::value_ptr(pd.colorStart));
        ImGui::ColorEdit4("Color End", glm::value_ptr(pd.colorEnd));

        ImGui::Separator();
        ImGui::Text("Physics");
        ImGui::DragFloat3("Gravity", glm::value_ptr(pd.gravity), 0.1f);
        ImGui::DragFloat("Drag", &pd.drag, 0.01f, 0.0f, 5.0f);

        ImGui::Separator();
        const char* blendModes[] = {"Alpha", "Additive"};
        ImGui::Combo("Blend Mode", &pd.blendMode, blendModes, 2);
    }

    // Instanced mesh properties
    if (selected->instancedMesh.has_value() && ImGui::CollapsingHeader("Instanced Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& imd = selected->instancedMesh.value();

        // Model path (read-only display)
        ImGui::Text("Model: %s", imd.modelPath.empty() ? "(none)" : imd.modelPath.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Change...")) {
            nfdu8filteritem_t filters[] = {{"3D Models", "glb,gltf,fbx,obj,passet"}};
            nfdu8char_t* outPath = nullptr;
            if (NFD_OpenDialogU8(&outPath, filters, 1, nullptr) == NFD_OKAY && outPath) {
                std::string pathStr(outPath);
                NFD_FreePathU8(outPath);
                std::string ext = std::filesystem::path(pathStr).extension().string();
                for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (ext != ".passet") {
                    std::string passetPath = AssetImporter::ImportModelToProject(pathStr);
                    if (!passetPath.empty()) pathStr = passetPath;
                }
                imd.modelPath = pathStr;
                imd.instances.clear();
                s_InstancedMeshNeedsRebuild = true;
            }
        }

        ImGui::Text("Instances: %d", static_cast<int>(imd.instances.size()));

        // Show visible count from culling
        {
            int idx = scene.GetSelectedIndex();
            auto cullIt = instancedMeshes.find(static_cast<size_t>(idx));
            if (cullIt != instancedMeshes.end() && cullIt->second) {
                ImGui::SameLine();
                ImGui::Text(" (Visible: %u)", cullIt->second->GetVisibleCount());
            }
        }

        ImGui::DragFloat("Max Draw Distance", &instancedMaxDrawDistance, 10.0f, 0.0f, 10000.0f, "%.0f");
        if (instancedMaxDrawDistance <= 0.0f) {
            ImGui::SameLine();
            ImGui::TextDisabled("(unlimited)");
        }

        ImGui::Separator();
        ImGui::Text("Scatter Generator");
        ImGui::DragFloat("Radius", &imd.scatterRadius, 1.0f, 1.0f, 500.0f, "%.0f");
        ImGui::DragInt("Count", &imd.scatterCount, 1, 1, 10000);
        ImGui::DragFloat("Scale Min", &imd.scaleMin, 0.01f, 0.01f, 10.0f, "%.2f");
        ImGui::DragFloat("Scale Max", &imd.scaleMax, 0.01f, 0.01f, 10.0f, "%.2f");
        ImGui::DragFloat("Y Rotation Random", &imd.rotationRandomY, 1.0f, 0.0f, 360.0f, "%.0f deg");

        if (ImGui::Button("Generate Instances")) {
            // Validate parameters to avoid undefined behavior
            float safeRadius = std::max(imd.scatterRadius, 0.1f);
            float safeScaleMin = std::max(imd.scaleMin, 0.01f);
            float safeScaleMax = std::max(imd.scaleMax, safeScaleMin); // ensure max >= min
            float safeRotY = std::max(imd.rotationRandomY, 0.0f);
            int safeCount = std::max(imd.scatterCount, 1);

            std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::uniform_real_distribution<float> distAngle(0.0f, glm::two_pi<float>());
            std::uniform_real_distribution<float> distRadius(0.0f, safeRadius);
            std::uniform_real_distribution<float> distScale(safeScaleMin, safeScaleMax);

            imd.instances.clear();
            imd.instances.reserve(safeCount);
            for (int i = 0; i < safeCount; i++) {
                InstanceTransformData t;
                float angle = distAngle(rng);
                float r = std::sqrt(distRadius(rng) / safeRadius) * safeRadius; // uniform disk distribution
                t.position = {r * std::cos(angle), 0.0f, r * std::sin(angle)};
                t.rotation = {0.0f, (safeRotY > 0.0f ? std::uniform_real_distribution<float>(0.0f, safeRotY)(rng) : 0.0f), 0.0f};
                float s = distScale(rng);
                t.scale = {s, s, s};
                imd.instances.push_back(t);
            }
            s_InstancedMeshNeedsRebuild = true;
        }

        // Snap all instances to terrain surface
        if (terrain.IsCreated() && !imd.instances.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Snap to Terrain")) {
                Vec3 objPos = selected->transform.position;
                for (auto& inst : imd.instances) {
                    float worldX = objPos.x + inst.position.x;
                    float worldZ = objPos.z + inst.position.z;
                    float terrainY = terrain.GetHeightAt(worldX, worldZ);
                    inst.position.y = terrainY - objPos.y;
                }
                s_InstancedMeshNeedsRebuild = true;
            }
        }

        // Instance list (collapsible, for manual editing)
        if (!imd.instances.empty() && ImGui::TreeNode("Instance List")) {
            int removeIdx = -1;
            int maxShow = std::min(static_cast<int>(imd.instances.size()), 100);
            if (static_cast<int>(imd.instances.size()) > 100) {
                ImGui::TextDisabled("Showing first 100 of %d instances", static_cast<int>(imd.instances.size()));
            }
            for (int i = 0; i < maxShow; i++) {
                ImGui::PushID(i);
                auto& inst = imd.instances[i];
                bool changed = false;
                changed |= ImGui::DragFloat3("Pos", glm::value_ptr(inst.position), 0.1f);
                changed |= ImGui::DragFloat3("Rot", glm::value_ptr(inst.rotation), 1.0f);
                changed |= ImGui::DragFloat3("Scl", glm::value_ptr(inst.scale), 0.01f, 0.01f, 10.0f);
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) removeIdx = i;
                if (changed) s_InstancedMeshNeedsRebuild = true;
                ImGui::Separator();
                ImGui::PopID();
            }
            if (removeIdx >= 0) {
                imd.instances.erase(imd.instances.begin() + removeIdx);
                s_InstancedMeshNeedsRebuild = true;
            }
            if (ImGui::Button("+ Add Instance")) {
                imd.instances.push_back(InstanceTransformData{});
                s_InstancedMeshNeedsRebuild = true;
            }
            ImGui::TreePop();
        }

        // ISM Materials (from the loaded model)
        int selIdx = scene.GetSelectedIndex();
        auto ismIt = instancedMeshes.find(static_cast<size_t>(selIdx));
        if (ismIt != instancedMeshes.end() && ismIt->second && ismIt->second->GetModel()) {
            auto& materials = ismIt->second->GetModel()->GetMutableMaterials();
            if (!materials.empty() && ImGui::CollapsingHeader("ISM Materials")) {
                for (size_t mi = 0; mi < materials.size(); mi++) {
                    ImGui::PushID(static_cast<int>(mi + 10000));
                    if (ImGui::TreeNode("Material", "Material %d", static_cast<int>(mi))) {
                        auto& mat = materials[mi];
                        ImGui::ColorEdit3("Albedo", &mat.albedo.r);
                        ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("AO", &mat.ao, 0.0f, 1.0f, "%.2f");

                        ImGui::Separator();
                        MaterialTexSlotUI("Albedo Map", mat.albedoMap);
                        MaterialTexSlotUI("Normal Map", mat.normalMap);
                        MaterialTexSlotUI("Metallic Map", mat.metallicMap);
                        MaterialTexSlotUI("Roughness Map", mat.roughnessMap);
                        MaterialTexSlotUI("AO Map", mat.aoMap);

                        ImGui::Separator();
                        ImGui::Checkbox("Alpha Mask", &mat.useAlphaMask);
                        if (mat.useAlphaMask) {
                            MaterialTexSlotUI("Mask Map", mat.maskMap);
                            ImGui::SliderFloat("Alpha Cutoff", &mat.alphaCutoff, 0.0f, 1.0f, "%.2f");
                        }

                        ImGui::Checkbox("SSS", &mat.useSSS);
                        if (mat.useSSS) {
                            ImGui::ColorEdit3("SSS Color", &mat.sssColor.r);
                            ImGui::SliderFloat("SSS Strength", &mat.sssStrength, 0.0f, 2.0f, "%.2f");
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }
    }

    // Terrain properties
    if (selected->terrain.has_value() && ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& td = selected->terrain.value();

        if (!terrain.IsCreated()) {
            ImGui::Text("Terrain not created");
            ImGui::DragFloat("World Size", &td.worldSize, 10.0f, 100.0f, 5000.0f, "%.0f");
            ImGui::DragInt("Heightmap Res", &td.heightmapRes, 1, 65, 2049);
            ImGui::DragInt("Patch Count", &td.patchCount, 1, 8, 256);
            ImGui::DragFloat("Height Scale", &td.heightScale, 1.0f, 1.0f, 500.0f, "%.0f");
            ImGui::DragFloat("Noise Freq", &td.noiseFreq, 0.1f, 0.1f, 20.0f, "%.1f");
            ImGui::DragInt("Noise Octaves", &td.noiseOctaves, 1, 1, 10);
            if (ImGui::Button("Create Terrain")) {
                // Sync to runtime params and signal creation
                terrainParams.worldSize = td.worldSize;
                terrainParams.heightmapRes = td.heightmapRes;
                terrainParams.patchCount = td.patchCount;
                terrainParams.heightScale = td.heightScale;
                terrainParams.uvScale = td.uvScale;
                s_TerrainNeedsCreate = true;
            }
        } else {
            ImGui::Text("Terrain: %dx%d, %.0fm",
                td.heightmapRes, td.heightmapRes, td.worldSize);

            bool paramsChanged = false;
            paramsChanged |= ImGui::DragFloat("Height Scale", &td.heightScale, 1.0f, 1.0f, 500.0f, "%.0f");
            paramsChanged |= ImGui::DragFloat("UV Scale", &td.uvScale, 0.5f, 1.0f, 200.0f, "%.1f");
            if (paramsChanged) {
                terrainParams.heightScale = td.heightScale;
                terrainParams.uvScale = td.uvScale;
            }

            ImGui::Separator();
            ImGui::Text("Heightmap Source");
            if (!td.heightmapPath.empty()) {
                ImGui::Text("Image: %s", td.heightmapPath.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear##hm")) {
                    td.heightmapPath.clear();
                    // Revert to noise
                    s_TerrainNeedsRegenerate = true;
                }
            }
            ImGui::DragFloat("Noise Freq", &td.noiseFreq, 0.1f, 0.1f, 20.0f, "%.1f");
            ImGui::DragInt("Noise Octaves", &td.noiseOctaves, 1, 1, 10);
            if (ImGui::Button("Regenerate Noise")) {
                td.heightmapPath.clear();
                s_TerrainNeedsRegenerate = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Heightmap...")) {
                nfdu8char_t* outPath = nullptr;
                nfdu8filteritem_t filter = { "Image", "png,jpg,jpeg,tga,bmp,tif" };
                nfdu8filteritem_t filters[] = { filter };
                if (NFD_OpenDialogU8(&outPath, filters, 1, nullptr) == NFD_OKAY && outPath) {
                    td.heightmapPath = outPath;
                    s_TerrainHeightmapLoadPath = outPath;
                    NFD_FreePathU8(outPath);
                }
            }

            ImGui::Separator();
            ImGui::Text("Material Layers");

            // Blend parameters
            ImGui::DragFloat("Height Split", &terrainMaterial.heightThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Slope Threshold", &terrainMaterial.slopeThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Blend Sharpness", &terrainMaterial.blendSharpness, 0.5f, 1.0f, 30.0f, "%.1f");

            ImGui::Separator();
            if (ImGui::TreeNode("Lower Layer (low height)")) {
                TerrainTexSlotUI("Albedo##lower", terrainMaterial.lower.albedoTex, terrainMaterial.lower.albedoPath);
                TerrainTexSlotUI("Normal##lower", terrainMaterial.lower.normalTex, terrainMaterial.lower.normalPath);
                TerrainTexSlotUI("Roughness##lower", terrainMaterial.lower.roughnessTex, terrainMaterial.lower.roughnessPath);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Upper Layer (high height)")) {
                TerrainTexSlotUI("Albedo##upper", terrainMaterial.upper.albedoTex, terrainMaterial.upper.albedoPath);
                TerrainTexSlotUI("Normal##upper", terrainMaterial.upper.normalTex, terrainMaterial.upper.normalPath);
                TerrainTexSlotUI("Roughness##upper", terrainMaterial.upper.roughnessTex, terrainMaterial.upper.roughnessPath);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Slope Layer (steep areas)")) {
                TerrainTexSlotUI("Albedo##slope", terrainMaterial.slope.albedoTex, terrainMaterial.slope.albedoPath);
                TerrainTexSlotUI("Normal##slope", terrainMaterial.slope.normalTex, terrainMaterial.slope.normalPath);
                TerrainTexSlotUI("Roughness##slope", terrainMaterial.slope.roughnessTex, terrainMaterial.slope.roughnessPath);
                ImGui::TreePop();
            }
        }
    }

    // Water properties
    if (selected->water.has_value() && ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& wd = selected->water.value();

        ImGui::DragFloat("Plane Size", &wd.planeSize, 1.0f, 5.0f, 500.0f, "%.0f");
        ImGui::ColorEdit3("Tint Color", glm::value_ptr(wd.tintColor));
        ImGui::DragFloat("Wave Speed", &wd.waveSpeed, 0.05f, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat("Wave Scale", &wd.waveScale, 0.01f, 0.01f, 5.0f, "%.2f");
        ImGui::DragFloat("Reflection", &wd.reflectionStrength, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Opacity", &wd.opacity, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Fresnel Power", &wd.fresnelPower, 0.1f, 0.5f, 10.0f, "%.1f");
    }

    ImGui::End();
}

// ---- Toolbar ----

void DrawToolbar(GizmoMode& mode, bool& wantsImport, CameraController& camera,
                 bool& useAtmosphere, AtmosphereParams& atmosphereParams,
                 FogParams& fogParams, CloudParams& cloudParams,
                 bool& fxaaEnabled, SSAOConfig& ssaoConfig, SSRConfig& ssrConfig,
                 WeatherConfig& weatherConfig) {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Gizmo Mode:");
    ImGui::SameLine();

    bool isTranslate = (mode == GizmoMode::Translate);
    bool isRotate = (mode == GizmoMode::Rotate);
    bool isScale = (mode == GizmoMode::Scale);

    if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_SliderGrab]);
    if (ImGui::Button("Translate (W)")) mode = GizmoMode::Translate;
    if (isTranslate) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_SliderGrab]);
    if (ImGui::Button("Rotate (E)")) mode = GizmoMode::Rotate;
    if (isRotate) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_SliderGrab]);
    if (ImGui::Button("Scale (R)")) mode = GizmoMode::Scale;
    if (isScale) ImGui::PopStyleColor();

    ImGui::Separator();

    if (ImGui::Button("Import Model...")) {
        wantsImport = true;
    }

    ImGui::Separator();

    // Camera settings
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        float speed = camera.GetMoveSpeed();
        if (ImGui::DragFloat("Move Speed", &speed, 0.1f, 0.5f, 50.0f, "%.1f")) {
            camera.SetMoveSpeed(speed);
        }
        float sensitivity = camera.GetMouseSensitivity();
        if (ImGui::DragFloat("Mouse Sensitivity", &sensitivity, 0.005f, 0.01f, 1.0f, "%.2f")) {
            camera.SetMouseSensitivity(sensitivity);
        }
    }

    ImGui::Separator();

    // Environment settings
    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* envModes[] = { "IBL (HDR Map)", "Atmosphere" };
        int currentMode = useAtmosphere ? 1 : 0;
        if (ImGui::Combo("Sky Mode", &currentMode, envModes, 2)) {
            useAtmosphere = (currentMode == 1);
        }

        if (useAtmosphere) {
            ImGui::Spacing();
            ImGui::Text("Sun Direction:");

            // Convert sun direction to elevation/azimuth angles for intuitive editing
            float elevation = glm::degrees(asin(glm::clamp(atmosphereParams.sunDirection.y, -1.0f, 1.0f)));
            float azimuth = glm::degrees(atan2(atmosphereParams.sunDirection.x, atmosphereParams.sunDirection.z));

            bool changed = false;
            changed |= ImGui::DragFloat("Elevation", &elevation, 0.5f, -10.0f, 90.0f, "%.1f deg");
            changed |= ImGui::DragFloat("Azimuth", &azimuth, 0.5f, -180.0f, 180.0f, "%.1f deg");

            if (changed) {
                float elRad = glm::radians(elevation);
                float azRad = glm::radians(azimuth);
                atmosphereParams.sunDirection = glm::normalize(Vec3(
                    cos(elRad) * sin(azRad),
                    sin(elRad),
                    cos(elRad) * cos(azRad)
                ));
            }

            ImGui::DragFloat("Sun Intensity", &atmosphereParams.sunIntensity, 0.5f, 1.0f, 100.0f, "%.1f");
            ImGui::DragFloat("Turbidity", &atmosphereParams.turbidity, 0.1f, 1.0f, 10.0f, "%.1f");
            ImGui::DragFloat("Mie G", &atmosphereParams.mieDirectionG, 0.01f, 0.0f, 0.999f, "%.3f");
        }

        ImGui::Spacing();
        static float iblIntensity = 1.0f;
        if (ImGui::DragFloat("IBL Intensity", &iblIntensity, 0.05f, 0.0f, 10.0f, "%.2f")) {
            Renderer::SetIBLIntensity(iblIntensity);
        }

        ImGui::Spacing();
        ImGui::Checkbox("Enable Shadows", &Renderer::s_ShadowEnabled);
    }

    ImGui::Separator();

    // Fog settings
    if (ImGui::CollapsingHeader("Fog")) {
        ImGui::Checkbox("Enable Fog", &fogParams.enabled);
        if (fogParams.enabled) {
            ImGui::DragFloat("Density", &fogParams.density, 0.001f, 0.0f, 1.0f, "%.4f");
            ImGui::DragFloat("Height Falloff", &fogParams.heightFalloff, 0.01f, 0.001f, 5.0f, "%.3f");
            ImGui::DragFloat("Max Opacity", &fogParams.maxOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Start Distance", &fogParams.startDistance, 0.5f, 0.0f, 100.0f, "%.1f");
            ImGui::ColorEdit3("Fog Color", glm::value_ptr(fogParams.fogColor));
            ImGui::Separator();
            ImGui::Text("Directional Inscattering");
            ImGui::DragFloat("Dir Exponent", &fogParams.directionalInscatteringExponent, 0.1f, 1.0f, 64.0f, "%.1f");
            ImGui::DragFloat("Dir Start Dist", &fogParams.directionalInscatteringStartDistance, 0.5f, 0.0f, 200.0f, "%.1f");
            // Directional inscattering color auto-derived, show preview
            ImGui::ColorEdit3("Dir Color (Auto)", glm::value_ptr(fogParams.directionalInscatteringColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
        }
    }

    ImGui::Separator();

    // Cloud settings
    if (ImGui::CollapsingHeader("Clouds")) {
        ImGui::Checkbox("Enable Clouds", &cloudParams.enabled);
        if (cloudParams.enabled) {
            ImGui::Text("Cloud Layer");
            ImGui::DragFloat("Bottom (m)", &cloudParams.cloudLayerBottom, 50.0f, 500.0f, 10000.0f, "%.0f");
            ImGui::DragFloat("Thickness (m)", &cloudParams.cloudLayerThickness, 50.0f, 200.0f, 5000.0f, "%.0f");
            ImGui::DragFloat("Coverage", &cloudParams.coverage, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Cloud Density", &cloudParams.density, 0.005f, 0.001f, 0.5f, "%.3f");

            ImGui::Separator();
            ImGui::Text("Noise Scale");
            ImGui::DragFloat("Base Scale", &cloudParams.baseScale, 0.00001f, 0.00001f, 0.001f, "%.5f");
            ImGui::DragFloat("Detail Scale", &cloudParams.detailScale, 0.00001f, 0.00001f, 0.005f, "%.5f");

            ImGui::Separator();
            ImGui::Text("Wind");
            ImGui::DragFloat("Wind Speed", &cloudParams.windSpeed, 0.5f, 0.0f, 50.0f, "%.1f");
            ImGui::DragFloat3("Wind Dir", glm::value_ptr(cloudParams.windDirection), 0.1f, -1.0f, 1.0f, "%.2f");

            ImGui::Separator();
            ImGui::Text("Lighting");
            ImGui::DragFloat("Phase G", &cloudParams.phaseG, 0.01f, 0.0f, 0.99f, "%.2f");
            ImGui::DragFloat("Powder", &cloudParams.powderStrength, 0.1f, 0.0f, 10.0f, "%.1f");
            ImGui::ColorEdit3("Ambient Color", glm::value_ptr(cloudParams.ambientColor));
            ImGui::DragFloat("Ambient Strength", &cloudParams.ambientStrength, 0.01f, 0.0f, 1.0f, "%.2f");
        }
    }

    // ---- Weather ----
    if (ImGui::CollapsingHeader("Weather")) {
        int weatherType = static_cast<int>(weatherConfig.type);
        const char* weatherNames[] = {"None", "Rain", "Snow"};
        if (ImGui::Combo("Type", &weatherType, weatherNames, 3)) {
            WeatherType newType = static_cast<WeatherType>(weatherType);
            if (newType != weatherConfig.type) {
                weatherConfig.type = newType;
                // Apply sensible defaults per type
                if (newType == WeatherType::Rain) {
                    weatherConfig.fallSpeed = 12.0f;
                    weatherConfig.size = 0.03f;
                    weatherConfig.streakLength = 0.4f;
                    weatherConfig.color = Vec4(0.7f, 0.8f, 0.9f, 0.25f);
                } else if (newType == WeatherType::Snow) {
                    weatherConfig.fallSpeed = 1.5f;
                    weatherConfig.size = 0.06f;
                    weatherConfig.streakLength = 0.0f;
                    weatherConfig.color = Vec4(0.95f, 0.95f, 1.0f, 0.6f);
                }
            }
        }
        if (weatherConfig.type != WeatherType::None) {
            ImGui::SliderFloat("Intensity", &weatherConfig.intensity, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Fall Speed", &weatherConfig.fallSpeed, 0.1f, 0.1f, 30.0f, "%.1f");
            ImGui::DragFloat("Wind X", &weatherConfig.wind.x, 0.1f, -20.0f, 20.0f, "%.1f");
            ImGui::DragFloat("Wind Z", &weatherConfig.wind.z, 0.1f, -20.0f, 20.0f, "%.1f");
            ImGui::DragFloat("Size", &weatherConfig.size, 0.005f, 0.01f, 0.5f, "%.3f");
            if (weatherConfig.type == WeatherType::Rain) {
                ImGui::DragFloat("Streak Length", &weatherConfig.streakLength, 0.01f, 0.05f, 2.0f, "%.2f");
            }
            ImGui::ColorEdit4("Color", &weatherConfig.color.r);
        }
    }

    // ---- Post Processing ----
    if (ImGui::CollapsingHeader("Post Processing")) {
        ImGui::Checkbox("FXAA", &fxaaEnabled);

        ImGui::Separator();
        ImGui::Checkbox("SSAO", &ssaoConfig.enabled);
        if (ssaoConfig.enabled) {
            ImGui::SliderFloat("AO Radius", &ssaoConfig.radius, 0.1f, 2.0f, "%.2f");
            ImGui::SliderFloat("AO Bias", &ssaoConfig.bias, 0.001f, 0.1f, "%.3f");
            ImGui::SliderFloat("AO Power", &ssaoConfig.power, 1.0f, 5.0f, "%.1f");
            int ks = static_cast<int>(ssaoConfig.kernelSize);
            if (ImGui::SliderInt("AO Samples", &ks, 8, 64)) {
                ssaoConfig.kernelSize = static_cast<uint32_t>(ks);
            }
        }

        ImGui::Separator();
        ImGui::Checkbox("SSR", &ssrConfig.enabled);
        if (ssrConfig.enabled) {
            ImGui::SliderInt("SSR Steps", &ssrConfig.maxSteps, 16, 128);
            ImGui::SliderFloat("SSR Distance", &ssrConfig.maxDistance, 5.0f, 200.0f, "%.0f");
            ImGui::SliderFloat("SSR Thickness", &ssrConfig.thickness, 0.1f, 2.0f, "%.2f");
        }

        ImGui::Separator();
        const char* debugItems[] = { "Off", "Shadow", "SSAO", "NdotL", "Ambient", "SunColor" };
        static int debugMode = 0;
        if (ImGui::Combo("Terrain Debug", &debugMode, debugItems, IM_ARRAYSIZE(debugItems))) {
            Renderer::SetTerrainDebugMode(debugMode);
        }
    }

    ImGui::End();
}

// ---- Gizmo ----

bool DrawGizmo(SceneObject& object, const Mat4& view, const Mat4& projection,
               GizmoMode mode, float viewportX, float viewportY, float viewportW, float viewportH) {
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewportX, viewportY, viewportW, viewportH);

    Mat4 matrix = object.transform.ToMatrix();

    ImGuizmo::OPERATION op;
    switch (mode) {
        case GizmoMode::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoMode::Rotate:    op = ImGuizmo::ROTATE; break;
        case GizmoMode::Scale:     op = ImGuizmo::SCALE; break;
        default:                   op = ImGuizmo::TRANSLATE; break;
    }

    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        op,
        ImGuizmo::LOCAL,
        glm::value_ptr(matrix)
    );

    if (manipulated) {
        object.transform.FromMatrix(matrix);
    }

    return manipulated;
}

// ---- Asset Browser (Icon Grid Layout) ----

// Helper: detect .passet sub-type by reading the file header
static PAssetType DetectPAssetType(const std::filesystem::path& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return PAssetType::Texture; // fallback
    PAssetHeader header;
    f.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!f.good() || header.magic != PASSET_MAGIC) return PAssetType::Texture;
    return static_cast<PAssetType>(header.type);
}

// Helper: get file type info
struct AssetTypeInfo {
    const char* icon;      // Text icon displayed in the card
    ImVec4 color;          // Icon background color
    bool isModel;
    bool isTexture;
    bool isDirectory;
};

static AssetTypeInfo GetAssetTypeInfo(const std::string& ext, bool isDir, const std::filesystem::path& filepath = {}) {
    if (isDir) return {"DIR", ImVec4(0.35f, 0.35f, 0.55f, 1.0f), false, false, true};
    if (ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj")
        return {"3D", ImVec4(0.2f, 0.5f, 0.8f, 1.0f), true, false, false};
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr")
        return {"TEX", ImVec4(0.6f, 0.4f, 0.2f, 1.0f), false, true, false};
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".comp")
        return {"SHD", ImVec4(0.5f, 0.3f, 0.6f, 1.0f), false, false, false};
    if (ext == ".pscene")
        return {"SCN", ImVec4(0.3f, 0.6f, 0.3f, 1.0f), false, false, false};
    if (ext == ".passet") {
        auto type = DetectPAssetType(filepath);
        if (type == PAssetType::Model)
            return {"3D", ImVec4(0.1f, 0.6f, 0.9f, 1.0f), true, false, false};
        else
            return {"TEX", ImVec4(0.7f, 0.5f, 0.1f, 1.0f), false, true, false};
    }
    return {"FILE", ImVec4(0.4f, 0.4f, 0.4f, 1.0f), false, false, false};
}

void DrawAssetBrowser(std::string& importPath) {
    ImGui::Begin("Asset Browser");

    static std::filesystem::path currentDir = "assets";
    importPath.clear();

    // Navigation bar
    if (currentDir != std::filesystem::path("assets")) {
        if (ImGui::Button("<- Back")) {
            currentDir = currentDir.parent_path();
        }
        ImGui::SameLine();
    }

    // Import buttons
    if (ImGui::Button("Import Model...")) {
        nfdu8filteritem_t filters[] = {{"3D Models", "glb,gltf,fbx,obj"}};
        nfdu8char_t* outPath = nullptr;
        if (NFD_OpenDialogU8(&outPath, filters, 1, nullptr) == NFD_OKAY && outPath) {
            std::string result = AssetImporter::ImportModelToProject(outPath);
            if (!result.empty()) {
                PULUO_CORE_INFO("Imported model to project: {}", result);
            }
            NFD_FreePathU8(outPath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Import Texture...")) {
        nfdu8filteritem_t filters[] = {{"Textures", "png,jpg,jpeg,tga,bmp,hdr"}};
        nfdu8char_t* outPath = nullptr;
        if (NFD_OpenDialogU8(&outPath, filters, 1, nullptr) == NFD_OKAY && outPath) {
            std::string result = AssetImporter::ImportTextureToProject(outPath);
            if (!result.empty()) {
                PULUO_CORE_INFO("Imported texture to project: {}", result);
            }
            NFD_FreePathU8(outPath);
        }
    }

    ImGui::SameLine();
    ImGui::Text("Path: %s", currentDir.string().c_str());
    ImGui::Separator();

    if (!std::filesystem::exists(currentDir) || !std::filesystem::is_directory(currentDir)) {
        ImGui::TextDisabled("Directory not found");
        ImGui::End();
        return;
    }

    // Collect entries: directories first, then files
    struct BrowserEntry {
        std::filesystem::path path;
        std::string name;
        std::string extLower;
        bool isDir;
    };
    std::vector<BrowserEntry> entries;

    for (auto& entry : std::filesystem::directory_iterator(currentDir)) {
        BrowserEntry e;
        e.path = entry.path();
        e.name = entry.path().filename().string();
        e.isDir = entry.is_directory();
        e.extLower = entry.path().extension().string();
        for (auto& c : e.extLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        entries.push_back(std::move(e));
    }

    // Sort: directories first, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const BrowserEntry& a, const BrowserEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    // Grid layout parameters
    const float cardWidth = 80.0f;
    const float cardHeight = 100.0f;
    const float iconHeight = 55.0f;
    const float padding = 8.0f;
    const float cellWidth = cardWidth + padding;
    const float cellHeight = cardHeight + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(panelWidth / cellWidth);
    if (columns < 1) columns = 1;

    ImGui::BeginChild("AssetGrid", ImVec2(0, 0), false, ImGuiWindowFlags_None);

    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < entries.size(); i++) {
        auto& e = entries[i];
        auto typeInfo = GetAssetTypeInfo(e.extLower, e.isDir, e.path);

        int col = static_cast<int>(i) % columns;
        int row = static_cast<int>(i) / columns;

        ImVec2 cardMin = ImVec2(startPos.x + col * cellWidth, startPos.y + row * cellHeight);
        ImVec2 cardMax = ImVec2(cardMin.x + cardWidth, cardMin.y + cardHeight);
        ImVec2 iconMax = ImVec2(cardMin.x + cardWidth, cardMin.y + iconHeight);

        // Invisible button for interaction
        ImGui::SetCursorScreenPos(cardMin);
        ImGui::PushID(static_cast<int>(i));
        bool clicked = ImGui::InvisibleButton("##card", ImVec2(cardWidth, cardHeight));
        bool hovered = ImGui::IsItemHovered();
        bool dblClicked = ImGui::IsMouseDoubleClicked(0) && hovered;

        // Card background
        ImU32 bgColor = hovered
            ? IM_COL32(60, 65, 80, 255)
            : IM_COL32(40, 42, 50, 255);
        drawList->AddRectFilled(cardMin, cardMax, bgColor, 4.0f);

        // Icon area
        ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(typeInfo.color);
        drawList->AddRectFilled(cardMin, iconMax, iconColor, 4.0f, ImDrawFlags_RoundCornersTop);

        // Icon text centered in icon area
        ImVec2 textSize = ImGui::CalcTextSize(typeInfo.icon);
        ImVec2 textPos = ImVec2(
            cardMin.x + (cardWidth - textSize.x) * 0.5f,
            cardMin.y + (iconHeight - textSize.y) * 0.5f
        );
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 230), typeInfo.icon);

        // Filename below icon (truncated to fit)
        float nameAreaHeight = cardHeight - iconHeight;
        const char* nameStr = e.name.c_str();
        ImVec2 nameSize = ImGui::CalcTextSize(nameStr);
        float maxTextWidth = cardWidth - 4.0f;

        // Truncate name if too long
        std::string displayName = e.name;
        if (nameSize.x > maxTextWidth) {
            while (displayName.size() > 3) {
                displayName.pop_back();
                std::string test = displayName + "..";
                if (ImGui::CalcTextSize(test.c_str()).x <= maxTextWidth) {
                    displayName = test;
                    break;
                }
            }
        }
        ImVec2 dNameSize = ImGui::CalcTextSize(displayName.c_str());
        ImVec2 namePos = ImVec2(
            cardMin.x + (cardWidth - dNameSize.x) * 0.5f,
            cardMin.y + iconHeight + (nameAreaHeight - dNameSize.y) * 0.5f
        );
        drawList->AddText(namePos, IM_COL32(200, 200, 200, 255), displayName.c_str());

        // Hover border
        if (hovered) {
            drawList->AddRect(cardMin, cardMax, IM_COL32(100, 140, 200, 200), 4.0f, 0, 1.5f);
        }

        // Tooltip for full name
        if (hovered && e.name != displayName) {
            ImGui::SetTooltip("%s", e.name.c_str());
        }

        // Handle interaction
        if (dblClicked) {
            if (e.isDir) {
                currentDir = e.path;
            } else if (typeInfo.isModel) {
                importPath = e.path.string();
            }
        }

        ImGui::PopID();
    }

    // Reserve space so scrolling works
    if (!entries.empty()) {
        int totalRows = (static_cast<int>(entries.size()) + columns - 1) / columns;
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + totalRows * cellHeight));
        ImGui::Dummy(ImVec2(0, 0));
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace Puluo
