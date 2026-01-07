#include <iostream>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstring>
#include <fstream>
#include <algorithm>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Embedded shaders
const char chunkVertSource[] = {
#embed "shaders/chunk.vert"
    , 0
};
const char chunkFragSource[] = {
#embed "shaders/chunk.frag"
    , 0
};
const char modelVertSource[] = {
#embed "shaders/model.vert"
    , 0
};
const char modelFragSource[] = {
#embed "shaders/model.frag"
    , 0
};
const char cullingCompSource[] = {
#embed "shaders/culling.comp"
    , 0
};

// Constants
constexpr int CHUNK_SIZE = 32;
constexpr int MICRO_BLOCK_SIZE = 16;
constexpr int MICRO_PALETTE_SIZE = 32;
constexpr size_t MAX_MODEL_INSTANCES = 1000000;
constexpr int MAX_ATLAS_SIZE = 4096;

// Camera
struct Camera {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
    float yaw;
    float pitch;
    float sensitivity;
    float speed;
    float render_distance;
};

struct Manager {
    float lastX;
    float lastY;
    float delta_time;
    float last_frame;
    bool ui_mode;
    Camera camera;
};

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

bool first_mouse = true;
Manager g_manager = {
    .lastX = SCR_WIDTH / 2.0f,
    .lastY = SCR_HEIGHT / 2.0f,
    .delta_time = 0.0f,
    .last_frame = 0.0f,
    .ui_mode = false,
    .camera = {
        .pos = glm::vec3(48.0f, 48.0f, 48.0f),
        .front = glm::vec3(0.0f, 0.0f, -1.0f),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
        .yaw = -135.0f,
        .pitch = -30.0f,
        .sensitivity = 0.1f,
        .speed = 50.0f,
        .render_distance = 500.0f,
    },
};

// Block classification
enum BlockCategory : uint8_t {
    CATEGORY_AIR = 0,
    CATEGORY_SOLID,  // Greedy meshed with textures
    CATEGORY_MODEL,  // GPU culled instances
    CATEGORY_MICRO,  // 16x16x16 micro blocks
};

// Block types
enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    // Solid blocks (textured, greedy meshed)
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_GLASS,
    BLOCK_BRICK,
    // Model blocks (GPU culled)
    BLOCK_FLOWER_RED,
    BLOCK_FLOWER_YELLOW,
    BLOCK_TALL_GRASS,
    BLOCK_MUSHROOM,
    BLOCK_CRYSTAL,
    // Micro blocks
    BLOCK_MICRO_TERRAIN,
    BLOCK_MICRO_SCULPTURE,
    BLOCK_COUNT
};

// Face directions
enum Face {
    FACE_NEG_X = 0,
    FACE_POS_X,
    FACE_NEG_Y,
    FACE_POS_Y,
    FACE_NEG_Z,
    FACE_POS_Z
};

const glm::vec3 FACE_NORMALS[] = {
    glm::vec3(-1, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0),
    glm::vec3(0, 1, 0),  glm::vec3(0, 0, -1), glm::vec3(0, 0, 1),
};

// Block rotation system (24 possible orientations)
// Rotation is defined by which direction is "up" and which is "forward"
enum BlockOrientation : uint8_t {
    ROT_Y_UP_Z_FWD = 0,    // Default
    ROT_Y_UP_X_FWD,
    ROT_Y_UP_NZ_FWD,
    ROT_Y_UP_NX_FWD,
    ROT_NY_UP_Z_FWD,
    ROT_NY_UP_X_FWD,
    ROT_NY_UP_NZ_FWD,
    ROT_NY_UP_NX_FWD,
    ROT_Z_UP_Y_FWD,
    ROT_Z_UP_X_FWD,
    ROT_Z_UP_NY_FWD,
    ROT_Z_UP_NX_FWD,
    ROT_NZ_UP_Y_FWD,
    ROT_NZ_UP_X_FWD,
    ROT_NZ_UP_NY_FWD,
    ROT_NZ_UP_NX_FWD,
    ROT_X_UP_Y_FWD,
    ROT_X_UP_Z_FWD,
    ROT_X_UP_NY_FWD,
    ROT_X_UP_NZ_FWD,
    ROT_NX_UP_Y_FWD,
    ROT_NX_UP_Z_FWD,
    ROT_NX_UP_NY_FWD,
    ROT_NX_UP_NZ_FWD,
    ROT_COUNT
};

// Face remapping for each rotation
// Maps: rotatedFace -> originalFace
const uint8_t ROTATION_FACE_MAP[24][6] = {
    {0, 1, 2, 3, 4, 5}, // ROT_Y_UP_Z_FWD (default)
    {4, 5, 2, 3, 1, 0}, // ROT_Y_UP_X_FWD
    {1, 0, 2, 3, 5, 4}, // ROT_Y_UP_NZ_FWD
    {5, 4, 2, 3, 0, 1}, // ROT_Y_UP_NX_FWD
    {0, 1, 3, 2, 5, 4}, // ROT_NY_UP_Z_FWD
    {5, 4, 3, 2, 1, 0}, // ROT_NY_UP_X_FWD
    {1, 0, 3, 2, 4, 5}, // ROT_NY_UP_NZ_FWD
    {4, 5, 3, 2, 0, 1}, // ROT_NY_UP_NX_FWD
    {0, 1, 4, 5, 3, 2}, // ROT_Z_UP_Y_FWD
    {2, 3, 4, 5, 1, 0}, // ROT_Z_UP_X_FWD
    {1, 0, 4, 5, 2, 3}, // ROT_Z_UP_NY_FWD
    {3, 2, 4, 5, 0, 1}, // ROT_Z_UP_NX_FWD
    {0, 1, 5, 4, 2, 3}, // ROT_NZ_UP_Y_FWD
    {3, 2, 5, 4, 1, 0}, // ROT_NZ_UP_X_FWD
    {1, 0, 5, 4, 3, 2}, // ROT_NZ_UP_NY_FWD
    {2, 3, 5, 4, 0, 1}, // ROT_NZ_UP_NX_FWD
    {2, 3, 0, 1, 4, 5}, // ROT_X_UP_Y_FWD
    {4, 5, 0, 1, 3, 2}, // ROT_X_UP_Z_FWD
    {3, 2, 0, 1, 5, 4}, // ROT_X_UP_NY_FWD
    {5, 4, 0, 1, 2, 3}, // ROT_X_UP_NZ_FWD
    {3, 2, 1, 0, 4, 5}, // ROT_NX_UP_Y_FWD
    {5, 4, 1, 0, 2, 3}, // ROT_NX_UP_Z_FWD
    {2, 3, 1, 0, 5, 4}, // ROT_NX_UP_NY_FWD
    {4, 5, 1, 0, 3, 2}, // ROT_NX_UP_NZ_FWD
};

// UV rotation per face for each block rotation (0-3 = 0°, 90°, 180°, 270°)
const uint8_t ROTATION_UV_ROT[24][6] = {
    {0, 0, 0, 0, 0, 0}, {1, 3, 1, 3, 0, 0}, {2, 2, 2, 2, 0, 0}, {3, 1, 3, 1, 0, 0},
    {0, 0, 0, 0, 2, 2}, {3, 1, 1, 3, 2, 2}, {2, 2, 2, 2, 2, 2}, {1, 3, 3, 1, 2, 2},
    {0, 0, 0, 0, 1, 3}, {0, 0, 1, 3, 1, 3}, {0, 0, 2, 2, 1, 3}, {0, 0, 3, 1, 1, 3},
    {0, 0, 0, 0, 3, 1}, {0, 0, 3, 1, 3, 1}, {0, 0, 2, 2, 3, 1}, {0, 0, 1, 3, 3, 1},
    {1, 3, 0, 0, 1, 3}, {1, 3, 1, 3, 1, 3}, {1, 3, 2, 2, 1, 3}, {1, 3, 3, 1, 1, 3},
    {3, 1, 0, 0, 3, 1}, {3, 1, 3, 1, 3, 1}, {3, 1, 2, 2, 3, 1}, {3, 1, 1, 3, 3, 1},
};

// Texture face specification
struct TextureFace {
    uint16_t textureId;  // Index into texture atlas
    uint8_t uvRotation;  // 0-3 for 90° increments
};

// Block texture definition
struct BlockTextureInfo {
    TextureFace faces[6];  // Per-face texture
    bool transparent;
    bool fullBright;  // Ignore lighting
};

// Block info with texture support
struct BlockInfo {
    BlockCategory category;
    glm::vec3 color;       // Fallback color / model color
    float scale;           // For model blocks
    uint32_t modelId;      // For model blocks
    BlockTextureInfo* textureInfo;  // For solid blocks
};

// Micro block data (16x16x16 with 32 colors)
struct MicroBlockData {
    uint8_t voxels[MICRO_BLOCK_SIZE][MICRO_BLOCK_SIZE][MICRO_BLOCK_SIZE];

    MicroBlockData() {
        memset(voxels, 0, sizeof(voxels));
    }

    uint8_t get(int x, int y, int z) const {
        if (x < 0 || x >= MICRO_BLOCK_SIZE ||
            y < 0 || y >= MICRO_BLOCK_SIZE ||
            z < 0 || z >= MICRO_BLOCK_SIZE)
            return 0;
        return voxels[z][y][x];
    }

    void set(int x, int y, int z, uint8_t colorIndex) {
        if (x < 0 || x >= MICRO_BLOCK_SIZE ||
            y < 0 || y >= MICRO_BLOCK_SIZE ||
            z < 0 || z >= MICRO_BLOCK_SIZE)
            return;
        voxels[z][y][x] = colorIndex;
    }
};

// Micro block palette
struct MicroBlockPalette {
    glm::vec3 colors[MICRO_PALETTE_SIZE] = {
        glm::vec3(0.0f),                    // 0: Air/transparent
        glm::vec3(1.0f, 1.0f, 1.0f),        // 1: White
        glm::vec3(0.8f, 0.8f, 0.8f),        // 2: Light gray
        glm::vec3(0.5f, 0.5f, 0.5f),        // 3: Gray
        glm::vec3(0.2f, 0.2f, 0.2f),        // 4: Dark gray
        glm::vec3(0.0f, 0.0f, 0.0f),        // 5: Black
        glm::vec3(1.0f, 0.0f, 0.0f),        // 6: Red
        glm::vec3(0.0f, 1.0f, 0.0f),        // 7: Green
        glm::vec3(0.0f, 0.0f, 1.0f),        // 8: Blue
        glm::vec3(1.0f, 1.0f, 0.0f),        // 9: Yellow
        glm::vec3(1.0f, 0.0f, 1.0f),        // 10: Magenta
        glm::vec3(0.0f, 1.0f, 1.0f),        // 11: Cyan
        glm::vec3(1.0f, 0.5f, 0.0f),        // 12: Orange
        glm::vec3(0.5f, 0.0f, 1.0f),        // 13: Purple
        glm::vec3(1.0f, 0.75f, 0.8f),       // 14: Pink
        glm::vec3(0.6f, 0.3f, 0.0f),        // 15: Brown
        glm::vec3(0.5f, 0.5f, 0.0f),        // 16: Olive
        glm::vec3(0.0f, 0.5f, 0.5f),        // 17: Teal
        glm::vec3(0.0f, 0.0f, 0.5f),        // 18: Navy
        glm::vec3(0.5f, 0.0f, 0.0f),        // 19: Maroon
        glm::vec3(0.9f, 0.9f, 0.6f),        // 20: Cream
        glm::vec3(0.8f, 0.6f, 0.4f),        // 21: Tan
        glm::vec3(0.4f, 0.2f, 0.1f),        // 22: Dark brown
        glm::vec3(0.6f, 0.8f, 0.2f),        // 23: Lime
        glm::vec3(0.2f, 0.4f, 0.2f),        // 24: Dark green
        glm::vec3(0.4f, 0.6f, 0.8f),        // 25: Sky blue
        glm::vec3(0.8f, 0.4f, 0.4f),        // 26: Salmon
        glm::vec3(0.6f, 0.4f, 0.8f),        // 27: Lavender
        glm::vec3(0.4f, 0.8f, 0.6f),        // 28: Mint
        glm::vec3(0.8f, 0.8f, 0.4f),        // 29: Khaki
        glm::vec3(0.6f, 0.6f, 0.8f),        // 30: Periwinkle
        glm::vec3(0.8f, 0.6f, 0.6f),        // 31: Rose
    };
};

// Global palette
MicroBlockPalette g_microPalette;

// Vertex structures
struct ChunkVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    float textureLayer;  // For texture array
    float ao;            // Ambient occlusion
};

// Model instance data for GPU culling
struct ModelInstance {
    glm::vec4 positionAndScale;
    glm::vec4 colorAndType;
};

// Indirect draw command
struct DrawArraysIndirectCommand {
    GLuint vertexCount;
    GLuint instanceCount;
    GLuint firstVertex;
    GLuint baseInstance;
};

// Texture Atlas System
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
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8,
                       tileSize, tileSize, layerCount);

        std::vector<uint8_t> defaultPixels(tileSize * tileSize * 4, 255);

        for (auto& [key, index] : textureIndices) {
            if (key.rfind("color_", 0) == 0) {
                // Parse color from key
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
                // Load from file
                int width, height, channels;
                stbi_set_flip_vertically_on_load(true);
                uint8_t* data = stbi_load(key.c_str(), &width, &height,
                                          &channels, 4);

                if (data && width == tileSize && height == tileSize) {
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                                   tileSize, tileSize, 1, GL_RGBA,
                                   GL_UNSIGNED_BYTE, data);
                } else {
                    // Use magenta for missing texture
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
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST_MIPMAP_LINEAR);
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

// Global texture atlas
TextureAtlas g_textureAtlas;

// Block texture definitions
BlockTextureInfo g_blockTextures[BLOCK_COUNT];

void initBlockTextures() {
    g_textureAtlas.init(16);

    // Add textures to atlas
    uint16_t stoneTop = g_textureAtlas.addTexture("textures/stone.png");
    uint16_t stoneSide = stoneTop;
    uint16_t dirtTex = g_textureAtlas.addTexture("textures/dirt.png");
    uint16_t grassTop = g_textureAtlas.addTexture("textures/grass_top.png");
    uint16_t grassSide = g_textureAtlas.addTexture("textures/grass_side.png");
    uint16_t sandTex = g_textureAtlas.addTexture("textures/sand.png");
    uint16_t woodTop = g_textureAtlas.addTexture("textures/wood_top.png");
    uint16_t woodSide = g_textureAtlas.addTexture("textures/wood_side.png");
    uint16_t leavesTex = g_textureAtlas.addTexture("textures/leaves.png");
    uint16_t glassTex = g_textureAtlas.addTexture("textures/glass.png");
    uint16_t brickTex = g_textureAtlas.addTexture("textures/brick.png");

    // Fallback colors if textures don't exist
    uint16_t stoneColor = g_textureAtlas.addColor(glm::vec3(0.5f));
    uint16_t dirtColor = g_textureAtlas.addColor(glm::vec3(0.45f, 0.3f, 0.15f));
    uint16_t grassColor = g_textureAtlas.addColor(glm::vec3(0.2f, 0.6f, 0.2f));
    uint16_t sandColor = g_textureAtlas.addColor(glm::vec3(0.9f, 0.85f, 0.6f));
    uint16_t woodColor = g_textureAtlas.addColor(glm::vec3(0.55f, 0.35f, 0.15f));
    uint16_t leavesColor = g_textureAtlas.addColor(glm::vec3(0.1f, 0.5f, 0.1f));
    uint16_t glassColor = g_textureAtlas.addColor(glm::vec3(0.8f, 0.9f, 1.0f));
    uint16_t brickColor = g_textureAtlas.addColor(glm::vec3(0.7f, 0.3f, 0.2f));

    // Stone - same texture all sides
    g_blockTextures[BLOCK_STONE] = {
        {{stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0},
         {stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0}},
        false, false
    };

    // Dirt - same texture all sides
    g_blockTextures[BLOCK_DIRT] = {
        {{dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0},
         {dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0}},
        false, false
    };

    // Grass - different top/side/bottom
    g_blockTextures[BLOCK_GRASS] = {
        {{grassColor, 0}, {grassColor, 0}, {dirtColor, 0},
         {grassColor, 0}, {grassColor, 0}, {grassColor, 0}},
        false, false
    };

    // Sand
    g_blockTextures[BLOCK_SAND] = {
        {{sandColor, 0}, {sandColor, 0}, {sandColor, 0},
         {sandColor, 0}, {sandColor, 0}, {sandColor, 0}},
        false, false
    };

    // Wood - different top/bottom vs sides
    g_blockTextures[BLOCK_WOOD] = {
        {{woodColor, 0}, {woodColor, 0}, {woodColor, 0},
         {woodColor, 0}, {woodColor, 0}, {woodColor, 0}},
        false, false
    };

    // Leaves - transparent
    g_blockTextures[BLOCK_LEAVES] = {
        {{leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0},
         {leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0}},
        true, false
    };

    // Glass - transparent
    g_blockTextures[BLOCK_GLASS] = {
        {{glassTex, 0}, {glassTex, 0}, {glassTex, 0},
         {glassTex, 0}, {glassTex, 0}, {glassTex, 0}},
        true, false
    };

    // Brick
    g_blockTextures[BLOCK_BRICK] = {
        {{brickColor, 0}, {brickColor, 0}, {brickColor, 0},
         {brickColor, 0}, {brickColor, 0}, {brickColor, 0}},
        false, false
    };

    g_textureAtlas.build();
}

// Block info array
BlockInfo BLOCK_INFO[] = {
    {CATEGORY_AIR, glm::vec3(0.0f), 0.0f, 0, nullptr},
    {CATEGORY_SOLID, glm::vec3(0.5f), 1.0f, 0, &g_blockTextures[BLOCK_STONE]},
    {CATEGORY_SOLID, glm::vec3(0.45f, 0.3f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_DIRT]},
    {CATEGORY_SOLID, glm::vec3(0.2f, 0.6f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_GRASS]},
    {CATEGORY_SOLID, glm::vec3(0.9f, 0.85f, 0.6f), 1.0f, 0, &g_blockTextures[BLOCK_SAND]},
    {CATEGORY_SOLID, glm::vec3(0.55f, 0.35f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_WOOD]},
    {CATEGORY_SOLID, glm::vec3(0.1f, 0.5f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LEAVES]},
    {CATEGORY_SOLID, glm::vec3(0.8f, 0.9f, 1.0f), 1.0f, 0, &g_blockTextures[BLOCK_GLASS]},
    {CATEGORY_SOLID, glm::vec3(0.7f, 0.3f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_BRICK]},
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.2f, 0.2f), 0.4f, 0, nullptr},
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.9f, 0.2f), 0.4f, 0, nullptr},
    {CATEGORY_MODEL, glm::vec3(0.3f, 0.7f, 0.3f), 0.6f, 1, nullptr},
    {CATEGORY_MODEL, glm::vec3(0.8f, 0.7f, 0.6f), 0.3f, 2, nullptr},
    {CATEGORY_MODEL, glm::vec3(0.6f, 0.8f, 1.0f), 0.5f, 3, nullptr},
    {CATEGORY_MICRO, glm::vec3(0.7f, 0.5f, 0.3f), 1.0f, 0, nullptr},
    {CATEGORY_MICRO, glm::vec3(0.8f, 0.8f, 0.8f), 1.0f, 1, nullptr},
};

// Block data with rotation
struct BlockData {
    uint8_t type;
    uint8_t rotation;  // BlockOrientation
    uint16_t microDataIndex;  // Index into micro block data array (if CATEGORY_MICRO)
};

// Chunk class
class Chunk {
public:
    glm::ivec3 chunkPos;
    std::array<BlockData, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> blocks;
    std::vector<MicroBlockData> microBlockData;

    // Opaque mesh
    GLuint VAO = 0, VBO = 0;
    size_t vertexCount = 0;

    // Transparent mesh (rendered after opaque)
    GLuint transparentVAO = 0, transparentVBO = 0;
    size_t transparentVertexCount = 0;

    bool needsRebuild = true;
    bool isEmpty = true;
    bool meshUploaded = false;

    std::vector<ModelInstance> modelBlocks;
    bool modelsDirty = true;

    Chunk(glm::ivec3 pos) : chunkPos(pos) {
        for (auto& block : blocks) {
            block.type = BLOCK_AIR;
            block.rotation = ROT_Y_UP_Z_FWD;
            block.microDataIndex = 0;
        }
    }

    ~Chunk() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (transparentVAO) glDeleteVertexArrays(1, &transparentVAO);
        if (transparentVBO) glDeleteBuffers(1, &transparentVBO);
    }

    inline int index(int x, int y, int z) const {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }

    BlockData getBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return {BLOCK_AIR, ROT_Y_UP_Z_FWD, 0};
        return blocks[index(x, y, z)];
    }

    void setBlock(int x, int y, int z, uint8_t type,
                  uint8_t rotation = ROT_Y_UP_Z_FWD) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return;

        BlockData& block = blocks[index(x, y, z)];
        block.type = type;
        block.rotation = rotation;

        if (BLOCK_INFO[type].category == CATEGORY_MICRO) {
            block.microDataIndex = static_cast<uint16_t>(microBlockData.size());
            microBlockData.push_back(MicroBlockData());
        }

        needsRebuild = true;
        modelsDirty = true;
        isEmpty = false;
    }

    void setMicroBlock(int blockX, int blockY, int blockZ,
                       int microX, int microY, int microZ,
                       uint8_t colorIndex) {
        BlockData& block = blocks[index(blockX, blockY, blockZ)];
        if (BLOCK_INFO[block.type].category != CATEGORY_MICRO) return;

        microBlockData[block.microDataIndex].set(microX, microY, microZ, colorIndex);
        needsRebuild = true;
    }

    MicroBlockData* getMicroData(int x, int y, int z) {
        BlockData& block = blocks[index(x, y, z)];
        if (BLOCK_INFO[block.type].category != CATEGORY_MICRO) return nullptr;
        return &microBlockData[block.microDataIndex];
    }

    glm::vec3 getWorldPos() const {
        return glm::vec3(chunkPos) * float(CHUNK_SIZE);
    }

    void getBoundingBox(glm::vec3& minPos, glm::vec3& maxPos) const {
        minPos = getWorldPos();
        maxPos = minPos + glm::vec3(CHUNK_SIZE);
    }

    void collectModelBlocks() {
        if (!modelsDirty) return;

        modelBlocks.clear();
        glm::vec3 worldBase = getWorldPos();

        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    BlockData blockData = blocks[index(x, y, z)];
                    if (blockData.type == BLOCK_AIR) continue;

                    const BlockInfo& info = BLOCK_INFO[blockData.type];
                    if (info.category != CATEGORY_MODEL) continue;

                    glm::vec3 pos = worldBase +
                                    glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f);

                    ModelInstance inst;
                    inst.positionAndScale = glm::vec4(pos, info.scale);
                    inst.colorAndType = glm::vec4(info.color, float(info.modelId));
                    modelBlocks.push_back(inst);
                }
            }
        }

        modelsDirty = false;
    }
};

// UV rotation helper
glm::vec2 rotateUV(glm::vec2 uv, uint8_t rotation) {
    switch (rotation % 4) {
        case 1: return glm::vec2(uv.y, 1.0f - uv.x);
        case 2: return glm::vec2(1.0f - uv.x, 1.0f - uv.y);
        case 3: return glm::vec2(1.0f - uv.y, uv.x);
        default: return uv;
    }
}

// Enhanced Greedy mesher with textures and micro blocks
class GreedyMesher {
public:
    static void mesh(Chunk& chunk,
                     std::vector<ChunkVertex>& opaqueVerts,
                     std::vector<ChunkVertex>& transparentVerts) {
        opaqueVerts.clear();
        transparentVerts.clear();

        // First pass: regular solid blocks
        for (int face = 0; face < 6; face++) {
            meshFace(chunk, opaqueVerts, transparentVerts, face);
        }

        // Second pass: micro blocks
        meshMicroBlocks(chunk, opaqueVerts);
    }

private:
    struct FaceData {
        uint8_t blockType;
        uint8_t rotation;
        int textureLayer;
        uint8_t uvRotation;
        bool transparent;
    };

static void meshFace(Chunk& chunk,
                     std::vector<ChunkVertex>& opaqueVerts,
                     std::vector<ChunkVertex>& transparentVerts,
                     int face) {
    int axis = face / 2;    // 0:X, 1:Y, 2:Z
    bool positive = face % 2;
    int u = (axis + 1) % 3;
    int v = (axis + 2) % 3;

    glm::ivec3 dir(0);
    dir[axis] = positive ? 1 : -1;
    glm::vec3 normal = FACE_NORMALS[face];

    std::array<FaceData, CHUNK_SIZE * CHUNK_SIZE> mask;

    for (int d = 0; d < CHUNK_SIZE; d++) {
        // 1. Build the mask for this slice
        for (int j = 0; j < CHUNK_SIZE; j++) {
            for (int i = 0; i < CHUNK_SIZE; i++) {
                glm::ivec3 pos(0);
                pos[axis] = d;
                pos[u] = i;
                pos[v] = j;

                BlockData blockData = chunk.getBlock(pos.x, pos.y, pos.z);
                const BlockInfo& blockInfo = BLOCK_INFO[blockData.type];

                // Skip non-solid categories
                if (blockInfo.category != CATEGORY_SOLID) {
                    mask[i + j * CHUNK_SIZE] = {0, 0, -1, 0, false};
                    continue;
                }

                // Neighbor logic
                glm::ivec3 neighborPos = pos + dir;
                BlockData neighbor = chunk.getBlock(neighborPos.x, neighborPos.y, neighborPos.z);
                const BlockInfo& neighborInfo = BLOCK_INFO[neighbor.type];

                bool isTransparent = blockInfo.textureInfo->transparent;
                bool isVisible = false;

                if (neighbor.type == BLOCK_AIR) {
                    isVisible = true;
                } else if (BLOCK_INFO[neighbor.type].category != CATEGORY_SOLID) {
                    isVisible = true; // Show faces against models/microblocks
                } else {
                    bool neighborTransparent = neighborInfo.textureInfo->transparent;

                    if (!isTransparent && neighborTransparent) {
                        // Opaque block next to glass -> Opaque face is visible
                        isVisible = true;
                    } else if (isTransparent && !neighborTransparent) {
                        // Glass block next to opaque -> Glass face is visible
                        isVisible = true;
                    } else if (isTransparent && neighborTransparent) {
                        // Glass next to Glass -> Only show if they are DIFFERENT types
                        // This prevents internal flickering/overlap
                        isVisible = (blockData.type != neighbor.type);
                    }
                    // Opaque next to Opaque -> hidden (isVisible = false)
                }

                if (isVisible) {
                    int originalFace = ROTATION_FACE_MAP[blockData.rotation][face];
                    uint8_t uvRot = (ROTATION_UV_ROT[blockData.rotation][face] +
                                     blockInfo.textureInfo->faces[originalFace].uvRotation) % 4;

                    mask[i + j * CHUNK_SIZE] = {
                        blockData.type,
                        blockData.rotation,
                        (int)blockInfo.textureInfo->faces[originalFace].textureId,
                        uvRot,
                        isTransparent
                    };
                } else {
                    mask[i + j * CHUNK_SIZE] = {0, 0, -1, 0, false};
                }
            }
        }

        // 2. Greedy Mesh the mask
        for (int j = 0; j < CHUNK_SIZE; j++) {
            for (int i = 0; i < CHUNK_SIZE; ) {
                FaceData& curr = mask[i + j * CHUNK_SIZE];
                if (curr.textureLayer < 0) { i++; continue; }

                // Find width
                int w = 1;
                while (i + w < CHUNK_SIZE) {
                    FaceData& next = mask[i + w + j * CHUNK_SIZE];
                    if (next.blockType != curr.blockType || next.textureLayer != curr.textureLayer ||
                        next.uvRotation != curr.uvRotation) break;
                    w++;
                }

                // Find height
                int h = 1;
                bool done = false;
                while (j + h < CHUNK_SIZE) {
                    for (int k = 0; k < w; k++) {
                        FaceData& nextRow = mask[i + k + (j + h) * CHUNK_SIZE];
                        if (nextRow.blockType != curr.blockType || nextRow.textureLayer != curr.textureLayer ||
                            nextRow.uvRotation != curr.uvRotation) {
                            done = true; break;
                        }
                    }
                    if (done) break;
                    h++;
                }

                // 3. Add to vertex buffer
                glm::ivec3 pos(0);
                pos[axis] = d; pos[u] = i; pos[v] = j;
                glm::vec3 worldPos = chunk.getWorldPos() + glm::vec3(pos);
                if (positive) worldPos[axis] += 1.0f;

                glm::vec3 du(0), dv(0);
                du[u] = (float)w; dv[v] = (float)h;

                float shade = 1.0f;
                if (face == FACE_NEG_Y) shade = 0.5f;
                else if (axis == 0) shade = 0.7f; // X faces
                else if (axis == 2) shade = 0.85f; // Z faces

                glm::vec3 finalColor = BLOCK_INFO[curr.blockType].color * shade;
                auto& targetBuffer = curr.transparent ? transparentVerts : opaqueVerts;

                // Calculate UVs with rotation and tiling
                glm::vec2 uv00 = rotateUV(glm::vec2(0, 0), curr.uvRotation);
                glm::vec2 uv10 = rotateUV(glm::vec2(w, 0), curr.uvRotation);
                glm::vec2 uv11 = rotateUV(glm::vec2(w, h), curr.uvRotation);
                glm::vec2 uv01 = rotateUV(glm::vec2(0, h), curr.uvRotation);

                if (positive) {
                    targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du, normal, finalColor, uv10, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + dv, normal, finalColor, uv01, (float)curr.textureLayer, 1.0f});
                } else {
                    targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du, normal, finalColor, uv10, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + dv, normal, finalColor, uv01, (float)curr.textureLayer, 1.0f});
                    targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, 1.0f});
                }

                // Clear mask for processed area
                for (int m = 0; m < h; m++) {
                    for (int n = 0; n < w; n++) {
                        mask[i + n + (j + m) * CHUNK_SIZE].textureLayer = -1;
                    }
                }
                i += w;
            }
        }
    }
}

    static void meshMicroBlocks(Chunk& chunk, std::vector<ChunkVertex>& verts) {
        float microScale = 1.0f / MICRO_BLOCK_SIZE;

        for (int bz = 0; bz < CHUNK_SIZE; bz++) {
            for (int by = 0; by < CHUNK_SIZE; by++) {
                for (int bx = 0; bx < CHUNK_SIZE; bx++) {
                    BlockData blockData = chunk.getBlock(bx, by, bz);
                    if (BLOCK_INFO[blockData.type].category != CATEGORY_MICRO)
                        continue;

                    MicroBlockData* microData = chunk.getMicroData(bx, by, bz);
                    if (!microData) continue;

                    glm::vec3 blockWorldPos = chunk.getWorldPos() +
                                              glm::vec3(bx, by, bz);

                    // Greedy mesh the micro block
                    meshMicroBlockFaces(*microData, blockWorldPos,
                                        microScale, verts);
                }
            }
        }
    }

    static void meshMicroBlockFaces(const MicroBlockData& data,
                                    glm::vec3 blockWorldPos,
                                    float scale,
                                    std::vector<ChunkVertex>& verts) {
        // Greedy mesh each face of the micro block
        for (int face = 0; face < 6; face++) {
            int axis = face / 2;
            bool positive = face % 2;
            int u = (axis + 1) % 3;
            int v = (axis + 2) % 3;

            glm::ivec3 dir(0);
            dir[axis] = positive ? 1 : -1;
            glm::vec3 normal = FACE_NORMALS[face];

            std::array<int, MICRO_BLOCK_SIZE * MICRO_BLOCK_SIZE> mask;

            for (int d = 0; d < MICRO_BLOCK_SIZE; d++) {
                // Build mask
                for (int j = 0; j < MICRO_BLOCK_SIZE; j++) {
                    for (int i = 0; i < MICRO_BLOCK_SIZE; i++) {
                        glm::ivec3 pos(0);
                        pos[axis] = d;
                        pos[u] = i;
                        pos[v] = j;

                        uint8_t colorIdx = data.get(pos.x, pos.y, pos.z);
                        if (colorIdx == 0) {
                            mask[i + j * MICRO_BLOCK_SIZE] = 0;
                            continue;
                        }

                        glm::ivec3 neighborPos = pos + dir;
                        uint8_t neighborColor = data.get(
                            neighborPos.x, neighborPos.y, neighborPos.z);

                        if (neighborColor == 0) {
                            mask[i + j * MICRO_BLOCK_SIZE] = colorIdx;
                        } else {
                            mask[i + j * MICRO_BLOCK_SIZE] = 0;
                        }
                    }
                }

                // Greedy mesh
                for (int j = 0; j < MICRO_BLOCK_SIZE; j++) {
                    for (int i = 0; i < MICRO_BLOCK_SIZE;) {
                        int colorIdx = mask[i + j * MICRO_BLOCK_SIZE];
                        if (colorIdx == 0) {
                            i++;
                            continue;
                        }

                        int w = 1;
                        while (i + w < MICRO_BLOCK_SIZE &&
                               mask[i + w + j * MICRO_BLOCK_SIZE] == colorIdx) {
                            w++;
                        }

                        int h = 1;
                        bool done = false;
                        while (j + h < MICRO_BLOCK_SIZE && !done) {
                            for (int k = 0; k < w; k++) {
                                if (mask[i + k + (j + h) * MICRO_BLOCK_SIZE] !=
                                    colorIdx) {
                                    done = true;
                                    break;
                                }
                            }
                            if (!done) h++;
                        }

                        glm::ivec3 pos(0);
                        pos[axis] = d;
                        pos[u] = i;
                        pos[v] = j;

                        glm::vec3 localPos = glm::vec3(pos) * scale;
                        glm::vec3 worldPos = blockWorldPos + localPos;
                        if (positive) worldPos[axis] += scale;

                        glm::vec3 du(0), dv(0);
                        du[u] = float(w) * scale;
                        dv[v] = float(h) * scale;

                        glm::vec3 color = g_microPalette.colors[colorIdx];

                        float shade = 1.0f;
                        if (face == FACE_NEG_Y) shade = 0.5f;
                        else if (face == FACE_NEG_X || face == FACE_POS_X)
                            shade = 0.7f;
                        else if (face == FACE_NEG_Z || face == FACE_POS_Z)
                            shade = 0.8f;
                        color *= shade;

                        // Use -1 for texture layer to indicate vertex color only
                        float texLayer = -1.0f;
                        glm::vec2 uvDummy(0);

                        if (positive) {
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + dv, normal, color, uvDummy, texLayer, 1.0f});
                        } else {
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + dv, normal, color, uvDummy, texLayer, 1.0f});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, 1.0f});
                        }

                        for (int l = 0; l < h; l++) {
                            for (int k = 0; k < w; k++) {
                                mask[i + k + (j + l) * MICRO_BLOCK_SIZE] = 0;
                            }
                        }

                        i += w;
                    }
                }
            }
        }
    }
};

// Model geometry generator
class ModelGeometry {
public:
    static std::vector<float> cubeVertices;

    static void init() {
        cubeVertices = {
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        };
    }
};

std::vector<float> ModelGeometry::cubeVertices;

// GPU Model Renderer with compute culling
class ModelRenderer {
public:
    GLuint modelVAO = 0, modelVBO = 0;
    GLuint inputSSBO = 0;
    GLuint outputSSBO = 0;
    GLuint indirectBuffer = 0;
    GLuint atomicBuffer = 0;
    GLuint computeProgram = 0;
    GLuint renderProgram = 0;

    std::vector<ModelInstance> allInstances;
    GLuint visibleCount = 0;

    void init(const char* computeSource, const char* vertSource,
              const char* fragSource) {
        ModelGeometry::init();

        GLuint cs = compileShader(GL_COMPUTE_SHADER, computeSource);
        computeProgram = glCreateProgram();
        glAttachShader(computeProgram, cs);
        glLinkProgram(computeProgram);
        glDeleteShader(cs);

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSource);
        renderProgram = glCreateProgram();
        glAttachShader(renderProgram, vs);
        glAttachShader(renderProgram, fs);
        glLinkProgram(renderProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        glGenVertexArrays(1, &modelVAO);
        glGenBuffers(1, &modelVBO);
        
        glBindVertexArray(modelVAO);
        glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     ModelGeometry::cubeVertices.size() * sizeof(float),
                     ModelGeometry::cubeVertices.data(), GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glGenBuffers(1, &inputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, 
                     GL_DYNAMIC_DRAW);
        
        glGenBuffers(1, &outputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, 
                     GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, outputSSBO);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
                              (void*)offsetof(ModelInstance, positionAndScale));
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
                              (void*)offsetof(ModelInstance, colorAndType));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glGenBuffers(1, &indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        DrawArraysIndirectCommand cmd = {36, 0, 0, 0};
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &atomicBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr,
                     GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    void uploadInstances() {
        if (allInstances.empty()) return;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        allInstances.size() * sizeof(ModelInstance),
                        allInstances.data());
    }

    void cullAndRender(const glm::vec4 frustumPlanes[6], const glm::mat4& view,
                       const glm::mat4& projection, const glm::vec3& viewPos) {
        if (allInstances.empty()) {
            visibleCount = 0;
            return;
        }

        GLuint totalInstances = static_cast<GLuint>(allInstances.size());

        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);

        glUseProgram(computeProgram);
        glUniform4fv(glGetUniformLocation(computeProgram, "frustumPlanes"), 6,
                     glm::value_ptr(frustumPlanes[0]));
        glUniform1ui(glGetUniformLocation(computeProgram, "totalInstances"),
                     totalInstances);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, atomicBuffer);

        GLuint numGroups = (totalInstances + 255) / 256;
        glDispatchCompute(numGroups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, indirectBuffer);
        glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_COPY_WRITE_BUFFER,
                            0, offsetof(DrawArraysIndirectCommand, instanceCount),
                            sizeof(GLuint));

        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint),
                           &visibleCount);

        glUseProgram(renderProgram);
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(renderProgram, "viewPos"), 1,
                     glm::value_ptr(viewPos));

        glBindVertexArray(modelVAO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &modelVAO);
        glDeleteBuffers(1, &modelVBO);
        glDeleteBuffers(1, &inputSSBO);
        glDeleteBuffers(1, &outputSSBO);
        glDeleteBuffers(1, &indirectBuffer);
        glDeleteBuffers(1, &atomicBuffer);
        glDeleteProgram(computeProgram);
        glDeleteProgram(renderProgram);
    }

private:
    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader error: " << infoLog << std::endl;
        }
        return shader;
    }
};

// Chunk manager
class ChunkManager {
public:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    std::vector<ChunkVertex> opaqueBuffer;
    std::vector<ChunkVertex> transparentBuffer;

    int64_t hashPos(glm::ivec3 pos) const {
        return (int64_t(pos.x) & 0xFFFFF) |
               ((int64_t(pos.y) & 0xFFFFF) << 20) |
               ((int64_t(pos.z) & 0xFFFFF) << 40);
    }

    Chunk* getChunk(glm::ivec3 pos) {
        auto it = chunks.find(hashPos(pos));
        return it != chunks.end() ? it->second.get() : nullptr;
    }

    Chunk* createChunk(glm::ivec3 pos) {
        auto chunk = std::make_unique<Chunk>(pos);
        Chunk* ptr = chunk.get();
        chunks[hashPos(pos)] = std::move(chunk);
        return ptr;
    }

    void generateTerrain(glm::ivec3 chunkPos) {
        Chunk* chunk = createChunk(chunkPos);
        glm::vec3 worldBase = chunk->getWorldPos();

        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                float wx = worldBase.x + x;
                float wz = worldBase.z + z;

                float height = 16.0f +
                    8.0f * sin(wx * 0.05f) * cos(wz * 0.05f) +
                    4.0f * sin(wx * 0.1f + 1.0f) * sin(wz * 0.1f);

                for (int y = 0; y < CHUNK_SIZE; y++) {
                    float wy = worldBase.y + y;

                    if (wy < height - 4) {
                        chunk->setBlock(x, y, z, BLOCK_STONE);
                    } else if (wy < height - 1) {
                        chunk->setBlock(x, y, z, BLOCK_DIRT);
                    } else if (wy < height) {
                        // Randomly rotate grass blocks
                        uint8_t rot = static_cast<uint8_t>(
                            (int(wx * 7 + wz * 13) % 4));
                        chunk->setBlock(x, y, z, BLOCK_GLASS, rot);

                        int iy = y + 1;
                        if (iy < CHUNK_SIZE &&
                            chunk->getBlock(x, iy, z).type == BLOCK_AIR) {
                            float r = fmodf(wx * 12.9898f + wz * 78.233f, 1.0f);
                            r = fmodf(sinf(r * 43758.5453f) * 0.5f + 0.5f, 1.0f);

                            if (r < 0.02f) {
                                chunk->setBlock(x, iy, z, BLOCK_FLOWER_RED);
                            } else if (r < 0.04f) {
                                chunk->setBlock(x, iy, z, BLOCK_FLOWER_YELLOW);
                            } else if (r < 0.15f) {
                                chunk->setBlock(x, iy, z, BLOCK_TALL_GRASS);
                            } else if (r < 0.17f) {
                                chunk->setBlock(x, iy, z, BLOCK_MUSHROOM);
                            } else if (r < 0.18f) {
                                chunk->setBlock(x, iy, z, BLOCK_CRYSTAL);
                            } else if (r < 0.20f) {
                                // Add a micro block
                                chunk->setBlock(x, iy, z, BLOCK_MICRO_TERRAIN);
                                generateMicroTerrain(chunk, x, iy, z);
                            } else if (r < 0.21f) {
                                chunk->setBlock(x, iy, z, BLOCK_MICRO_SCULPTURE);
                                generateMicroSculpture(chunk, x, iy, z);
                            }
                        }
                    }

                    // Add some glass windows at certain heights
                    if (wy > height && wy < height + 3) {
                        float gr = fmodf(wx * 5.5f + wz * 3.3f + wy * 2.1f, 1.0f);
                        gr = fmodf(sinf(gr * 12345.6f) * 0.5f + 0.5f, 1.0f);
                        if (gr < 0.01f) {
                            chunk->setBlock(x, y, z, BLOCK_GLASS);
                        }
                    }
                }
            }
        }
    }

    void generateMicroTerrain(Chunk* chunk, int bx, int by, int bz) {
        MicroBlockData* data = chunk->getMicroData(bx, by, bz);
        if (!data) return;

        // Generate a small terrain-like pattern
        for (int z = 0; z < MICRO_BLOCK_SIZE; z++) {
            for (int x = 0; x < MICRO_BLOCK_SIZE; x++) {
                float h = 4.0f + 3.0f * sinf(x * 0.5f) * cosf(z * 0.5f);
                for (int y = 0; y < MICRO_BLOCK_SIZE; y++) {
                    if (y < h) {
                        if (y < h - 2) {
                            data->set(x, y, z, 3);  // Gray (stone)
                        } else if (y < h - 1) {
                            data->set(x, y, z, 15); // Brown (dirt)
                        } else {
                            data->set(x, y, z, 7);  // Green (grass)
                        }
                    }
                }
            }
        }
    }

    void generateMicroSculpture(Chunk* chunk, int bx, int by, int bz) {
        MicroBlockData* data = chunk->getMicroData(bx, by, bz);
        if (!data) return;

        // Generate a simple sculpture (sphere-ish)
        glm::vec3 center(8, 8, 8);
        for (int z = 0; z < MICRO_BLOCK_SIZE; z++) {
            for (int y = 0; y < MICRO_BLOCK_SIZE; y++) {
                for (int x = 0; x < MICRO_BLOCK_SIZE; x++) {
                    float dist = glm::length(glm::vec3(x, y, z) - center);
                    if (dist < 6.0f) {
                        // Color based on distance from center
                        uint8_t colorIdx = static_cast<uint8_t>(
                            6 + (int(dist) % 8));
                        data->set(x, y, z, colorIdx);
                    }
                }
            }
        }
    }

    void rebuildChunkMesh(Chunk* chunk) {
        if (!chunk->needsRebuild) return;

        GreedyMesher::mesh(*chunk, opaqueBuffer, transparentBuffer);

        chunk->vertexCount = opaqueBuffer.size();
        chunk->transparentVertexCount = transparentBuffer.size();

        if (chunk->vertexCount == 0 && chunk->transparentVertexCount == 0) {
            chunk->isEmpty = true;
            chunk->needsRebuild = false;
            return;
        }

        chunk->isEmpty = false;

        // Upload opaque mesh
        if (chunk->vertexCount > 0) {
            if (!chunk->VAO) {
                glGenVertexArrays(1, &chunk->VAO);
                glGenBuffers(1, &chunk->VBO);
            }

            glBindVertexArray(chunk->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
            glBufferData(GL_ARRAY_BUFFER,
                         opaqueBuffer.size() * sizeof(ChunkVertex),
                         opaqueBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        // Upload transparent mesh
        if (chunk->transparentVertexCount > 0) {
            if (!chunk->transparentVAO) {
                glGenVertexArrays(1, &chunk->transparentVAO);
                glGenBuffers(1, &chunk->transparentVBO);
            }

            glBindVertexArray(chunk->transparentVAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->transparentVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         transparentBuffer.size() * sizeof(ChunkVertex),
                         transparentBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        chunk->needsRebuild = false;
        chunk->meshUploaded = true;
    }

    void setupChunkVertexAttribs() {
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, position));
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, normal));
        glEnableVertexAttribArray(1);

        // Color
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, color));
        glEnableVertexAttribArray(2);

        // UV
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, uv));
        glEnableVertexAttribArray(3);

        // Texture layer
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, textureLayer));
        glEnableVertexAttribArray(4);

        // AO
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, ao));
        glEnableVertexAttribArray(5);
    }

    void collectAllModelBlocks(ModelRenderer& renderer) {
        renderer.allInstances.clear();

        for (auto& [hash, chunk] : chunks) {
            chunk->collectModelBlocks();
            renderer.allInstances.insert(
                renderer.allInstances.end(),
                chunk->modelBlocks.begin(),
                chunk->modelBlocks.end()
            );
        }

        renderer.uploadInstances();
    }
};

// Frustum
class Frustum {
public:
    glm::vec4 planes[6];

    void extract(const glm::mat4& vp) {
        planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                              vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
        planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                              vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
        planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                              vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
        planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                              vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
        planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                              vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
        planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                              vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

        for (int i = 0; i < 6; i++) {
            float len = glm::length(glm::vec3(planes[i]));
            planes[i] /= len;
        }
    }

    bool isBoxVisible(const glm::vec3& minPos, const glm::vec3& maxPos) const {
        for (int i = 0; i < 6; i++) {
            glm::vec3 p(
                planes[i].x > 0 ? maxPos.x : minPos.x,
                planes[i].y > 0 ? maxPos.y : minPos.y,
                planes[i].z > 0 ? maxPos.z : minPos.z
            );
            if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0)
                return false;
        }
        return true;
    }
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double x_in, double y_in) {
    float x = static_cast<float>(x_in);
    float y = static_cast<float>(y_in);

    if (g_manager.ui_mode) return;
    if (first_mouse) {
        g_manager.lastX = x;
        g_manager.lastY = y;
        first_mouse = false;
    }

    float delta_x = (x - g_manager.lastX) * g_manager.camera.sensitivity;
    float delta_y = (g_manager.lastY - y) * g_manager.camera.sensitivity;
    g_manager.lastX = x;
    g_manager.lastY = y;

    g_manager.camera.yaw += delta_x;
    g_manager.camera.pitch = glm::clamp(g_manager.camera.pitch + delta_y,
                                         -89.0f, 89.0f);

    glm::vec3 front;
    front.x = cos(glm::radians(g_manager.camera.yaw)) *
              cos(glm::radians(g_manager.camera.pitch));
    front.y = sin(glm::radians(g_manager.camera.pitch));
    front.z = sin(glm::radians(g_manager.camera.yaw)) *
              cos(glm::radians(g_manager.camera.pitch));
    g_manager.camera.front = glm::normalize(front);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool tab_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tab_pressed) {
            g_manager.ui_mode = !g_manager.ui_mode;
            glfwSetInputMode(window, GLFW_CURSOR,
                             g_manager.ui_mode ? GLFW_CURSOR_NORMAL :
                                                 GLFW_CURSOR_DISABLED);
            if (!g_manager.ui_mode) first_mouse = true;
            tab_pressed = true;
        }
    } else {
        tab_pressed = false;
    }

    if (!g_manager.ui_mode) {
        float speed = g_manager.camera.speed * g_manager.delta_time;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            g_manager.camera.pos += speed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            g_manager.camera.pos -= speed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            g_manager.camera.pos -= glm::normalize(
                glm::cross(g_manager.camera.front, g_manager.camera.up)) * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            g_manager.camera.pos += glm::normalize(
                glm::cross(g_manager.camera.front, g_manager.camera.up)) * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            g_manager.camera.pos += speed * g_manager.camera.up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            g_manager.camera.pos -= speed * g_manager.camera.up;
    }
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader error:\n" << infoLog << std::endl;
    }
    return shader;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "Hybrid Voxels - Textured + Micro Blocks", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Initialize texture atlas and block textures
    initBlockTextures();

    // Compile chunk shaders
    GLuint chunkVS = compileShader(GL_VERTEX_SHADER, chunkVertSource);
    GLuint chunkFS = compileShader(GL_FRAGMENT_SHADER, chunkFragSource);
    GLuint chunkProgram = glCreateProgram();
    glAttachShader(chunkProgram, chunkVS);
    glAttachShader(chunkProgram, chunkFS);
    glLinkProgram(chunkProgram);
    glDeleteShader(chunkVS);
    glDeleteShader(chunkFS);

    // Initialize model renderer
    ModelRenderer modelRenderer;
    modelRenderer.init(cullingCompSource, modelVertSource, modelFragSource);

    // Generate chunks
    ChunkManager chunkManager;
    const int WORLD_SIZE = 16;

    std::cout << "Generating terrain..." << std::endl;
    for (int z = 0; z < WORLD_SIZE; z++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            for (int x = 0; x < WORLD_SIZE; x++) {
                chunkManager.generateTerrain(glm::ivec3(x, y, z));
            }
        }
    }

    std::cout << "Building meshes..." << std::endl;
    int count = 0;
    for (auto& [hash, chunk] : chunkManager.chunks) {
        chunkManager.rebuildChunkMesh(chunk.get());
        std::cout << count << "/" << WORLD_SIZE * WORLD_SIZE * WORLD_SIZE << std::endl;
        count+=1;
    }

    std::cout << "Collecting model blocks..." << std::endl;
    chunkManager.collectAllModelBlocks(modelRenderer);
    std::cout << "Total model instances: " << modelRenderer.allInstances.size()
              << std::endl;

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Frustum frustum;
    uint32_t totalChunks = 0, visibleChunks = 0;
    uint32_t totalOpaqueVerts = 0, visibleOpaqueVerts = 0;
    uint32_t totalTransparentVerts = 0, visibleTransparentVerts = 0;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f,
            g_manager.camera.render_distance
        );
        glm::mat4 view = glm::lookAt(
            g_manager.camera.pos,
            g_manager.camera.pos + g_manager.camera.front,
            g_manager.camera.up
        );
        glm::mat4 vp = projection * view;

        frustum.extract(vp);

        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Bind texture atlas
        g_textureAtlas.bind(0);

        // Setup chunk shader
        glUseProgram(chunkProgram);
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(chunkProgram, "viewPos"), 1,
                     glm::value_ptr(g_manager.camera.pos));
        glUniform1i(glGetUniformLocation(chunkProgram, "textureArray"), 0);

        totalChunks = 0;
        visibleChunks = 0;
        totalOpaqueVerts = 0;
        visibleOpaqueVerts = 0;
        totalTransparentVerts = 0;
        visibleTransparentVerts = 0;

        // Render opaque geometry first
        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded) continue;

            totalChunks++;
            totalOpaqueVerts += chunk->vertexCount;
            totalTransparentVerts += chunk->transparentVertexCount;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (!frustum.isBoxVisible(minPos, maxPos)) continue;

            visibleChunks++;

            if (chunk->vertexCount > 0) {
                visibleOpaqueVerts += chunk->vertexCount;
                glBindVertexArray(chunk->VAO);
                glDrawArrays(GL_TRIANGLES, 0,
                             static_cast<GLsizei>(chunk->vertexCount));
            }
        }

        // Render GPU-culled model blocks
        glDisable(GL_CULL_FACE);
        modelRenderer.cullAndRender(frustum.planes, view, projection,
                                    g_manager.camera.pos);
        glEnable(GL_CULL_FACE);

        // Render transparent geometry last (with blending)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glUseProgram(chunkProgram);

        // Collect chunks with transparent geometry
        struct TransparentJob {
            Chunk* chunk;
            float distanceSq;
        };
        std::vector<TransparentJob> transparentJobs;

        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded || chunk->transparentVertexCount == 0)
                continue;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (frustum.isBoxVisible(minPos, maxPos)) {
                glm::vec3 chunkCenter = minPos + glm::vec3(CHUNK_SIZE / 2.0f);
                float distSq = glm::distance2(g_manager.camera.pos, chunkCenter);
                transparentJobs.push_back({chunk.get(), distSq});
            }
        }

        // Sort Back-to-Front (highest distance first)
        std::sort(transparentJobs.begin(), transparentJobs.end(),
            [](const TransparentJob& a, const TransparentJob& b) {
                return a.distanceSq > b.distanceSq;
            });

        // Draw sorted chunks
        for (auto& job : transparentJobs) {
            visibleTransparentVerts += job.chunk->transparentVertexCount;
            glBindVertexArray(job.chunk->transparentVAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(job.chunk->transparentVertexCount));
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_manager.ui_mode) {
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }

        ImGui::Begin("Debug Menu");
        if (g_manager.ui_mode) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MENU MODE (TAB to close)");
        } else {
            ImGui::Text("GAME MODE (TAB for menu)");
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "Solid Blocks (Textured Greedy Mesh):");
        ImGui::Text("  Chunks: %u / %u visible", visibleChunks, totalChunks);
        ImGui::Text("  Opaque Verts: %u / %u", visibleOpaqueVerts, totalOpaqueVerts);
        ImGui::Text("  Transparent Verts: %u / %u", visibleTransparentVerts,
                    totalTransparentVerts);
        ImGui::Text("  Triangles: %u",
                    (visibleOpaqueVerts + visibleTransparentVerts) / 3);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f),
                           "Model Blocks (GPU Culled):");
        ImGui::Text("  Total: %zu", modelRenderer.allInstances.size());
        ImGui::Text("  Visible: %u", modelRenderer.visibleCount);
        ImGui::Text("  Culled: %.1f%%",
                    modelRenderer.allInstances.size() > 0
                    ? 100.0f * (1.0f - (float)modelRenderer.visibleCount /
                               modelRenderer.allInstances.size())
                    : 0.0f);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "Texture Atlas:");
        ImGui::Text("  Textures: %d", g_textureAtlas.layerCount);
        ImGui::Text("  Tile Size: %d", g_textureAtlas.tileSize);

        ImGui::Separator();
        ImGui::Text("Camera: %.1f, %.1f, %.1f",
                    g_manager.camera.pos.x, g_manager.camera.pos.y,
                    g_manager.camera.pos.z);
        ImGui::SliderFloat("Speed", &g_manager.camera.speed, 1.0f, 200.0f);
        ImGui::SliderFloat("Render Dist", &g_manager.camera.render_distance,
                           50.0f, 1000.0f);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(chunkProgram);
    modelRenderer.cleanup();
    g_textureAtlas.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
