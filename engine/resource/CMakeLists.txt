add_library(PuluoResource STATIC
    src/TextureCache.cpp
    src/ModelCache.cpp
    src/AssetImporter.cpp
    src/AssetLoader.cpp
)

target_include_directories(PuluoResource PUBLIC include)
# AssetImporter needs stb_image.h from vendor and Assimp headers
target_include_directories(PuluoResource PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)

target_link_libraries(PuluoResource PUBLIC
    Puluo::Renderer
)

add_library(Puluo::Resource ALIAS PuluoResource)
