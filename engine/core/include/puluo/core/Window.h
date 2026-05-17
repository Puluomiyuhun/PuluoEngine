#pragma once

#include "puluo/core/Event.h"

#include <string>
#include <memory>
#include <functional>

namespace Puluo {

struct WindowProps {
    std::string title = "PuluoEngine";
    int width  = 1280;
    int height = 720;
};

class Window {
public:
    virtual ~Window() = default;

    virtual void OnUpdate() = 0;
    virtual void SwapBuffers() = 0;

    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual bool ShouldClose() const = 0;

    virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
    virtual void SetCursorMode(bool enabled) = 0;

    virtual void* GetNativeWindow() const = 0;

    static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps());
};

} // namespace Puluo
