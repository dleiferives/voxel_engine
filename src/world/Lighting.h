#pragma once

#include "Chunk.h"

// Light node for BFS propagation
struct LightNode {
    int x, y, z;
    uint8_t light;
};

// Direction offsets for 6-connected neighbors
inline const int LIGHT_DIRS[6][3] = {
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
};

// Propagate sunlight from the top of the chunk
inline void propagateSunlight(Chunk& chunk) {
    std::queue<LightNode> lightQueue;

    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            bool inSunlight = true;
            for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
                BlockData block = chunk.getBlock(x, y, z);
                const BlockInfo& info = BLOCK_INFO[block.type];

                if (inSunlight && !info.blocksLight) {
                    chunk.setSkyLight(x, y, z, 15);
                    lightQueue.push({x, y, z, 15});
                } else if (info.blocksLight) {
                    inSunlight = false;
                    chunk.setSkyLight(x, y, z, 0);
                } else {
                    chunk.setSkyLight(x, y, z, 0);
                }
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        for (int d = 0; d < 6; d++) {
            int nx = node.x + LIGHT_DIRS[d][0];
            int ny = node.y + LIGHT_DIRS[d][1];
            int nz = node.z + LIGHT_DIRS[d][2];

            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE ||
                nz < 0 || nz >= CHUNK_SIZE)
                continue;

            BlockData neighborBlock = chunk.getBlock(nx, ny, nz);
            const BlockInfo& neighborInfo = BLOCK_INFO[neighborBlock.type];

            if (neighborInfo.blocksLight)
                continue;

            uint8_t newLight = (node.light > 1) ? node.light - 1 : 0;

            if (d == 2 && node.light == 15) {
                newLight = 15;
            }

            if (newLight > chunk.getSkyLight(nx, ny, nz)) {
                chunk.setSkyLight(nx, ny, nz, newLight);
                if (newLight > 1) {
                    lightQueue.push({nx, ny, nz, newLight});
                }
            }
        }
    }
}

// Propagate block light from light-emitting blocks
inline void propagateBlockLight(Chunk& chunk) {
    std::queue<LightNode> lightQueue;

    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                BlockData block = chunk.getBlock(x, y, z);
                const BlockInfo& info = BLOCK_INFO[block.type];

                if (info.lightEmission > 0) {
                    chunk.setBlockLight(x, y, z, info.lightEmission);
                    lightQueue.push({x, y, z, info.lightEmission});
                }
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        for (int d = 0; d < 6; d++) {
            int nx = node.x + LIGHT_DIRS[d][0];
            int ny = node.y + LIGHT_DIRS[d][1];
            int nz = node.z + LIGHT_DIRS[d][2];

            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE ||
                nz < 0 || nz >= CHUNK_SIZE)
                continue;

            BlockData neighborBlock = chunk.getBlock(nx, ny, nz);
            const BlockInfo& neighborInfo = BLOCK_INFO[neighborBlock.type];

            if (neighborInfo.blocksLight)
                continue;

            uint8_t newLight = (node.light > 1) ? node.light - 1 : 0;

            if (newLight > chunk.getBlockLight(nx, ny, nz)) {
                chunk.setBlockLight(nx, ny, nz, newLight);
                if (newLight > 1) {
                    lightQueue.push({nx, ny, nz, newLight});
                }
            }
        }
    }
}

// Main light calculation function
inline void calculateChunkLighting(Chunk& chunk) {
    chunk.lightData.fill(0);
    propagateSunlight(chunk);
    propagateBlockLight(chunk);
    chunk.needsLightRebuild = false;
}

// Get light level for a face - samples the air block adjacent to the face
inline float getSmoothLight(Chunk& chunk, int bx, int by, int bz, int face) {
    int nx = bx + (int)FACE_NORMALS[face].x;
    int ny = by + (int)FACE_NORMALS[face].y;
    int nz = bz + (int)FACE_NORMALS[face].z;

    // Handle out-of-chunk samples
    if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE) {
        // Above chunk = full sunlight
        if (ny >= CHUNK_SIZE) return 1.0f;
        // Below chunk or outside horizontally = assume some ambient
        if (ny < 0) return 0.3f;
        // Side edges - use edge block's light or ambient
        return 0.7f;
    }

    return chunk.getCombinedLight(nx, ny, nz);
}
