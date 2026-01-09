#pragma once

#include "Common.h"

// Frustum culling helper
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

// Framebuffer resize callback
inline void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
