#pragma once

#include "puluo/renderer/Model.h"
#include "puluo/renderer/Texture2D.h"
#include <string>
#include <memory>

namespace Puluo {

class AssetLoader {
public:
    // Load a Model from a .passet binary file.
    // Returns nullptr on failure.
    static std::shared_ptr<Model> LoadModel(const std::string& passetPath);

    // Load a standalone Texture2D from a .passet binary file.
    // Returns nullptr on failure.
    static std::shared_ptr<Texture2D> LoadTexture(const std::string& passetPath);

private:
    // Create a Texture2D from raw decoded pixel data (friend access to Texture2D)
    static std::shared_ptr<Texture2D> CreateTextureFromRaw(
        int width, int height, int channels, const unsigned char* pixels);
};

} // namespace Puluo
