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
    ,0
};
const char fragmentShaderSource[] = {
#embed "shaders/cube.frag"
    ,0
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
    .lastX = SCR_WIDTH / 2.0,
    .lastY = SCR_HEIGHT / 2.0,
    .delta_time = 0.0,
    .last_frame = 0.0,
    .ui_mode = false,
    .camera = {
        .pos   = glm::vec3(0.0f, 0.0f, 3.0f),
        .front = glm::vec3(0.0f, 0.0f, -1.0f),
        .up    = glm::vec3(0.0f, 1.0f, 0.0f),
        .yaw   = -90.0f,
        .pitch = 0.0f,
        .sensitivity = 0.1f,
        .speed= 30.0f,
        .render_distance = 300.0f,
    },
};


float g_cube[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f,  0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f,  0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
};




void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void mouse_callback(GLFWwindow* window, double x_in, double y_in){
   float x = static_cast<float>(x_in);
   float y = static_cast<float>(y_in);

   if (g_manager.ui_mode) return;
   if (first_mouse){
       g_manager.lastX = x;
       g_manager.lastY = y;
       first_mouse = false;
   }

   // Update our last positions
   float delta_x = (x - g_manager.lastX) * g_manager.camera.sensitivity;
   float delta_y = (g_manager.lastY - y) * g_manager.camera.sensitivity;
   g_manager.lastX = x;
   g_manager.lastY = y;

   g_manager.camera.yaw += delta_x;
   g_manager.camera.pitch += delta_y;

   // Compute camera changes
   g_manager.camera.pitch = (g_manager.camera.pitch > 89.0f) ? 89.0f : g_manager.camera.pitch;
   g_manager.camera.pitch = (g_manager.camera.pitch < -89.0f) ? -89.0f : g_manager.camera.pitch;
   glm::vec3 front;
   front.x = cos(glm::radians(g_manager.camera.yaw)) * cos(glm::radians(g_manager.camera.pitch));
   front.y = sin(glm::radians(g_manager.camera.pitch));
   front.z = sin(glm::radians(g_manager.camera.yaw)) * cos(glm::radians(g_manager.camera.pitch));
   g_manager.camera.front = glm::normalize(front);
}


void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle UI mode with TAB
    static bool tab_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tab_pressed) {
            g_manager.ui_mode = !g_manager.ui_mode;

            if (g_manager.ui_mode) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                first_mouse = true; // Prevents camera snap
            }
            tab_pressed = true;
        }
    } else {
        tab_pressed = false;
    }

    // Only move camera if NOT in UI mode
    if (!g_manager.ui_mode) {
        // FORCE cursor disabled while in game mode (safety check)
        // Some systems/drivers need this if focus is lost and regained
        if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        float cameraSpeed = static_cast<float>(g_manager.camera.speed * g_manager.delta_time);
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




int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Request opengl 4.6
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "C++23 OpenGL Window", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    // VSYNC
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

    // Setup our offset buffers thing
    std::vector<glm::vec3> cube_positions;
    float offset = 2.0f;
    for (int y = -100; y < 100; y += 2) {
        for (int x = -100; x < 100; x += 2) {
            for (int z = -100; z < 100; z += 2) {
                cube_positions.push_back(glm::vec3(x, y, z));
            }
        }
    }

    // Compile Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char *vptr = vertexShaderSource;
    glShaderSource(vertexShader, 1, &vptr, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char *fptr = fragmentShaderSource;
    glShaderSource(fragmentShader, 1, &fptr, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Setup buffers
    //
    unsigned int VBO, VAO, cubesVBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &cubesVBO);

    // cube
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_cube), g_cube, GL_STATIC_DRAW);

    // Position attribute
    // Stride could be 3 * sizeof(float). But since we're tightly packing I will just do that explicitly
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // cubes positions
    glBindBuffer(GL_ARRAY_BUFFER, cubesVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        cube_positions.size() * sizeof(glm::vec3),
        cube_positions.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    // We are changing per instance not per vertex
    glVertexAttribDivisor(1, 1);


    // Setup IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

        // IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (!g_manager.ui_mode) {
            // Tell ImGui to ignore the mouse and keyboard entirely
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;

            // Clear keyboard focus so nothing is highlighted
            ImGui::SetWindowFocus(NULL);
        } else {
            // Re-enable mouse when UI is active
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        }

        // Create a simple debug window
        ImGui::Begin("Debug Menu");
        if (g_manager.ui_mode) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MENU MODE ACTIVE (TAB to close)");
        } else {
            ImGui::Text("GAME MODE ACTIVE (TAB for menu)");
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Cube Count: %zu ", cube_positions.size());
        ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", g_manager.camera.pos.x, g_manager.camera.pos.y, g_manager.camera.pos.z);
        ImGui::SliderFloat("Camera Speed", &g_manager.camera.speed, 0.5f, 100.0f);
        ImGui::End();

        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Transformation matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, g_manager.camera.render_distance);
        glm::mat4 view = glm::lookAt(g_manager.camera.pos, g_manager.camera.pos + g_manager.camera.front, g_manager.camera.up);
        glm::mat4 model = glm::mat4(1.0f);
        // model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.5f, 1.0f, 0.0f));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)cube_positions.size());

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &cubesVBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
