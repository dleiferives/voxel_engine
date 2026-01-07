#pragma once

#include "../world/Chunk.h"
#include "../world/Lighting.h"

inline glm::vec2 rotateUV(glm::vec2 uv, uint8_t rotation) {
    switch (rotation % 4) {
        case 1: return glm::vec2(uv.y, 1.0f - uv.x);
        case 2: return glm::vec2(1.0f - uv.x, 1.0f - uv.y);
        case 3: return glm::vec2(1.0f - uv.y, uv.x);
        default: return uv;
    }
}

class GreedyMesher {
public:
    static void mesh(Chunk& chunk,
                     std::vector<ChunkVertex>& opaqueVerts,
                     std::vector<ChunkVertex>& transparentVerts) {
        opaqueVerts.clear();
        transparentVerts.clear();

        for (int face = 0; face < 6; face++) {
            meshFace(chunk, opaqueVerts, transparentVerts, face);
        }

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
        int axis = face / 2;
        bool positive = face % 2;
        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;

        glm::ivec3 dir(0);
        dir[axis] = positive ? 1 : -1;
        glm::vec3 normal = FACE_NORMALS[face];

        std::array<FaceData, CHUNK_SIZE * CHUNK_SIZE> mask;

        for (int d = 0; d < CHUNK_SIZE; d++) {
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    glm::ivec3 pos(0);
                    pos[axis] = d;
                    pos[u] = i;
                    pos[v] = j;

                    BlockData blockData = chunk.getBlock(pos.x, pos.y, pos.z);
                    const BlockInfo& blockInfo = BLOCK_INFO[blockData.type];

                    if (blockInfo.category != CATEGORY_SOLID) {
                        mask[i + j * CHUNK_SIZE] = {0, 0, -1, 0, false};
                        continue;
                    }

                    glm::ivec3 neighborPos = pos + dir;
                    BlockData neighbor = chunk.getBlock(neighborPos.x, neighborPos.y, neighborPos.z);
                    const BlockInfo& neighborInfo = BLOCK_INFO[neighbor.type];

                    bool isTransparent = blockInfo.textureInfo->transparent;
                    bool isVisible = false;

                    if (neighbor.type == BLOCK_AIR) {
                        isVisible = true;
                    } else if (BLOCK_INFO[neighbor.type].category != CATEGORY_SOLID) {
                        isVisible = true;
                    } else {
                        bool neighborTransparent = neighborInfo.textureInfo->transparent;

                        if (!isTransparent && neighborTransparent) {
                            isVisible = true;
                        } else if (isTransparent && !neighborTransparent) {
                            isVisible = true;
                        } else if (isTransparent && neighborTransparent) {
                            isVisible = (blockData.type != neighbor.type);
                        }
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

            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; ) {
                    FaceData& curr = mask[i + j * CHUNK_SIZE];
                    if (curr.textureLayer < 0) { i++; continue; }

                    int w = 1;
                    while (i + w < CHUNK_SIZE) {
                        FaceData& next = mask[i + w + j * CHUNK_SIZE];
                        if (next.blockType != curr.blockType || next.textureLayer != curr.textureLayer ||
                            next.uvRotation != curr.uvRotation) break;
                        w++;
                    }

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

                    glm::ivec3 pos(0);
                    pos[axis] = d; pos[u] = i; pos[v] = j;
                    glm::vec3 worldPos = chunk.getWorldPos() + glm::vec3(pos);
                    if (positive) worldPos[axis] += 1.0f;

                    glm::vec3 du(0), dv(0);
                    du[u] = (float)w; dv[v] = (float)h;

                    float lightLevel = getSmoothLight(chunk, pos.x, pos.y, pos.z, face);

                    const BlockInfo& blockInfo = BLOCK_INFO[curr.blockType];
                    if (blockInfo.textureInfo && blockInfo.textureInfo->fullBright) {
                        lightLevel = 1.0f;
                    }

                    lightLevel = std::max(lightLevel, 0.1f);

                    float dirShade = 1.0f;
                    if (face == FACE_NEG_Y) dirShade = 0.6f;
                    else if (axis == 0) dirShade = 0.8f;
                    else if (axis == 2) dirShade = 0.9f;

                    glm::vec3 finalColor = BLOCK_INFO[curr.blockType].color * dirShade;
                    auto& targetBuffer = curr.transparent ? transparentVerts : opaqueVerts;

                    glm::vec2 uv00 = rotateUV(glm::vec2(0, 0), curr.uvRotation);
                    glm::vec2 uv10 = rotateUV(glm::vec2(w, 0), curr.uvRotation);
                    glm::vec2 uv11 = rotateUV(glm::vec2(w, h), curr.uvRotation);
                    glm::vec2 uv01 = rotateUV(glm::vec2(0, h), curr.uvRotation);

                    if (positive) {
                        targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du, normal, finalColor, uv10, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + dv, normal, finalColor, uv01, (float)curr.textureLayer, lightLevel});
                    } else {
                        targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du, normal, finalColor, uv10, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos, normal, finalColor, uv00, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + dv, normal, finalColor, uv01, (float)curr.textureLayer, lightLevel});
                        targetBuffer.push_back({worldPos + du + dv, normal, finalColor, uv11, (float)curr.textureLayer, lightLevel});
                    }

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

                    glm::vec3 blockWorldPos = chunk.getWorldPos() + glm::vec3(bx, by, bz);

                    meshMicroBlockFaces(*microData, blockWorldPos, microScale, verts, chunk, bx, by, bz);
                }
            }
        }
    }

    static void meshMicroBlockFaces(const MicroBlockData& data,
                                    glm::vec3 blockWorldPos,
                                    float scale,
                                    std::vector<ChunkVertex>& verts,
                                    Chunk& chunk, int bx, int by, int bz) {
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
                        uint8_t neighborColor = data.get(neighborPos.x, neighborPos.y, neighborPos.z);

                        if (neighborColor == 0) {
                            mask[i + j * MICRO_BLOCK_SIZE] = colorIdx;
                        } else {
                            mask[i + j * MICRO_BLOCK_SIZE] = 0;
                        }
                    }
                }

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
                                if (mask[i + k + (j + h) * MICRO_BLOCK_SIZE] != colorIdx) {
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

                        float lightLevel = getSmoothLight(chunk, bx, by, bz, face);
                        lightLevel = std::max(lightLevel, 0.1f);

                        float dirShade = 1.0f;
                        if (face == FACE_NEG_Y) dirShade = 0.6f;
                        else if (face == FACE_NEG_X || face == FACE_POS_X)
                            dirShade = 0.8f;
                        else if (face == FACE_NEG_Z || face == FACE_POS_Z)
                            dirShade = 0.9f;
                        color *= dirShade;

                        float texLayer = -1.0f;
                        glm::vec2 uvDummy(0);

                        if (positive) {
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + dv, normal, color, uvDummy, texLayer, lightLevel});
                        } else {
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + dv, normal, color, uvDummy, texLayer, lightLevel});
                            verts.push_back({worldPos + du + dv, normal, color, uvDummy, texLayer, lightLevel});
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
