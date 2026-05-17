add_library(PuluoCore STATIC
    src/Log.cpp
    src/Application.cpp
)

target_include_directories(PuluoCore PUBLIC include)

target_link_libraries(PuluoCore PUBLIC
    spdlog::spdlog
    glm::glm
    nlohmann_json::nlohmann_json
)

target_compile_features(PuluoCore PUBLIC cxx_std_20)

add_library(Puluo::Core ALIAS PuluoCore)
