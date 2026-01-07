#pragma once

#include "Common.h"

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

// Globals
inline bool first_mouse = true;
inline Manager g_manager = {
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

// Callbacks
inline void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

inline void mouse_callback(GLFWwindow* window, double x_in, double y_in) {
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

inline void processInput(GLFWwindow* window) {
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
