#pragma once

#include "Block.h"

class Chunk {
public:
    glm::ivec3 chunkPos;
    std::array<BlockData, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> blocks;
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> lightData;
    std::vector<MicroBlockData> microBlockData;

    GLuint VAO = 0, VBO = 0;
    size_t vertexCount = 0;

    GLuint transparentVAO = 0, transparentVBO = 0;
    size_t transparentVertexCount = 0;

    bool needsRebuild = true;
    bool isEmpty = true;
    bool meshUploaded = false;
    bool needsLightRebuild = true;

    std::vector<ModelInstance> modelBlocks;
    bool modelsDirty = true;

    Chunk(glm::ivec3 pos) : chunkPos(pos) {
        for (auto& block : blocks) {
            block.type = BLOCK_AIR;
            block.rotation = ROT_Y_UP_Z_FWD;
            block.microDataIndex = 0;
        }
        lightData.fill(0);
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

    // Light access methods
    uint8_t getSkyLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return 15;
        return (lightData[index(x, y, z)] >> 4) & 0x0F;
    }

    uint8_t getBlockLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return 0;
        return lightData[index(x, y, z)] & 0x0F;
    }

    void setSkyLight(int x, int y, int z, uint8_t level) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return;
        int idx = index(x, y, z);
        lightData[idx] = (lightData[idx] & 0x0F) | ((level & 0x0F) << 4);
    }

    void setBlockLight(int x, int y, int z, uint8_t level) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
            z < 0 || z >= CHUNK_SIZE)
            return;
        int idx = index(x, y, z);
        lightData[idx] = (lightData[idx] & 0xF0) | (level & 0x0F);
    }

    float getCombinedLight(int x, int y, int z) const {
        uint8_t sky = getSkyLight(x, y, z);
        uint8_t block = getBlockLight(x, y, z);
        uint8_t maxLight = std::max(sky, block);
        return maxLight / 15.0f;
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
        needsLightRebuild = true;
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
