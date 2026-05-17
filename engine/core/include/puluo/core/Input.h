#pragma once

#include "puluo/core/KeyCodes.h"
#include "puluo/core/Math.h"

namespace Puluo {

class Input {
public:
    static bool IsKeyPressed(Key key);
    static bool IsMouseButtonPressed(MouseButton button);
    static Vec2 GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();
};

} // namespace Puluo
