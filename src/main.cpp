#include <iostream>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


const char* vertexShaderSource = R"glsl(
#version 460 core
layout (location = 0) in vec3 aPos;
out vec3 ourColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aPos;
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 460 core
out vec4 FragColor;
in vec3 ourColor;
void main() {
    FragColor = vec4(ourColor, 1.0);
}
)glsl";


// Camera
typedef struct {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
    float yaw;
    float pitch;
    float sensitivity;
    float speed;
} Camera;

typedef struct {
    float lastX;
    float lastY;
    float delta_time;
    float last_frame;
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
    .camera = {
        .pos   = glm::vec3(0.0f, 0.0f, 3.0f),
        .front = glm::vec3(0.0f, 0.0f, -1.0f),
        .up    = glm::vec3(0.0f, 1.0f, 0.0f),
        .yaw   = -90.0f,
        .pitch = 0.0f,
        .sensitivity = 0.1f,
        .speed= 2.5f
    }
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


    // Compile Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Setup buffers
    //
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_cube), g_cube, GL_STATIC_DRAW);

    // Position attribute
    // Stride could be 3 * sizeof(float). But since we're tightly packing I will just do that explicitly
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);


    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Transformation matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(g_manager.camera.pos, g_manager.camera.pos + g_manager.camera.front, g_manager.camera.up);
        glm::mat4 model = glm::mat4(1.0f);
        // model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.5f, 1.0f, 0.0f));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
