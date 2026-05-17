#pragma once

#include "puluo/core/KeyCodes.h"
#include <string>
#include <functional>

namespace Puluo {

enum class EventType {
    None = 0,
    WindowClose, WindowResize, WindowFocus, WindowLostFocus,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

enum EventCategory : uint8_t {
    EventCategory_None        = 0,
    EventCategory_Application = 1 << 0,
    EventCategory_Input       = 1 << 1,
    EventCategory_Keyboard    = 1 << 2,
    EventCategory_Mouse       = 1 << 3,
    EventCategory_MouseButton = 1 << 4
};

class Event {
public:
    virtual ~Event() = default;
    virtual EventType GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual uint8_t GetCategoryFlags() const = 0;

    bool IsInCategory(EventCategory category) const {
        return GetCategoryFlags() & category;
    }

    bool handled = false;
};

// Helper macro to reduce boilerplate
#define PULUO_EVENT_CLASS(type, categories) \
    static EventType GetStaticType() { return EventType::type; } \
    EventType GetType() const override { return GetStaticType(); } \
    const char* GetName() const override { return #type; } \
    uint8_t GetCategoryFlags() const override { return categories; }

// ---- Window Events ----

class WindowCloseEvent : public Event {
public:
    PULUO_EVENT_CLASS(WindowClose, EventCategory_Application)
};

class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(int width, int height) : m_Width(width), m_Height(height) {}
    PULUO_EVENT_CLASS(WindowResize, EventCategory_Application)

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    int m_Width, m_Height;
};

// ---- Key Events ----

class KeyEvent : public Event {
public:
    Key GetKeyCode() const { return m_KeyCode; }
protected:
    KeyEvent(Key keyCode) : m_KeyCode(keyCode) {}
    Key m_KeyCode;
};

class KeyPressedEvent : public KeyEvent {
public:
    KeyPressedEvent(Key keyCode, bool isRepeat = false)
        : KeyEvent(keyCode), m_IsRepeat(isRepeat) {}
    PULUO_EVENT_CLASS(KeyPressed, EventCategory_Input | EventCategory_Keyboard)

    bool IsRepeat() const { return m_IsRepeat; }

private:
    bool m_IsRepeat;
};

class KeyReleasedEvent : public KeyEvent {
public:
    KeyReleasedEvent(Key keyCode) : KeyEvent(keyCode) {}
    PULUO_EVENT_CLASS(KeyReleased, EventCategory_Input | EventCategory_Keyboard)
};

// ---- Mouse Events ----

class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}
    PULUO_EVENT_CLASS(MouseMoved, EventCategory_Input | EventCategory_Mouse)

    float GetX() const { return m_X; }
    float GetY() const { return m_Y; }

private:
    float m_X, m_Y;
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float xOffset, float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}
    PULUO_EVENT_CLASS(MouseScrolled, EventCategory_Input | EventCategory_Mouse)

    float GetXOffset() const { return m_XOffset; }
    float GetYOffset() const { return m_YOffset; }

private:
    float m_XOffset, m_YOffset;
};

class MouseButtonEvent : public Event {
public:
    MouseButton GetButton() const { return m_Button; }
protected:
    MouseButtonEvent(MouseButton button) : m_Button(button) {}
    MouseButton m_Button;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
    MouseButtonPressedEvent(MouseButton button) : MouseButtonEvent(button) {}
    PULUO_EVENT_CLASS(MouseButtonPressed, EventCategory_Input | EventCategory_Mouse | EventCategory_MouseButton)
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    MouseButtonReleasedEvent(MouseButton button) : MouseButtonEvent(button) {}
    PULUO_EVENT_CLASS(MouseButtonReleased, EventCategory_Input | EventCategory_Mouse | EventCategory_MouseButton)
};

// ---- Event Dispatcher (type-safe dispatch) ----

class EventDispatcher {
public:
    EventDispatcher(Event& event) : m_Event(event) {}

    template<typename T, typename F>
    bool Dispatch(const F& func) {
        if (m_Event.GetType() == T::GetStaticType()) {
            m_Event.handled |= func(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }

private:
    Event& m_Event;
};

// Callback type used by Window
using EventCallbackFn = std::function<void(Event&)>;

} // namespace Puluo
