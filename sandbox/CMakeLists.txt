add_executable(PuluoSandbox
    src/SandboxApp.cpp
    src/EditorPanels.cpp
)

target_link_libraries(PuluoSandbox PRIVATE
    Puluo::Engine
    imgui
    imguizmo
    nfd
)

# Set working directory to exe output dir (so relative paths like "assets/" work in VS/Rider)
set_target_properties(PuluoSandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:PuluoSandbox>
)

# Copy assets to build directory
add_custom_command(TARGET PuluoSandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:PuluoSandbox>/assets
)
