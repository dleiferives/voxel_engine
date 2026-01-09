#pragma once

#include "../core/Common.h"
#include "../world/Block.h"

struct RaycastHit {
    bool hit;
    glm::ivec3 blockPos;    // Block that was hit
    glm::ivec3 placePos;    // Adjacent position for placing
    int face;               // Which face was hit (Face enum)
    float distance;
};

// DDA voxel traversal algorithm
// Returns the first solid block hit along the ray
template<typename GetBlockFunc>
inline RaycastHit raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, GetBlockFunc getBlock) {
    RaycastHit result = {false, glm::ivec3(0), glm::ivec3(0), -1, maxDist};

    if (glm::length(direction) < 0.0001f) return result;
    direction = glm::normalize(direction);

    // Current voxel position
    glm::ivec3 voxel = glm::ivec3(glm::floor(origin));

    // Direction to step in each axis
    glm::ivec3 step;
    step.x = direction.x >= 0 ? 1 : -1;
    step.y = direction.y >= 0 ? 1 : -1;
    step.z = direction.z >= 0 ? 1 : -1;

    // Distance along ray to cross one voxel in each axis
    glm::vec3 tDelta;
    tDelta.x = direction.x != 0 ? std::abs(1.0f / direction.x) : 1e30f;
    tDelta.y = direction.y != 0 ? std::abs(1.0f / direction.y) : 1e30f;
    tDelta.z = direction.z != 0 ? std::abs(1.0f / direction.z) : 1e30f;

    // Distance to next voxel boundary in each axis
    glm::vec3 tMax;
    if (direction.x > 0) {
        tMax.x = (float(voxel.x + 1) - origin.x) / direction.x;
    } else if (direction.x < 0) {
        tMax.x = (origin.x - float(voxel.x)) / -direction.x;
    } else {
        tMax.x = 1e30f;
    }

    if (direction.y > 0) {
        tMax.y = (float(voxel.y + 1) - origin.y) / direction.y;
    } else if (direction.y < 0) {
        tMax.y = (origin.y - float(voxel.y)) / -direction.y;
    } else {
        tMax.y = 1e30f;
    }

    if (direction.z > 0) {
        tMax.z = (float(voxel.z + 1) - origin.z) / direction.z;
    } else if (direction.z < 0) {
        tMax.z = (origin.z - float(voxel.z)) / -direction.z;
    } else {
        tMax.z = 1e30f;
    }

    float dist = 0.0f;
    int lastAxis = -1;
    glm::ivec3 prevVoxel = voxel;

    while (dist < maxDist) {
        // Check current voxel
        BlockData block = getBlock(voxel.x, voxel.y, voxel.z);
        const BlockInfo& info = BLOCK_INFO[block.type];

        // Hit a solid or micro block
        if (info.category == CATEGORY_SOLID || info.category == CATEGORY_MICRO) {
            result.hit = true;
            result.blockPos = voxel;
            result.placePos = prevVoxel;
            result.distance = dist;

            // Determine which face was hit based on entry direction
            if (lastAxis == 0) {
                result.face = step.x > 0 ? FACE_NEG_X : FACE_POS_X;
            } else if (lastAxis == 1) {
                result.face = step.y > 0 ? FACE_NEG_Y : FACE_POS_Y;
            } else if (lastAxis == 2) {
                result.face = step.z > 0 ? FACE_NEG_Z : FACE_POS_Z;
            }

            return result;
        }

        prevVoxel = voxel;

        // Step to next voxel
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            dist = tMax.x;
            tMax.x += tDelta.x;
            voxel.x += step.x;
            lastAxis = 0;
        } else if (tMax.y < tMax.z) {
            dist = tMax.y;
            tMax.y += tDelta.y;
            voxel.y += step.y;
            lastAxis = 1;
        } else {
            dist = tMax.z;
            tMax.z += tDelta.z;
            voxel.z += step.z;
            lastAxis = 2;
        }
    }

    return result;
}
