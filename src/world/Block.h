#pragma once

#include "../core/Common.h"

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
    // Light-emitting solid blocks
    BLOCK_GLOWSTONE,
    BLOCK_LAVA,
    // Model blocks (GPU culled)
    BLOCK_FLOWER_RED,
    BLOCK_FLOWER_YELLOW,
    BLOCK_TALL_GRASS,
    BLOCK_MUSHROOM,
    BLOCK_CRYSTAL,
    BLOCK_TORCH,
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

inline const glm::vec3 FACE_NORMALS[] = {
    glm::vec3(-1, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0),
    glm::vec3(0, 1, 0),  glm::vec3(0, 0, -1), glm::vec3(0, 0, 1),
};

// Block rotation system (24 possible orientations)
enum BlockOrientation : uint8_t {
    ROT_Y_UP_Z_FWD = 0,
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
inline const uint8_t ROTATION_FACE_MAP[24][6] = {
    {0, 1, 2, 3, 4, 5}, {4, 5, 2, 3, 1, 0}, {1, 0, 2, 3, 5, 4}, {5, 4, 2, 3, 0, 1},
    {0, 1, 3, 2, 5, 4}, {5, 4, 3, 2, 1, 0}, {1, 0, 3, 2, 4, 5}, {4, 5, 3, 2, 0, 1},
    {0, 1, 4, 5, 3, 2}, {2, 3, 4, 5, 1, 0}, {1, 0, 4, 5, 2, 3}, {3, 2, 4, 5, 0, 1},
    {0, 1, 5, 4, 2, 3}, {3, 2, 5, 4, 1, 0}, {1, 0, 5, 4, 3, 2}, {2, 3, 5, 4, 0, 1},
    {2, 3, 0, 1, 4, 5}, {4, 5, 0, 1, 3, 2}, {3, 2, 0, 1, 5, 4}, {5, 4, 0, 1, 2, 3},
    {3, 2, 1, 0, 4, 5}, {5, 4, 1, 0, 2, 3}, {2, 3, 1, 0, 5, 4}, {4, 5, 1, 0, 3, 2},
};

// UV rotation per face for each block rotation
inline const uint8_t ROTATION_UV_ROT[24][6] = {
    {0, 0, 0, 0, 0, 0}, {1, 3, 1, 3, 0, 0}, {2, 2, 2, 2, 0, 0}, {3, 1, 3, 1, 0, 0},
    {0, 0, 0, 0, 2, 2}, {3, 1, 1, 3, 2, 2}, {2, 2, 2, 2, 2, 2}, {1, 3, 3, 1, 2, 2},
    {0, 0, 0, 0, 1, 3}, {0, 0, 1, 3, 1, 3}, {0, 0, 2, 2, 1, 3}, {0, 0, 3, 1, 1, 3},
    {0, 0, 0, 0, 3, 1}, {0, 0, 3, 1, 3, 1}, {0, 0, 2, 2, 3, 1}, {0, 0, 1, 3, 3, 1},
    {1, 3, 0, 0, 1, 3}, {1, 3, 1, 3, 1, 3}, {1, 3, 2, 2, 1, 3}, {1, 3, 3, 1, 1, 3},
    {3, 1, 0, 0, 3, 1}, {3, 1, 3, 1, 3, 1}, {3, 1, 2, 2, 3, 1}, {3, 1, 1, 3, 3, 1},
};

// Texture face specification
struct TextureFace {
    uint16_t textureId;
    uint8_t uvRotation;
};

// Block texture definition
struct BlockTextureInfo {
    TextureFace faces[6];
    bool transparent;
    bool fullBright;
};

// Block info with texture support
struct BlockInfo {
    BlockCategory category;
    glm::vec3 color;
    float scale;
    uint32_t modelId;
    BlockTextureInfo* textureInfo;
    uint8_t lightEmission;
    bool blocksLight;
};

// Block data with rotation
struct BlockData {
    uint8_t type;
    uint8_t rotation;
    uint16_t microDataIndex;
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
        glm::vec3(0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.8f, 0.8f, 0.8f),
        glm::vec3(0.5f, 0.5f, 0.5f),
        glm::vec3(0.2f, 0.2f, 0.2f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 0.5f, 0.0f),
        glm::vec3(0.5f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.75f, 0.8f),
        glm::vec3(0.6f, 0.3f, 0.0f),
        glm::vec3(0.5f, 0.5f, 0.0f),
        glm::vec3(0.0f, 0.5f, 0.5f),
        glm::vec3(0.0f, 0.0f, 0.5f),
        glm::vec3(0.5f, 0.0f, 0.0f),
        glm::vec3(0.9f, 0.9f, 0.6f),
        glm::vec3(0.8f, 0.6f, 0.4f),
        glm::vec3(0.4f, 0.2f, 0.1f),
        glm::vec3(0.6f, 0.8f, 0.2f),
        glm::vec3(0.2f, 0.4f, 0.2f),
        glm::vec3(0.4f, 0.6f, 0.8f),
        glm::vec3(0.8f, 0.4f, 0.4f),
        glm::vec3(0.6f, 0.4f, 0.8f),
        glm::vec3(0.4f, 0.8f, 0.6f),
        glm::vec3(0.8f, 0.8f, 0.4f),
        glm::vec3(0.6f, 0.6f, 0.8f),
        glm::vec3(0.8f, 0.6f, 0.6f),
    };
};

// Vertex structures
struct ChunkVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    float textureLayer;
    float ao;
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

// Global palette (defined in main.cpp)
extern MicroBlockPalette g_microPalette;

// Global block textures (defined in main.cpp)
extern BlockTextureInfo g_blockTextures[BLOCK_COUNT];

// Global block info (defined in main.cpp)
extern BlockInfo BLOCK_INFO[];
