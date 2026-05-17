# Engine sub-modules
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(renderer)
add_subdirectory(resource)

# Aggregate interface library
add_library(PuluoEngine INTERFACE)
target_link_libraries(PuluoEngine INTERFACE
    Puluo::Core
    Puluo::Platform
    Puluo::Renderer
    Puluo::Resource
)
add_library(Puluo::Engine ALIAS PuluoEngine)
