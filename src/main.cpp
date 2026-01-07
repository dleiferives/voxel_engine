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

// Embedded shaders
const char chunkVertSource[] = {
#embed "shaders/chunk.vert"
    , 0
};
const char chunkFragSource[] = {
#embed "shaders/chunk.frag"
    , 0
};
const char modelVertSource[] = {
#embed "shaders/model.vert"
    , 0
};
const char modelFragSource[] = {
#embed "shaders/model.frag"
    , 0
};
const char cullingCompSource[] = {
#embed "shaders/culling.comp"
    , 0
};

// Constants
constexpr int CHUNK_SIZE = 32;
constexpr size_t MAX_MODEL_INSTANCES = 1000000;

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

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

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

// Block classification
enum BlockCategory : uint8_t {
    CATEGORY_AIR = 0,
    CATEGORY_SOLID,    // Greedy meshed
    CATEGORY_MODEL,    // GPU culled instances
};

// Block types
enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    // Solid blocks (greedy meshed)
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    // Model blocks (GPU culled)
    BLOCK_FLOWER_RED,
    BLOCK_FLOWER_YELLOW,
    BLOCK_TALL_GRASS,
    BLOCK_MUSHROOM,
    BLOCK_CRYSTAL,
    BLOCK_COUNT
};

struct BlockInfo {
    BlockCategory category;
    glm::vec3 color;
    float scale;      // For model blocks
    uint32_t modelId; // Which model mesh to use
};

const BlockInfo BLOCK_INFO[] = {
    {CATEGORY_AIR,   glm::vec3(0.0f), 0.0f, 0},                           // AIR
    {CATEGORY_SOLID, glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0},               // STONE
    {CATEGORY_SOLID, glm::vec3(0.45f, 0.3f, 0.15f), 1.0f, 0},             // DIRT
    {CATEGORY_SOLID, glm::vec3(0.2f, 0.6f, 0.2f), 1.0f, 0},               // GRASS
    {CATEGORY_SOLID, glm::vec3(0.9f, 0.85f, 0.6f), 1.0f, 0},              // SAND
    {CATEGORY_SOLID, glm::vec3(0.55f, 0.35f, 0.15f), 1.0f, 0},            // WOOD
    {CATEGORY_SOLID, glm::vec3(0.1f, 0.5f, 0.1f), 1.0f, 0},               // LEAVES
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.2f, 0.2f), 0.4f, 0},               // FLOWER_RED
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.9f, 0.2f), 0.4f, 0},               // FLOWER_YELLOW
    {CATEGORY_MODEL, glm::vec3(0.3f, 0.7f, 0.3f), 0.6f, 1},               // TALL_GRASS
    {CATEGORY_MODEL, glm::vec3(0.8f, 0.7f, 0.6f), 0.3f, 2},               // MUSHROOM
    {CATEGORY_MODEL, glm::vec3(0.6f, 0.8f, 1.0f), 0.5f, 3},               // CRYSTAL
};

// Vertex structures
struct ChunkVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Model instance data for GPU culling
struct ModelInstance {
    glm::vec4 positionAndScale;  // xyz = position, w = scale
    glm::vec4 colorAndType;      // xyz = color, w = modelId
};

// Indirect draw command
struct DrawArraysIndirectCommand {
    GLuint vertexCount;
    GLuint instanceCount;
    GLuint firstVertex;
    GLuint baseInstance;
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
    glm::ivec3 chunkPos;
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> blocks;

    // Greedy mesh data
    GLuint VAO = 0, VBO = 0;
    size_t vertexCount = 0;
    bool needsRebuild = true;
    bool isEmpty = true;
    bool meshUploaded = false;

    // Model blocks in this chunk
    std::vector<ModelInstance> modelBlocks;
    bool modelsDirty = true;

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
        modelsDirty = true;
        isEmpty = false;
    }

    glm::vec3 getWorldPos() const {
        return glm::vec3(chunkPos) * float(CHUNK_SIZE);
    }

    void getBoundingBox(glm::vec3& minPos, glm::vec3& maxPos) const {
        minPos = getWorldPos();
        maxPos = minPos + glm::vec3(CHUNK_SIZE);
    }

    void collectModelBlocks() {
        if (!modelsDirty) return;

        modelBlocks.clear();
        glm::vec3 worldBase = getWorldPos();

        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    uint8_t type = blocks[index(x, y, z)];
                    if (type == BLOCK_AIR) continue;

                    const BlockInfo& info = BLOCK_INFO[type];
                    if (info.category != CATEGORY_MODEL) continue;

                    glm::vec3 pos = worldBase + glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f);

                    ModelInstance inst;
                    inst.positionAndScale = glm::vec4(pos, info.scale);
                    inst.colorAndType = glm::vec4(info.color, float(info.modelId));
                    modelBlocks.push_back(inst);
                }
            }
        }

        modelsDirty = false;
    }
};

// Greedy mesher (only for SOLID blocks)
class GreedyMesher {
public:
    static void mesh(Chunk& chunk, std::vector<ChunkVertex>& vertices) {
        vertices.clear();

        for (int face = 0; face < 6; face++) {
            meshFace(chunk, vertices, face);
        }
    }

private:
    static void meshFace(Chunk& chunk, std::vector<ChunkVertex>& vertices, int face) {
        int axis = face / 2;
        bool positive = face % 2;

        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;

        glm::ivec3 dir(0);
        dir[axis] = positive ? 1 : -1;

        glm::vec3 normal = FACE_NORMALS[face];
        std::array<int, CHUNK_SIZE * CHUNK_SIZE> mask;

        for (int d = 0; d < CHUNK_SIZE; d++) {
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    glm::ivec3 pos(0);
                    pos[axis] = d;
                    pos[u] = i;
                    pos[v] = j;

                    uint8_t block = chunk.getBlock(pos.x, pos.y, pos.z);
                    const BlockInfo& blockInfo = BLOCK_INFO[block];

                    // Only mesh SOLID blocks
                    if (blockInfo.category != CATEGORY_SOLID) {
                        mask[i + j * CHUNK_SIZE] = 0;
                        continue;
                    }

                    glm::ivec3 neighborPos = pos + dir;
                    uint8_t neighbor;
                    if (neighborPos[axis] < 0 || neighborPos[axis] >= CHUNK_SIZE) {
                        neighbor = BLOCK_AIR;
                    } else {
                        neighbor = chunk.getBlock(neighborPos.x, neighborPos.y, neighborPos.z);
                    }

                    // Face visible if neighbor is not solid
                    const BlockInfo& neighborInfo = BLOCK_INFO[neighbor];
                    if (neighborInfo.category != CATEGORY_SOLID) {
                        mask[i + j * CHUNK_SIZE] = block;
                    } else {
                        mask[i + j * CHUNK_SIZE] = 0;
                    }
                }
            }

            // Greedy mesh
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE;) {
                    int blockType = mask[i + j * CHUNK_SIZE];
                    if (blockType == 0) {
                        i++;
                        continue;
                    }

                    int w = 1;
                    while (i + w < CHUNK_SIZE && mask[i + w + j * CHUNK_SIZE] == blockType) {
                        w++;
                    }

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

                    glm::ivec3 pos(0);
                    pos[axis] = d;
                    pos[u] = i;
                    pos[v] = j;

                    glm::vec3 worldPos = chunk.getWorldPos() + glm::vec3(pos);
                    if (positive) {
                        worldPos[axis] += 1.0f;
                    }

                    glm::vec3 du(0), dv(0);
                    du[u] = float(w);
                    dv[v] = float(h);

                    glm::vec3 color = BLOCK_INFO[blockType].color;

                    float shade = 1.0f;
                    if (face == FACE_NEG_Y) shade = 0.5f;
                    else if (face == FACE_NEG_X || face == FACE_POS_X) shade = 0.7f;
                    else if (face == FACE_NEG_Z || face == FACE_POS_Z) shade = 0.8f;
                    color *= shade;

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

// Model geometry generator
class ModelGeometry {
public:
    // Cube vertices for model blocks
    static std::vector<float> cubeVertices;

    static void init() {
        cubeVertices = {
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        };
    }
};

std::vector<float> ModelGeometry::cubeVertices;

// GPU Model Renderer with compute culling
class ModelRenderer {
public:
    GLuint modelVAO = 0, modelVBO = 0;
    GLuint inputSSBO = 0;
    GLuint outputSSBO = 0;
    GLuint indirectBuffer = 0;
    GLuint atomicBuffer = 0;
    GLuint computeProgram = 0;
    GLuint renderProgram = 0;

    std::vector<ModelInstance> allInstances;
    GLuint visibleCount = 0;

    void init(const char* computeSource, const char* vertSource, const char* fragSource) {
        ModelGeometry::init();

        // Compile compute shader
        GLuint cs = compileShader(GL_COMPUTE_SHADER, computeSource);
        computeProgram = glCreateProgram();
        glAttachShader(computeProgram, cs);
        glLinkProgram(computeProgram);
        glDeleteShader(cs);

        // Compile render shaders
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSource);
        renderProgram = glCreateProgram();
        glAttachShader(renderProgram, vs);
        glAttachShader(renderProgram, fs);
        glLinkProgram(renderProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);

        // Setup VAO for model geometry
        glGenVertexArrays(1, &modelVAO);
        glGenBuffers(1, &modelVBO);

        glBindVertexArray(modelVAO);
        glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
        glBufferData(GL_ARRAY_BUFFER,
            ModelGeometry::cubeVertices.size() * sizeof(float),
            ModelGeometry::cubeVertices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Create SSBOs
        glGenBuffers(1, &inputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
            MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &outputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
            MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, GL_DYNAMIC_DRAW);

        // Bind output as instance data
        glBindBuffer(GL_ARRAY_BUFFER, outputSSBO);
        // Instance position + scale
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
            (void*)offsetof(ModelInstance, positionAndScale));
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);
        // Instance color + type
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
            (void*)offsetof(ModelInstance, colorAndType));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        // Indirect buffer
        glGenBuffers(1, &indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        DrawArraysIndirectCommand cmd = { 36, 0, 0, 0 };
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);

        // Atomic counter
        glGenBuffers(1, &atomicBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    void uploadInstances() {
        if (allInstances.empty()) return;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
            allInstances.size() * sizeof(ModelInstance), allInstances.data());
    }

    void cullAndRender(const glm::vec4 frustumPlanes[6], const glm::mat4& view,
                       const glm::mat4& projection, const glm::vec3& viewPos) {
        if (allInstances.empty()) {
            visibleCount = 0;
            return;
        }

        GLuint totalInstances = static_cast<GLuint>(allInstances.size());

        // Reset atomic counter
        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);

        // Dispatch compute shader
        glUseProgram(computeProgram);
        glUniform4fv(glGetUniformLocation(computeProgram, "frustumPlanes"), 6,
            glm::value_ptr(frustumPlanes[0]));
        glUniform1ui(glGetUniformLocation(computeProgram, "totalInstances"), totalInstances);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, atomicBuffer);

        GLuint numGroups = (totalInstances + 255) / 256;
        glDispatchCompute(numGroups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        // Copy count to indirect buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, indirectBuffer);
        glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_COPY_WRITE_BUFFER,
            0, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(GLuint));

        // Read back for stats
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &visibleCount);

        // Render
        glUseProgram(renderProgram);
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "view"), 1, GL_FALSE,
            glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(renderProgram, "viewPos"), 1,
            glm::value_ptr(viewPos));

        glBindVertexArray(modelVAO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &modelVAO);
        glDeleteBuffers(1, &modelVBO);
        glDeleteBuffers(1, &inputSSBO);
        glDeleteBuffers(1, &outputSSBO);
        glDeleteBuffers(1, &indirectBuffer);
        glDeleteBuffers(1, &atomicBuffer);
        glDeleteProgram(computeProgram);
        glDeleteProgram(renderProgram);
    }

private:
    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader error: " << infoLog << std::endl;
        }
        return shader;
    }
};

// Chunk manager
class ChunkManager {
public:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    std::vector<ChunkVertex> meshBuffer;

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

        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                float wx = worldBase.x + x;
                float wz = worldBase.z + z;

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

                        // Add decorations on grass
                        int iy = y + 1;
                        if (iy < CHUNK_SIZE && chunk->getBlock(x, iy, z) == BLOCK_AIR) {
                            float r = fmodf(wx * 12.9898f + wz * 78.233f, 1.0f);
                            r = fmodf(sinf(r * 43758.5453f) * 0.5f + 0.5f, 1.0f);

                            if (r < 0.02f) {
                                chunk->setBlock(x, iy, z, BLOCK_FLOWER_RED);
                            } else if (r < 0.04f) {
                                chunk->setBlock(x, iy, z, BLOCK_FLOWER_YELLOW);
                            } else if (r < 0.15f) {
                                chunk->setBlock(x, iy, z, BLOCK_TALL_GRASS);
                            } else if (r < 0.17f) {
                                chunk->setBlock(x, iy, z, BLOCK_MUSHROOM);
                            } else if (r < 0.18f) {
                                chunk->setBlock(x, iy, z, BLOCK_CRYSTAL);
                            }
                        }
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

        if (!chunk->VAO) {
            glGenVertexArrays(1, &chunk->VAO);
            glGenBuffers(1, &chunk->VBO);
        }

        glBindVertexArray(chunk->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
        glBufferData(GL_ARRAY_BUFFER,
            meshBuffer.size() * sizeof(ChunkVertex),
            meshBuffer.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
            (void*)offsetof(ChunkVertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
            (void*)offsetof(ChunkVertex, normal));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
            (void*)offsetof(ChunkVertex, color));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        chunk->needsRebuild = false;
        chunk->meshUploaded = true;
    }

    void collectAllModelBlocks(ModelRenderer& renderer) {
        renderer.allInstances.clear();

        for (auto& [hash, chunk] : chunks) {
            chunk->collectModelBlocks();
            renderer.allInstances.insert(
                renderer.allInstances.end(),
                chunk->modelBlocks.begin(),
                chunk->modelBlocks.end()
            );
        }

        renderer.uploadInstances();
    }
};

// Frustum
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
        std::cerr << "Shader error:\n" << infoLog << std::endl;
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
        "Hybrid Voxels - Greedy Mesh + GPU Culled Models", nullptr, nullptr);
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

    // Compile chunk shaders
    GLuint chunkVS = compileShader(GL_VERTEX_SHADER, chunkVertSource);
    GLuint chunkFS = compileShader(GL_FRAGMENT_SHADER, chunkFragSource);
    GLuint chunkProgram = glCreateProgram();
    glAttachShader(chunkProgram, chunkVS);
    glAttachShader(chunkProgram, chunkFS);
    glLinkProgram(chunkProgram);
    glDeleteShader(chunkVS);
    glDeleteShader(chunkFS);

    // Initialize model renderer
    ModelRenderer modelRenderer;
    modelRenderer.init(cullingCompSource, modelVertSource, modelFragSource);

    // Generate chunks
    ChunkManager chunkManager;
    const int WORLD_SIZE = 4;

    std::cout << "Generating terrain..." << std::endl;
    for (int z = 0; z < WORLD_SIZE; z++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            for (int x = 0; x < WORLD_SIZE; x++) {
                chunkManager.generateTerrain(glm::ivec3(x, y, z));
            }
        }
    }

    std::cout << "Building meshes..." << std::endl;
    for (auto& [hash, chunk] : chunkManager.chunks) {
        chunkManager.rebuildChunkMesh(chunk.get());
    }

    std::cout << "Collecting model blocks..." << std::endl;
    chunkManager.collectAllModelBlocks(modelRenderer);
    std::cout << "Total model instances: " << modelRenderer.allInstances.size() << std::endl;

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Frustum frustum;
    uint32_t totalChunks = 0, visibleChunks = 0;
    uint32_t totalMeshVerts = 0, visibleMeshVerts = 0;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_manager.delta_time = currentFrame - g_manager.last_frame;
        g_manager.last_frame = currentFrame;

        processInput(window);

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

        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render greedy-meshed chunks
        glUseProgram(chunkProgram);
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "view"), 1, GL_FALSE,
            glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(chunkProgram, "viewPos"), 1,
            glm::value_ptr(g_manager.camera.pos));

        totalChunks = 0;
        visibleChunks = 0;
        totalMeshVerts = 0;
        visibleMeshVerts = 0;

        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded) continue;

            totalChunks++;
            totalMeshVerts += chunk->vertexCount;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (!frustum.isBoxVisible(minPos, maxPos)) continue;

            visibleChunks++;
            visibleMeshVerts += chunk->vertexCount;

            glBindVertexArray(chunk->VAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(chunk->vertexCount));
        }

        // Render GPU-culled model blocks
        glDisable(GL_CULL_FACE);  // Models might need both sides
        modelRenderer.cullAndRender(frustum.planes, view, projection, g_manager.camera.pos);
        glEnable(GL_CULL_FACE);

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
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Solid Blocks (Greedy Mesh):");
        ImGui::Text("  Chunks: %u / %u visible", visibleChunks, totalChunks);
        ImGui::Text("  Vertices: %u / %u", visibleMeshVerts, totalMeshVerts);
        ImGui::Text("  Triangles: %u", visibleMeshVerts / 3);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f), "Model Blocks (GPU Culled):");
        ImGui::Text("  Total: %zu", modelRenderer.allInstances.size());
        ImGui::Text("  Visible: %u", modelRenderer.visibleCount);
        ImGui::Text("  Culled: %.1f%%",
            modelRenderer.allInstances.size() > 0
                ? 100.0f * (1.0f - (float)modelRenderer.visibleCount / modelRenderer.allInstances.size())
                : 0.0f);

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

    glDeleteProgram(chunkProgram);
    modelRenderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
