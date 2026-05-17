add_library(PuluoPlatform STATIC
    src/GlfwWindow.cpp
    src/GlfwInput.cpp
    src/ImGuiLayer.cpp
)

target_include_directories(PuluoPlatform PUBLIC include)

target_link_libraries(PuluoPlatform PUBLIC
    Puluo::Core
    glfw
    glad_gl
    imgui
    imguizmo
)

add_library(Puluo::Platform ALIAS PuluoPlatform)
