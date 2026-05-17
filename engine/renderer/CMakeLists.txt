add_library(PuluoRenderer STATIC
    src/RenderCommand.cpp
    src/Buffer.cpp
    src/VertexArray.cpp
    src/Shader.cpp
    src/Texture2D.cpp
    src/Framebuffer.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/Material.cpp
    src/Mesh.cpp
    src/Light.cpp
    src/Model.cpp
    src/Cubemap.cpp
    src/IBL.cpp
    src/SkyLight.cpp
    src/Terrain.cpp
    src/CascadedShadowMap.cpp
    src/SSAO.cpp
    src/ParticleSystem.cpp
    src/WeatherSystem.cpp
    src/InstancedMesh.cpp
    src/SSR.cpp
    src/ShadowPass.cpp
    src/DepthPrepass.cpp
    src/SSAOPass.cpp
    src/PostProcessPass.cpp
    src/stb_image_impl.cpp
    src/stb_image_write_impl.cpp
)

target_include_directories(PuluoRenderer PUBLIC include)
target_include_directories(PuluoRenderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)
# Allow renderer source files to include resource headers (TextureCache, ModelCache)
# The actual resource library is linked at the PuluoEngine aggregate level
target_include_directories(PuluoRenderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../resource/include)

target_link_libraries(PuluoRenderer PUBLIC
    Puluo::Core
    glad_gl
    assimp::assimp
)

add_library(Puluo::Renderer ALIAS PuluoRenderer)
