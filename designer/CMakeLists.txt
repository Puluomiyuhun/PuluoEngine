add_executable(PuluoDesigner
    src/main.cpp
    src/graph/Graph.cpp
    src/graph/Evaluator.cpp
    src/gpu/Gpu.cpp
    src/ui/SdTheme.cpp
    src/ui/NodeCanvas.cpp
    src/preview/PreviewScene.cpp
)

# Consumes the engine; the engine has no knowledge of this target.
target_link_libraries(PuluoDesigner PRIVATE
    Puluo::Engine
    imgui
)

set_target_properties(PuluoDesigner PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:PuluoDesigner>
)

# Shares the engine's asset tree: pbr.vert/frag, skybox, environment.hdr and
# the texgraph compute kernels all live under assets/.
add_custom_command(TARGET PuluoDesigner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:PuluoDesigner>/assets
)
