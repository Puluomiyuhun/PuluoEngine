#pragma once

#include "puluo/core/Scene.h"
#include <memory>
#include <vector>

namespace Puluo {

class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
};

class CommandHistory {
public:
    void Execute(std::unique_ptr<Command> cmd) {
        cmd->Execute();
        m_UndoStack.push_back(std::move(cmd));
        m_RedoStack.clear();
    }

    // Push an already-executed command (e.g., gizmo manipulation applied by ImGuizmo)
    void PushExecuted(std::unique_ptr<Command> cmd) {
        m_UndoStack.push_back(std::move(cmd));
        m_RedoStack.clear();
    }

    void Undo() {
        if (m_UndoStack.empty()) return;
        auto cmd = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        cmd->Undo();
        m_RedoStack.push_back(std::move(cmd));
    }

    void Redo() {
        if (m_RedoStack.empty()) return;
        auto cmd = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        cmd->Execute();
        m_UndoStack.push_back(std::move(cmd));
    }

    bool CanUndo() const { return !m_UndoStack.empty(); }
    bool CanRedo() const { return !m_RedoStack.empty(); }
    void Clear() { m_UndoStack.clear(); m_RedoStack.clear(); }

private:
    std::vector<std::unique_ptr<Command>> m_UndoStack;
    std::vector<std::unique_ptr<Command>> m_RedoStack;
};

// ---- Concrete Commands (use index to locate objects, safe against vector reallocation) ----

class TransformChangeCommand : public Command {
public:
    TransformChangeCommand(Scene& scene, size_t index, const Transform& oldT, const Transform& newT)
        : m_Scene(scene), m_Index(index), m_OldTransform(oldT), m_NewTransform(newT) {}

    void Execute() override {
        auto& objects = m_Scene.GetObjects();
        if (m_Index < objects.size()) objects[m_Index].transform = m_NewTransform;
    }
    void Undo() override {
        auto& objects = m_Scene.GetObjects();
        if (m_Index < objects.size()) objects[m_Index].transform = m_OldTransform;
    }

private:
    Scene& m_Scene;
    size_t m_Index;
    Transform m_OldTransform, m_NewTransform;
};

class RenameCommand : public Command {
public:
    RenameCommand(Scene& scene, size_t index, const std::string& oldName, const std::string& newName)
        : m_Scene(scene), m_Index(index), m_OldName(oldName), m_NewName(newName) {}

    void Execute() override {
        auto& objects = m_Scene.GetObjects();
        if (m_Index < objects.size()) objects[m_Index].name = m_NewName;
    }
    void Undo() override {
        auto& objects = m_Scene.GetObjects();
        if (m_Index < objects.size()) objects[m_Index].name = m_OldName;
    }

private:
    Scene& m_Scene;
    size_t m_Index;
    std::string m_OldName, m_NewName;
};

class AddObjectCommand : public Command {
public:
    AddObjectCommand(Scene& scene, const std::string& name, std::shared_ptr<Model> model, const std::string& modelPath)
        : m_Scene(scene), m_Name(name), m_Model(model), m_ModelPath(modelPath) {}

    void Execute() override {
        auto& obj = m_Scene.AddObject(m_Name, m_Model);
        obj.modelPath = m_ModelPath;
        m_AddedIndex = m_Scene.GetObjects().size() - 1;
    }
    void Undo() override {
        m_Scene.RemoveObject(m_AddedIndex);
    }

private:
    Scene& m_Scene;
    std::string m_Name, m_ModelPath;
    std::shared_ptr<Model> m_Model;
    size_t m_AddedIndex = 0;
};

class DeleteObjectCommand : public Command {
public:
    DeleteObjectCommand(Scene& scene, size_t index)
        : m_Scene(scene), m_Index(index) {
        auto& obj = scene.GetObjects()[index];
        m_SavedName = obj.name;
        m_SavedModelPath = obj.modelPath;
        m_SavedModel = obj.model;
        m_SavedTransform = obj.transform;
        m_SavedLight = obj.light;
    }

    void Execute() override { m_Scene.RemoveObject(m_Index); }
    void Undo() override {
        SceneObject restored;
        restored.name = m_SavedName;
        restored.modelPath = m_SavedModelPath;
        restored.model = m_SavedModel;
        restored.transform = m_SavedTransform;
        restored.light = m_SavedLight;
        m_Scene.InsertObject(m_Index, std::move(restored));
    }

private:
    Scene& m_Scene;
    size_t m_Index;
    std::string m_SavedName, m_SavedModelPath;
    std::shared_ptr<Model> m_SavedModel;
    Transform m_SavedTransform;
    std::optional<SceneLightData> m_SavedLight;
};

} // namespace Puluo
