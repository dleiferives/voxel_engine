#pragma once

#include "../core/Common.h"
#include "Block.h"
#include "Chunk.h"

// Simple hash-based noise (no external libraries needed)
class TerrainGenerator {
public:
    uint32_t seed;

    TerrainGenerator(uint32_t s = 12345) : seed(s) {}

    // Fast hash function for noise
    uint32_t hash(int x, int y, int z) const {
        uint32_t h = seed;
        h ^= x * 374761393u;
        h ^= y * 668265263u;
        h ^= z * 1274126177u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    // 2D hash for heightmap
    uint32_t hash2D(int x, int z) const {
        return hash(x, 0, z);
    }

    // Smooth noise interpolation
    float smoothNoise2D(float x, float z) const {
        int ix = (int)floor(x);
        int iz = (int)floor(z);
        float fx = x - ix;
        float fz = z - iz;

        // Smoothstep
        fx = fx * fx * (3.0f - 2.0f * fx);
        fz = fz * fz * (3.0f - 2.0f * fz);

        float n00 = (hash2D(ix, iz) & 0xFFFF) / 65535.0f;
        float n10 = (hash2D(ix + 1, iz) & 0xFFFF) / 65535.0f;
        float n01 = (hash2D(ix, iz + 1) & 0xFFFF) / 65535.0f;
        float n11 = (hash2D(ix + 1, iz + 1) & 0xFFFF) / 65535.0f;

        float nx0 = n00 * (1 - fx) + n10 * fx;
        float nx1 = n01 * (1 - fx) + n11 * fx;
        return nx0 * (1 - fz) + nx1 * fz;
    }

    // Multi-octave noise for terrain
    float terrainNoise(float x, float z, int octaves = 4) const {
        float value = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves; i++) {
            value += smoothNoise2D(x * frequency * 0.02f, z * frequency * 0.02f) * amplitude;
            maxValue += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        return value / maxValue;
    }

    // Get terrain height at world position
    float getHeight(float worldX, float worldZ) const {
        float baseHeight = terrainNoise(worldX, worldZ, 4);
        float hills = terrainNoise(worldX * 0.5f + 1000, worldZ * 0.5f + 1000, 3);

        // Base ground level around 32, with hills up to ~80
        return 32.0f + baseHeight * 24.0f + hills * hills * 20.0f;
    }

    // Determine block type based on position
    uint8_t getBlockType(float wx, float wy, float wz, float surfaceHeight) const {
        float depth = surfaceHeight - wy;

        if (wy > surfaceHeight) {
            return BLOCK_AIR;
        }

        // Surface layer
        if (depth < 1.0f) {
            // Beach near water level
            if (surfaceHeight < 36.0f) {
                return BLOCK_SAND;
            }
            return BLOCK_GRASS;
        }

        // Dirt layer (3-5 blocks deep)
        if (depth < 4.0f) {
            if (surfaceHeight < 36.0f) {
                return BLOCK_SAND;
            }
            return BLOCK_DIRT;
        }

        // Stone below
        return BLOCK_STONE;
    }

    // Generate a chunk at given chunk coordinates
    void generateChunk(Chunk& chunk, int chunkX, int chunkY, int chunkZ) {
        glm::vec3 chunkWorldPos = glm::vec3(chunkX, chunkY, chunkZ) * (float)CHUNK_SIZE;

        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                float worldX = chunkWorldPos.x + x;
                float worldZ = chunkWorldPos.z + z;
                float surfaceHeight = getHeight(worldX, worldZ);

                for (int y = 0; y < CHUNK_SIZE; y++) {
                    float worldY = chunkWorldPos.y + y;
                    uint8_t blockType = getBlockType(worldX, worldY, worldZ, surfaceHeight);
                    chunk.setBlock(x, y, z, blockType);
                }
            }
        }

        // Add some trees on grass
        for (int z = 2; z < CHUNK_SIZE - 2; z++) {
            for (int x = 2; x < CHUNK_SIZE - 2; x++) {
                float worldX = chunkWorldPos.x + x;
                float worldZ = chunkWorldPos.z + z;

                // Random tree placement
                if ((hash2D((int)worldX, (int)worldZ) % 100) < 2) {
                    float surfaceHeight = getHeight(worldX, worldZ);
                    int localY = (int)(surfaceHeight - chunkWorldPos.y);

                    // Check if surface is in this chunk and is grass
                    if (localY >= 0 && localY < CHUNK_SIZE - 6) {
                        BlockData surfaceBlock = chunk.getBlock(x, localY, z);
                        if (surfaceBlock.type == BLOCK_GRASS) {
                            // Place tree trunk
                            int trunkHeight = 4 + (hash2D((int)worldX + 100, (int)worldZ + 100) % 3);
                            for (int ty = 1; ty <= trunkHeight && (localY + ty) < CHUNK_SIZE; ty++) {
                                chunk.setBlock(x, localY + ty, z, BLOCK_LOG);
                            }

                            // Place leaves
                            int leafStart = trunkHeight - 1;
                            for (int ly = leafStart; ly <= trunkHeight + 1 && (localY + ly) < CHUNK_SIZE; ly++) {
                                int radius = (ly == trunkHeight + 1) ? 1 : 2;
                                for (int lx = -radius; lx <= radius; lx++) {
                                    for (int lz = -radius; lz <= radius; lz++) {
                                        if (lx == 0 && lz == 0 && ly <= trunkHeight) continue;
                                        int nx = x + lx;
                                        int nz = z + lz;
                                        int ny = localY + ly;
                                        if (nx >= 0 && nx < CHUNK_SIZE &&
                                            nz >= 0 && nz < CHUNK_SIZE &&
                                            ny >= 0 && ny < CHUNK_SIZE) {
                                            BlockData existing = chunk.getBlock(nx, ny, nz);
                                            if (existing.type == BLOCK_AIR) {
                                                chunk.setBlock(nx, ny, nz, BLOCK_LEAVES);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

// Global terrain generator instance
inline TerrainGenerator g_terrainGen(42);
