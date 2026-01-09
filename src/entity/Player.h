#pragma once

#include "../core/Common.h"

class Player {
public:
    // Position and movement
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    float yaw;
    float pitch;

    // Settings
    float moveSpeed = 50.0f;
    float mouseSensitivity = 0.1f;
    int chunkRenderDistance = 8;      // Chunks to load around player
    float viewDistance = 500.0f;      // Far plane distance

    // Input state
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    bool firstMouse = true;

    Player(glm::vec3 startPos = glm::vec3(0.0f, 64.0f, 0.0f))
        : position(startPos)
        , velocity(0.0f)
        , yaw(-90.0f)
        , pitch(0.0f)
        , up(0.0f, 1.0f, 0.0f)
    {
        updateVectors();
    }

    void updateVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
        right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, front));
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, viewDistance);
    }

    // Get the chunk coordinates the player is currently in
    glm::ivec3 getChunkPosition() const {
        return glm::ivec3(
            (int)floor(position.x / CHUNK_SIZE),
            (int)floor(position.y / CHUNK_SIZE),
            (int)floor(position.z / CHUNK_SIZE)
        );
    }

    // Get block position player is at
    glm::ivec3 getBlockPosition() const {
        return glm::ivec3(floor(position.x), floor(position.y), floor(position.z));
    }

    void processKeyboard(GLFWwindow* window, float deltaTime, bool uiMode) {
        if (uiMode) return;

        float speed = moveSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += speed * front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= speed * front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= speed * right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += speed * right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position.y += speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            position.y -= speed;
    }

    void processMouse(float xpos, float ypos, bool uiMode) {
        if (uiMode) return;

        if (firstMouse) {
            lastMouseX = xpos;
            lastMouseY = ypos;
            firstMouse = false;
        }

        float xoffset = (xpos - lastMouseX) * mouseSensitivity;
        float yoffset = (lastMouseY - ypos) * mouseSensitivity;
        lastMouseX = xpos;
        lastMouseY = ypos;

        yaw += xoffset;
        pitch = glm::clamp(pitch + yoffset, -89.0f, 89.0f);

        updateVectors();
    }

    void resetMouse() {
        firstMouse = true;
    }
};
