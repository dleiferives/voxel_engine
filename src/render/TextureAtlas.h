#pragma once

#include "../core/Common.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

class TextureAtlas {
public:
    GLuint textureArray = 0;
    int tileSize = 16;
    int layerCount = 0;
    std::unordered_map<std::string, int> textureIndices;

    void init(int tileResolution) {
        tileSize = tileResolution;
    }

    int addTexture(const std::string& path) {
        auto it = textureIndices.find(path);
        if (it != textureIndices.end()) {
            return it->second;
        }
        int index = layerCount++;
        textureIndices[path] = index;
        return index;
    }

    int addColor(const glm::vec3& color) {
        std::string key = "color_" + std::to_string(color.r) + "_" +
                          std::to_string(color.g) + "_" + std::to_string(color.b);
        auto it = textureIndices.find(key);
        if (it != textureIndices.end()) {
            return it->second;
        }
        int index = layerCount++;
        textureIndices[key] = index;
        return index;
    }

    void build() {
        if (layerCount == 0) return;

        glGenTextures(1, &textureArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, tileSize, tileSize, layerCount);

        for (auto& [key, index] : textureIndices) {
            if (key.rfind("color_", 0) == 0) {
                glm::vec3 color;
                sscanf(key.c_str(), "color_%f_%f_%f", &color.r, &color.g, &color.b);

                std::vector<uint8_t> colorPixels(tileSize * tileSize * 4);
                for (int i = 0; i < tileSize * tileSize; i++) {
                    colorPixels[i * 4 + 0] = static_cast<uint8_t>(color.r * 255);
                    colorPixels[i * 4 + 1] = static_cast<uint8_t>(color.g * 255);
                    colorPixels[i * 4 + 2] = static_cast<uint8_t>(color.b * 255);
                    colorPixels[i * 4 + 3] = 255;
                }
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                               tileSize, tileSize, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                               colorPixels.data());
            } else {
                int width, height, channels;
                stbi_set_flip_vertically_on_load(true);
                uint8_t* data = stbi_load(key.c_str(), &width, &height, &channels, 4);

                if (data && width == tileSize && height == tileSize) {
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                                   tileSize, tileSize, 1, GL_RGBA,
                                   GL_UNSIGNED_BYTE, data);
                } else {
                    std::vector<uint8_t> errorPixels(tileSize * tileSize * 4);
                    for (int i = 0; i < tileSize * tileSize; i++) {
                        bool checker = ((i % tileSize) / 4 + (i / tileSize) / 4) % 2;
                        errorPixels[i * 4 + 0] = checker ? 255 : 0;
                        errorPixels[i * 4 + 1] = 0;
                        errorPixels[i * 4 + 2] = checker ? 255 : 0;
                        errorPixels[i * 4 + 3] = 255;
                    }
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                                   tileSize, tileSize, 1, GL_RGBA,
                                   GL_UNSIGNED_BYTE, errorPixels.data());
                    std::cerr << "Failed to load texture: " << key << std::endl;
                }

                if (data) stbi_image_free(data);
            }
        }

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    }

    void bind(int unit = 0) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    }

    void cleanup() {
        if (textureArray) glDeleteTextures(1, &textureArray);
    }
};

// Global texture atlas (defined in main.cpp)
extern TextureAtlas g_textureAtlas;
