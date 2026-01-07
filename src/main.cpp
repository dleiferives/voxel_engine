#include <iostream>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstring>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <memory>
#include <unordered_map>


const char vertexShaderSource[] = {
#embed "shaders/chunk.vert"
    , 0
};
const char fragmentShaderSource[] = {
#embed "shaders/chunk.frag"
    , 0
};

// Constants
constexpr int CHUNK_SIZE = 32;
constexpr float BLOCK_SIZE = 1.0f;

// Camera
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
        .pos   = glm::vec3(48.0f, 48.0f, 48.0f),
        .front = glm::vec3(0.0f, 0.0f, -1.0f),
        .up    = glm::vec3(0.0f, 1.0f, 0.0f),
        .yaw   = -135.0f,
        .pitch = -30.0f,
        .sensitivity = 0.1f,
        .speed = 50.0f,
        .render_distance = 500.0f,
    },
};

// Vertex structure for greedy meshed chunks
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Block types
enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_COUNT
};

// Block colors
const glm::vec3 BLOCK_COLORS[] = {
    glm::vec3(0.0f, 0.0f, 0.0f),       // AIR (unused)
    glm::vec3(0.5f, 0.5f, 0.5f),       // STONE
    glm::vec3(0.45f, 0.3f, 0.15f),     // DIRT
    glm::vec3(0.2f, 0.6f, 0.2f),       // GRASS
    glm::vec3(0.9f, 0.85f, 0.6f),      // SAND
    glm::vec3(0.2f, 0.4f, 0.8f),       // WATER
};

// Face directions
enum Face { FACE_NEG_X, FACE_POS_X, FACE_NEG_Y, FACE_POS_Y, FACE_NEG_Z, FACE_POS_Z };

const glm::vec3 FACE_NORMALS[] = {
    glm::vec3(-1, 0, 0), glm::vec3(1, 0, 0),
    glm::vec3(0, -1, 0), glm::vec3(0, 1, 0),
    glm::vec3(0, 0, -1), glm::vec3(0, 0, 1),
};

// Chunk class
class Chunk {
public:
    glm::ivec3 chunkPos;  // Position in chunk coordinates
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> blocks;
    GLuint VAO = 0, VBO = 0;
    size_t vertexCount = 0;
    bool needsRebuild = true;
    bool isEmpty = true;
    bool meshUploaded = false;

    Chunk(glm::ivec3 pos) : chunkPos(pos) {
        blocks.fill(BLOCK_AIR);
    }

    ~Chunk() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
    }

    inline int index(int x, int y, int z) const {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }

    uint8_t getBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
            return BLOCK_AIR;
        return blocks[index(x, y, z)];
    }

    void setBlock(int x, int y, int z, uint8_t type) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
            return;
        blocks[index(x, y, z)] = type;
        needsRebuild = true;
        isEmpty = false;
    }

    glm::vec3 getWorldPos() const {
        return glm::vec3(chunkPos) * float(CHUNK_SIZE);
    }

    // Get bounding box for frustum culling
    void getBoundingBox(glm::vec3& minPos, glm::vec3& maxPos) const {
        minPos = getWorldPos();
        maxPos = minPos + glm::vec3(CHUNK_SIZE);
    }
};

// Greedy meshing implementation
class GreedyMesher {
public:
    static void mesh(Chunk& chunk, std::vector<Vertex>& vertices) {
        vertices.clear();

        // For each face direction
        for (int face = 0; face < 6; face++) {
            meshFace(chunk, vertices, face);
        }
    }

private:
    static void meshFace(Chunk& chunk, std::vector<Vertex>& vertices, int face) {
        // Determine axis and direction
        int axis = face / 2;        // 0=X, 1=Y, 2=Z
        bool positive = face % 2;   // false=negative, true=positive

        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;

        glm::ivec3 dir(0);
        dir[axis] = positive ? 1 : -1;

        glm::vec3 normal = FACE_NORMALS[face];

        // Mask for greedy meshing
        std::array<int, CHUNK_SIZE * CHUNK_SIZE> mask;

        // Iterate through slices
        for (int d = 0; d < CHUNK_SIZE; d++) {
            // Build mask for this slice
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    glm::ivec3 pos(0);
                    pos[axis] = d;
                    pos[u] = i;
                    pos[v] = j;

                    uint8_t block = chunk.getBlock(pos.x, pos.y, pos.z);

                    // Check neighbor
                    glm::ivec3 neighborPos = pos + dir;
                    uint8_t neighbor;
                    if (neighborPos[axis] < 0 || neighborPos[axis] >= CHUNK_SIZE) {
                        neighbor = BLOCK_AIR;  // Chunk boundary
                    } else {
                        neighbor = chunk.getBlock(neighborPos.x, neighborPos.y, neighborPos.z);
                    }

                    // Face is visible if current block is solid and neighbor is air
                    if (block != BLOCK_AIR && neighbor == BLOCK_AIR) {
                        mask[i + j * CHUNK_SIZE] = block;
                    } else {
                        mask[i + j * CHUNK_SIZE] = 0;
                    }
                }
            }

            // Greedy mesh the mask
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE;) {
                    int blockType = mask[i + j * CHUNK_SIZE];
                    if (blockType == 0) {
                        i++;
                        continue;
                    }

                    // Find width
                    int w = 1;
                    while (i + w < CHUNK_SIZE && mask[i + w + j * CHUNK_SIZE] == blockType) {
                        w++;
                    }

                    // Find height
                    int h = 1;
                    bool done = false;
                    while (j + h < CHUNK_SIZE && !done) {
                        for (int k = 0; k < w; k++) {
                            if (mask[i + k + (j + h) * CHUNK_SIZE] != blockType) {
                                done = true;
                                break;
                            }
                        }
                        if (!done) h++;
                    }

                    // Create quad
                    glm::ivec3 pos(0);
                    pos[axis] = d;
                    pos[u] = i;
                    pos[v] = j;

                    glm::vec3 worldPos = chunk.getWorldPos() + glm::vec3(pos);

                    // Offset position for positive faces
                    if (positive) {
                        worldPos[axis] += 1.0f;
                    }

                    // Create quad vertices
                    glm::vec3 du(0), dv(0);
                    du[u] = float(w);
                    dv[v] = float(h);

                    glm::vec3 color = BLOCK_COLORS[blockType];

                    // Apply simple ambient occlusion-like shading based on face
                    float shade = 1.0f;
                    if (face == FACE_NEG_Y) shade = 0.5f;
                    else if (face == FACE_NEG_X || face == FACE_POS_X) shade = 0.7f;
                    else if (face == FACE_NEG_Z || face == FACE_POS_Z) shade = 0.8f;
                    color *= shade;

                    // Two triangles for the quad
                    if (positive) {
                        vertices.push_back({worldPos, normal, color});
                        vertices.push_back({worldPos + du, normal, color});
                        vertices.push_back({worldPos + du + dv, normal, color});

                        vertices.push_back({worldPos, normal, color});
                        vertices.push_back({worldPos + du + dv, normal, color});
                        vertices.push_back({worldPos + dv, normal, color});
                    } else {
                        vertices.push_back({worldPos, normal, color});
                        vertices.push_back({worldPos + du + dv, normal, color});
                        vertices.push_back({worldPos + du, normal, color});

                        vertices.push_back({worldPos, normal, color});
                        vertices.push_back({worldPos + dv, normal, color});
                        vertices.push_back({worldPos + du + dv, normal, color});
                    }

                    // Clear mask
                    for (int l = 0; l < h; l++) {
                        for (int k = 0; k < w; k++) {
                            mask[i + k + (j + l) * CHUNK_SIZE] = 0;
                        }
                    }

                    i += w;
                }
            }
        }
    }
};

// Chunk manager
class ChunkManager {
public:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    std::vector<Vertex> meshBuffer;  // Reusable buffer

    int64_t hashPos(glm::ivec3 pos) const {
        return (int64_t(pos.x) & 0xFFFFF) |
               ((int64_t(pos.y) & 0xFFFFF) << 20) |
               ((int64_t(pos.z) & 0xFFFFF) << 40);
    }

    Chunk* getChunk(glm::ivec3 pos) {
        auto it = chunks.find(hashPos(pos));
        return it != chunks.end() ? it->second.get() : nullptr;
    }

    Chunk* createChunk(glm::ivec3 pos) {
        auto chunk = std::make_unique<Chunk>(pos);
        Chunk* ptr = chunk.get();
        chunks[hashPos(pos)] = std::move(chunk);
        return ptr;
    }

    void generateTerrain(glm::ivec3 chunkPos) {
        Chunk* chunk = createChunk(chunkPos);
        glm::vec3 worldBase = chunk->getWorldPos();

        // Simple terrain generation
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                float wx = worldBase.x + x;
                float wz = worldBase.z + z;

                // Simple height function
                float height = 16.0f +
                    8.0f * sin(wx * 0.05f) * cos(wz * 0.05f) +
                    4.0f * sin(wx * 0.1f + 1.0f) * sin(wz * 0.1f);

                for (int y = 0; y < CHUNK_SIZE; y++) {
                    float wy = worldBase.y + y;

                    if (wy < height - 4) {
                        chunk->setBlock(x, y, z, BLOCK_STONE);
                    } else if (wy < height - 1) {
                        chunk->setBlock(x, y, z, BLOCK_DIRT);
                    } else if (wy < height) {
                        chunk->setBlock(x, y, z, BLOCK_GRASS);
                    }
                }
            }
        }
    }

    void rebuildChunkMesh(Chunk* chunk) {
        if (!chunk->needsRebuild) return;

        GreedyMesher::mesh(*chunk, meshBuffer);
        chunk->vertexCount = meshBuffer.size();

        if (chunk->vertexCount == 0) {
            chunk->isEmpty = true;
            chunk->needsRebuild = false;
            return;
        }

        chunk->isEmpty = false;

        // Create/update GPU buffers
        if (!chunk->VAO) {
            glGenVertexArrays(1, &chunk->VAO);
            glGenBuffers(1, &chunk->VBO);
        }

        glBindVertexArray(chunk->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
        glBufferData(GL_ARRAY_BUFFER,
            meshBuffer.size() * sizeof(Vertex),
            meshBuffer.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        // Color
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        chunk->needsRebuild = false;
        chunk->meshUploaded = true;
    }
};

// Frustum culling
class Frustum {
public:
    glm::vec4 planes[6];

    void extract(const glm::mat4& vp) {
        // Left
        planes[0] = glm::vec4(
            vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
            vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
        // Right
        planes[1] = glm::vec4(
            vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
            vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
        // Bottom
        planes[2] = glm::vec4(
            vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
            vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
        // Top
        planes[3] = glm::vec4(
            vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
            vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
        // Near
        planes[4] = glm::vec4(
            vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
            vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
        // Far
        planes[5] = glm::vec4(
            vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
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
            if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0) {
                return false;
            }
        }
        return true;
    }
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
    g_manager.camera.pitch = glm::clamp(g_manager.camera.pitch + delta_y, -89.0f, 89.0f);

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

        float speed = g_manager.camera.speed * g_manager.delta_time;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            g_manager.camera.pos += speed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            g_manager.camera.pos -= speed * g_manager.camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            g_manager.camera.pos -= glm::normalize(glm::cross(g_manager.camera.front, g_manager.camera.up)) * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            g_manager.camera.pos += glm::normalize(glm::cross(g_manager.camera.front, g_manager.camera.up)) * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            g_manager.camera.pos += speed * g_manager.camera.up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            g_manager.camera.pos -= speed * g_manager.camera.up;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "Chunked Voxels - Greedy Meshing + Frustum Culling", nullptr, nullptr);
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
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Compile shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Generate chunks
    ChunkManager chunkManager;
    const int WORLD_SIZE = 4;  // 4x4x4 chunks = 128x128x128 blocks

    for (int z = 0; z < WORLD_SIZE; z++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            for (int x = 0; x < WORLD_SIZE; x++) {
                chunkManager.generateTerrain(glm::ivec3(x, y, z));
            }
        }
    }

    // Build all meshes
    for (auto& [hash, chunk] : chunkManager.chunks) {
        chunkManager.rebuildChunkMesh(chunk.get());
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Frustum frustum;
    uint32_t totalChunks = 0;
    uint32_t visibleChunks = 0;
    uint32_t totalVertices = 0;
    uint32_t visibleVertices = 0;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

        // Build matrices
        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
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

        frustum.extract(vp);

        // Clear
        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(g_manager.camera.pos));

        // Render chunks with frustum culling
        totalChunks = 0;
        visibleChunks = 0;
        totalVertices = 0;
        visibleVertices = 0;

        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded) continue;

            totalChunks++;
            totalVertices += chunk->vertexCount;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (!frustum.isBoxVisible(minPos, maxPos)) continue;

            visibleChunks++;
            visibleVertices += chunk->vertexCount;

            glBindVertexArray(chunk->VAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(chunk->vertexCount));
        }

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_manager.ui_mode) {
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }

        ImGui::Begin("Debug Menu");
        if (g_manager.ui_mode) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MENU MODE (TAB to close)");
        } else {
            ImGui::Text("GAME MODE (TAB for menu)");
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);
        ImGui::Separator();
        ImGui::Text("Chunk Size: %dx%dx%d", CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
        ImGui::Text("Total Chunks: %u", totalChunks);
        ImGui::Text("Visible Chunks: %u", visibleChunks);
        ImGui::Text("Chunks Culled: %.1f%%",
            totalChunks > 0 ? 100.0f * (1.0f - (float)visibleChunks / totalChunks) : 0.0f);
        ImGui::Separator();
        ImGui::Text("Total Vertices: %u", totalVertices);
        ImGui::Text("Visible Vertices: %u", visibleVertices);
        ImGui::Text("Total Triangles: %u", totalVertices / 3);
        ImGui::Text("Visible Triangles: %u", visibleVertices / 3);
        ImGui::Separator();
        ImGui::Text("Camera: %.1f, %.1f, %.1f",
            g_manager.camera.pos.x, g_manager.camera.pos.y, g_manager.camera.pos.z);
        ImGui::SliderFloat("Speed", &g_manager.camera.speed, 1.0f, 200.0f);
        ImGui::SliderFloat("Render Dist", &g_manager.camera.render_distance, 50.0f, 1000.0f);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
