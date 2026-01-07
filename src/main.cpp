#include <iostream>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

const char vertexShaderSource[] = {
#embed "shaders/cube.vert"
    , 0
};
const char fragmentShaderSource[] = {
#embed "shaders/cube.frag"
    , 0
};
const char computeShaderSource[] = {
#embed "shaders/culling.comp"
    , 0
};

// Camera
typedef struct {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
    float yaw;
    float pitch;
    float sensitivity;
    float speed;
    float render_distance;
} Camera;

typedef struct {
    float lastX;
    float lastY;
    float delta_time;
    float last_frame;
    bool ui_mode;
    Camera camera;
} Manager;

// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Globals
bool first_mouse = true;
Manager g_manager = {
    .lastX = SCR_WIDTH / 2.0f,
    .lastY = SCR_HEIGHT / 2.0f,
    .delta_time = 0.0f,
    .last_frame = 0.0f,
    .ui_mode = false,
    .camera = {
        .pos   = glm::vec3(0.0f, 0.0f, 3.0f),
        .front = glm::vec3(0.0f, 0.0f, -1.0f),
        .up    = glm::vec3(0.0f, 1.0f, 0.0f),
        .yaw   = -90.0f,
        .pitch = 0.0f,
        .sensitivity = 0.1f,
        .speed = 30.0f,
        .render_distance = 300.0f,
    },
};

float g_cube[] = {
    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
};

// Indirect draw command structure
struct DrawArraysIndirectCommand {
    GLuint vertexCount;
    GLuint instanceCount;
    GLuint firstVertex;
    GLuint baseInstance;
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
    g_manager.camera.pitch += delta_y;

    g_manager.camera.pitch = glm::clamp(g_manager.camera.pitch, -89.0f, 89.0f);

    glm::vec3 front;
    front.x = cos(glm::radians(g_manager.camera.yaw)) * cos(glm::radians(g_manager.camera.pitch));
    front.y = sin(glm::radians(g_manager.camera.pitch));
    front.z = sin(glm::radians(g_manager.camera.yaw)) * cos(glm::radians(g_manager.camera.pitch));
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
                g_manager.ui_mode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            if (!g_manager.ui_mode) first_mouse = true;
            tab_pressed = true;
        }
    } else {
        tab_pressed = false;
    }

    if (!g_manager.ui_mode) {
        if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        float cameraSpeed = g_manager.camera.speed * g_manager.delta_time;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            g_manager.camera.pos += cameraSpeed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            g_manager.camera.pos -= cameraSpeed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            g_manager.camera.pos -= glm::normalize(glm::cross(g_manager.camera.front, g_manager.camera.up)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            g_manager.camera.pos += glm::normalize(glm::cross(g_manager.camera.front, g_manager.camera.up)) * cameraSpeed;
    }
}

// Extract frustum planes from view-projection matrix
void extractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]) {
    // Left
    planes[0] = glm::vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );
    // Right
    planes[1] = glm::vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );
    // Bottom
    planes[2] = glm::vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );
    // Top
    planes[3] = glm::vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );
    // Near
    planes[4] = glm::vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );
    // Far
    planes[5] = glm::vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
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
        std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "C++23 OpenGL - GPU Frustum Culling", nullptr, nullptr);
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

    // Generate cube positions
    std::vector<glm::vec4> cube_positions;
    for (int y = -100; y < 100; y += 2) {
        for (int x = -100; x < 100; x += 2) {
            for (int z = -100; z < 100; z += 2) {
                cube_positions.push_back(glm::vec4(x, y, z, 1.0f));
            }
        }
    }
    GLuint totalCubes = static_cast<GLuint>(cube_positions.size());

    // Compile render shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Compile compute shader
    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeShaderSource);
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);
    glDeleteShader(computeShader);

    // Setup VAO/VBO for cube geometry
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_cube), g_cube, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // SSBO 0: Input positions (all cubes)
    GLuint inputSSBO;
    glGenBuffers(1, &inputSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        cube_positions.size() * sizeof(glm::vec4),
        cube_positions.data(), GL_STATIC_DRAW);

    // SSBO 1: Output positions (visible cubes) - also used as instance VBO
    GLuint outputSSBO;
    glGenBuffers(1, &outputSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        cube_positions.size() * sizeof(glm::vec4),
        nullptr, GL_DYNAMIC_DRAW);

    // Bind output SSBO as instance attribute
    glBindBuffer(GL_ARRAY_BUFFER, outputSSBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // SSBO 2: Indirect draw command
    GLuint indirectBuffer;
    glGenBuffers(1, &indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    DrawArraysIndirectCommand cmd = { 36, 0, 0, 0 };
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);

    // SSBO 3: Atomic counter
    GLuint atomicBuffer;
    glGenBuffers(1, &atomicBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    GLuint visibleCount = 0;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

        // Build view-projection matrix and extract frustum planes
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
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

        glm::vec4 frustumPlanes[6];
        extractFrustumPlanes(vp, frustumPlanes);

        // Reset atomic counter
        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);

        // Dispatch compute shader for frustum culling
        glUseProgram(computeProgram);
        glUniform4fv(glGetUniformLocation(computeProgram, "frustumPlanes"), 6, glm::value_ptr(frustumPlanes[0]));
        glUniform1ui(glGetUniformLocation(computeProgram, "totalCubes"), totalCubes);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, atomicBuffer);

        GLuint numGroups = (totalCubes + 255) / 256;
        glDispatchCompute(numGroups, 1, 1);

        // Memory barrier to ensure compute shader writes are visible
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        // Copy visible count to indirect buffer's instanceCount
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, indirectBuffer);
        glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_COPY_WRITE_BUFFER,
            0, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(GLuint));

        // Read back visible count for debug display
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &visibleCount);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_manager.ui_mode) {
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
            ImGui::SetWindowFocus(NULL);
        }

        ImGui::Begin("Debug Menu");
        if (g_manager.ui_mode) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MENU MODE ACTIVE (TAB to close)");
        } else {
            ImGui::Text("GAME MODE ACTIVE (TAB for menu)");
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);
        ImGui::Separator();
        ImGui::Text("Total Cubes: %u", totalCubes);
        ImGui::Text("Visible Cubes: %u", visibleCount);
        ImGui::Text("Culled: %.1f%%", 100.0f * (1.0f - (float)visibleCount / totalCubes));
        ImGui::Separator();
        ImGui::Text("Camera Pos: %.2f, %.2f, %.2f",
            g_manager.camera.pos.x, g_manager.camera.pos.y, g_manager.camera.pos.z);
        ImGui::SliderFloat("Camera Speed", &g_manager.camera.speed, 0.5f, 100.0f);
        ImGui::SliderFloat("Render Distance", &g_manager.camera.render_distance, 50.0f, 500.0f);
        ImGui::End();

        // Render
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &inputSSBO);
    glDeleteBuffers(1, &outputSSBO);
    glDeleteBuffers(1, &indirectBuffer);
    glDeleteBuffers(1, &atomicBuffer);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(computeProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
