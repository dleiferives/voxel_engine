// Core headers
#include "core/Common.h"
#include "core/Camera.h"

// World headers
#include "world/Block.h"
#include "world/Chunk.h"
#include "world/Lighting.h"

// Render headers
#include "render/TextureAtlas.h"
#include "render/ChunkMesher.h"
#include "render/ModelRenderer.h"

// ImGui
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

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

// Global definitions (declared as extern in headers)
MicroBlockPalette g_microPalette;
BlockTextureInfo g_blockTextures[BLOCK_COUNT];
TextureAtlas g_textureAtlas;

void initBlockTextures() {
    g_textureAtlas.init(16);

    // Add textures to atlas
    uint16_t stoneTop = g_textureAtlas.addTexture("textures/stone.png");
    uint16_t stoneSide = stoneTop;
    uint16_t dirtTex = g_textureAtlas.addTexture("textures/dirt.png");
    uint16_t grassTop = g_textureAtlas.addTexture("textures/grass_top.png");
    uint16_t grassSide = g_textureAtlas.addTexture("textures/grass_side.png");
    uint16_t sandTex = g_textureAtlas.addTexture("textures/sand.png");
    uint16_t woodTop = g_textureAtlas.addTexture("textures/wood_top.png");
    uint16_t woodSide = g_textureAtlas.addTexture("textures/wood_side.png");
    uint16_t leavesTex = g_textureAtlas.addTexture("textures/leaves.png");
    uint16_t glassTex = g_textureAtlas.addTexture("textures/glass.png");
    uint16_t brickTex = g_textureAtlas.addTexture("textures/brick.png");

    // Fallback colors if textures don't exist
    uint16_t stoneColor = g_textureAtlas.addColor(glm::vec3(0.5f));
    uint16_t dirtColor = g_textureAtlas.addColor(glm::vec3(0.45f, 0.3f, 0.15f));
    uint16_t grassColor = g_textureAtlas.addColor(glm::vec3(0.2f, 0.6f, 0.2f));
    uint16_t sandColor = g_textureAtlas.addColor(glm::vec3(0.9f, 0.85f, 0.6f));
    uint16_t woodColor = g_textureAtlas.addColor(glm::vec3(0.55f, 0.35f, 0.15f));
    uint16_t leavesColor = g_textureAtlas.addColor(glm::vec3(0.1f, 0.5f, 0.1f));
    uint16_t glassColor = g_textureAtlas.addColor(glm::vec3(0.8f, 0.9f, 1.0f));
    uint16_t brickColor = g_textureAtlas.addColor(glm::vec3(0.7f, 0.3f, 0.2f));

    // Stone - same texture all sides
    g_blockTextures[BLOCK_STONE] = {
        {{stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0},
         {stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0}},
        false, false
    };

    // Dirt - same texture all sides
    g_blockTextures[BLOCK_DIRT] = {
        {{dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0},
         {dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0}},
        false, false
    };

    // Grass - different top/side/bottom
    g_blockTextures[BLOCK_GRASS] = {
        {{grassColor, 0}, {grassColor, 0}, {dirtColor, 0},
         {grassColor, 0}, {grassColor, 0}, {grassColor, 0}},
        false, false
    };

    // Sand
    g_blockTextures[BLOCK_SAND] = {
        {{sandColor, 0}, {sandColor, 0}, {sandColor, 0},
         {sandColor, 0}, {sandColor, 0}, {sandColor, 0}},
        false, false
    };

    // Wood - different top/bottom vs sides
    g_blockTextures[BLOCK_WOOD] = {
        {{woodColor, 0}, {woodColor, 0}, {woodColor, 0},
         {woodColor, 0}, {woodColor, 0}, {woodColor, 0}},
        false, false
    };

    // Leaves - transparent
    g_blockTextures[BLOCK_LEAVES] = {
        {{leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0},
         {leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0}},
        true, false
    };

    // Glass - transparent
    g_blockTextures[BLOCK_GLASS] = {
        {{glassTex, 0}, {glassTex, 0}, {glassTex, 0},
         {glassTex, 0}, {glassTex, 0}, {glassTex, 0}},
        true, false
    };

    // Brick
    g_blockTextures[BLOCK_BRICK] = {
        {{brickColor, 0}, {brickColor, 0}, {brickColor, 0},
         {brickColor, 0}, {brickColor, 0}, {brickColor, 0}},
        false, false
    };

    // Glowstone - bright yellow/orange
    uint16_t glowstoneColor = g_textureAtlas.addColor(glm::vec3(1.0f, 0.9f, 0.5f));
    g_blockTextures[BLOCK_GLOWSTONE] = {
        {{glowstoneColor, 0}, {glowstoneColor, 0}, {glowstoneColor, 0},
         {glowstoneColor, 0}, {glowstoneColor, 0}, {glowstoneColor, 0}},
        false, true  // fullBright = true (ignores lighting)
    };

    // Lava - orange/red
    uint16_t lavaColor = g_textureAtlas.addColor(glm::vec3(1.0f, 0.4f, 0.1f));
    g_blockTextures[BLOCK_LAVA] = {
        {{lavaColor, 0}, {lavaColor, 0}, {lavaColor, 0},
         {lavaColor, 0}, {lavaColor, 0}, {lavaColor, 0}},
        false, true  // fullBright = true
    };

    g_textureAtlas.build();
}

// Block info array
// Format: {category, color, scale, modelId, textureInfo, lightEmission, blocksLight}
BlockInfo BLOCK_INFO[] = {
    // BLOCK_AIR - doesn't block light
    {CATEGORY_AIR, glm::vec3(0.0f), 0.0f, 0, nullptr, 0, false},
    // Solid blocks - all block light
    {CATEGORY_SOLID, glm::vec3(0.5f), 1.0f, 0, &g_blockTextures[BLOCK_STONE], 0, true},           // STONE
    {CATEGORY_SOLID, glm::vec3(0.45f, 0.3f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_DIRT], 0, true},  // DIRT
    {CATEGORY_SOLID, glm::vec3(0.2f, 0.6f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_GRASS], 0, true},   // GRASS
    {CATEGORY_SOLID, glm::vec3(0.9f, 0.85f, 0.6f), 1.0f, 0, &g_blockTextures[BLOCK_SAND], 0, true},   // SAND
    {CATEGORY_SOLID, glm::vec3(0.55f, 0.35f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_WOOD], 0, true}, // WOOD
    {CATEGORY_SOLID, glm::vec3(0.1f, 0.5f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LEAVES], 0, false}, // LEAVES - doesn't fully block light
    {CATEGORY_SOLID, glm::vec3(0.8f, 0.9f, 1.0f), 1.0f, 0, &g_blockTextures[BLOCK_GLASS], 0, false},  // GLASS - doesn't block light
    {CATEGORY_SOLID, glm::vec3(0.7f, 0.3f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_BRICK], 0, true},   // BRICK
    // Light-emitting solid blocks
    {CATEGORY_SOLID, glm::vec3(1.0f, 0.9f, 0.5f), 1.0f, 0, &g_blockTextures[BLOCK_GLOWSTONE], 15, true}, // GLOWSTONE - max light
    {CATEGORY_SOLID, glm::vec3(1.0f, 0.4f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LAVA], 15, true},      // LAVA - max light
    // Model blocks - don't block light
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.2f, 0.2f), 0.4f, 0, nullptr, 0, false},  // FLOWER_RED
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.9f, 0.2f), 0.4f, 0, nullptr, 0, false},  // FLOWER_YELLOW
    {CATEGORY_MODEL, glm::vec3(0.3f, 0.7f, 0.3f), 0.6f, 1, nullptr, 0, false},  // TALL_GRASS
    {CATEGORY_MODEL, glm::vec3(0.8f, 0.7f, 0.6f), 0.3f, 2, nullptr, 0, false},  // MUSHROOM
    {CATEGORY_MODEL, glm::vec3(0.6f, 0.8f, 1.0f), 0.5f, 3, nullptr, 7, false},  // CRYSTAL - emits some light
    {CATEGORY_MODEL, glm::vec3(1.0f, 0.8f, 0.3f), 0.3f, 4, nullptr, 14, false}, // TORCH - bright light
    // Micro blocks - block light
    {CATEGORY_MICRO, glm::vec3(0.7f, 0.5f, 0.3f), 1.0f, 0, nullptr, 0, true},   // MICRO_TERRAIN
    {CATEGORY_MICRO, glm::vec3(0.8f, 0.8f, 0.8f), 1.0f, 1, nullptr, 0, true},   // MICRO_SCULPTURE
};

class ChunkManager {
public:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    std::vector<ChunkVertex> opaqueBuffer;
    std::vector<ChunkVertex> transparentBuffer;

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
                        // Randomly rotate grass blocks
                        uint8_t rot = static_cast<uint8_t>(
                            (int(wx * 7 + wz * 13) % 4));
                        chunk->setBlock(x, y, z, BLOCK_GLASS, rot);

                        int iy = y + 1;
                        if (iy < CHUNK_SIZE &&
                            chunk->getBlock(x, iy, z).type == BLOCK_AIR) {
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
                            } else if (r < 0.20f) {
                                // Add a micro block
                                chunk->setBlock(x, iy, z, BLOCK_MICRO_TERRAIN);
                                generateMicroTerrain(chunk, x, iy, z);
                            } else if (r < 0.21f) {
                                chunk->setBlock(x, iy, z, BLOCK_MICRO_SCULPTURE);
                                generateMicroSculpture(chunk, x, iy, z);
                            }
                        }
                    }

                    // Add some glass windows at certain heights
                    if (wy > height && wy < height + 3) {
                        float gr = fmodf(wx * 5.5f + wz * 3.3f + wy * 2.1f, 1.0f);
                        gr = fmodf(sinf(gr * 12345.6f) * 0.5f + 0.5f, 1.0f);
                        if (gr < 0.01f) {
                            chunk->setBlock(x, y, z, BLOCK_GLASS);
                        }
                    }
                }
            }
        }
    }

    void generateMicroTerrain(Chunk* chunk, int bx, int by, int bz) {
        MicroBlockData* data = chunk->getMicroData(bx, by, bz);
        if (!data) return;

        // Generate a small terrain-like pattern
        for (int z = 0; z < MICRO_BLOCK_SIZE; z++) {
            for (int x = 0; x < MICRO_BLOCK_SIZE; x++) {
                float h = 4.0f + 3.0f * sinf(x * 0.5f) * cosf(z * 0.5f);
                for (int y = 0; y < MICRO_BLOCK_SIZE; y++) {
                    if (y < h) {
                        if (y < h - 2) {
                            data->set(x, y, z, 3);  // Gray (stone)
                        } else if (y < h - 1) {
                            data->set(x, y, z, 15); // Brown (dirt)
                        } else {
                            data->set(x, y, z, 7);  // Green (grass)
                        }
                    }
                }
            }
        }
    }

    void generateMicroSculpture(Chunk* chunk, int bx, int by, int bz) {
        MicroBlockData* data = chunk->getMicroData(bx, by, bz);
        if (!data) return;

        // Generate a simple sculpture (sphere-ish)
        glm::vec3 center(8, 8, 8);
        for (int z = 0; z < MICRO_BLOCK_SIZE; z++) {
            for (int y = 0; y < MICRO_BLOCK_SIZE; y++) {
                for (int x = 0; x < MICRO_BLOCK_SIZE; x++) {
                    float dist = glm::length(glm::vec3(x, y, z) - center);
                    if (dist < 6.0f) {
                        // Color based on distance from center
                        uint8_t colorIdx = static_cast<uint8_t>(
                            6 + (int(dist) % 8));
                        data->set(x, y, z, colorIdx);
                    }
                }
            }
        }
    }

    void rebuildChunkMesh(Chunk* chunk) {
        if (!chunk->needsRebuild) return;

        // Calculate lighting before meshing (light values are baked into vertices)
        if (chunk->needsLightRebuild) {
            calculateChunkLighting(*chunk);
        }

        GreedyMesher::mesh(*chunk, opaqueBuffer, transparentBuffer);

        chunk->vertexCount = opaqueBuffer.size();
        chunk->transparentVertexCount = transparentBuffer.size();

        if (chunk->vertexCount == 0 && chunk->transparentVertexCount == 0) {
            chunk->isEmpty = true;
            chunk->needsRebuild = false;
            return;
        }

        chunk->isEmpty = false;

        // Upload opaque mesh
        if (chunk->vertexCount > 0) {
            if (!chunk->VAO) {
                glGenVertexArrays(1, &chunk->VAO);
                glGenBuffers(1, &chunk->VBO);
            }

            glBindVertexArray(chunk->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
            glBufferData(GL_ARRAY_BUFFER,
                         opaqueBuffer.size() * sizeof(ChunkVertex),
                         opaqueBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        // Upload transparent mesh
        if (chunk->transparentVertexCount > 0) {
            if (!chunk->transparentVAO) {
                glGenVertexArrays(1, &chunk->transparentVAO);
                glGenBuffers(1, &chunk->transparentVBO);
            }

            glBindVertexArray(chunk->transparentVAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->transparentVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         transparentBuffer.size() * sizeof(ChunkVertex),
                         transparentBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        chunk->needsRebuild = false;
        chunk->meshUploaded = true;
    }

    void setupChunkVertexAttribs() {
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, position));
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, normal));
        glEnableVertexAttribArray(1);

        // Color
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, color));
        glEnableVertexAttribArray(2);

        // UV
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, uv));
        glEnableVertexAttribArray(3);

        // Texture layer
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, textureLayer));
        glEnableVertexAttribArray(4);

        // AO
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, ao));
        glEnableVertexAttribArray(5);
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
        "Hybrid Voxels - Textured + Micro Blocks", nullptr, nullptr);
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

    // Initialize texture atlas and block textures
    initBlockTextures();

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
    const int WORLD_SIZE = 8;

    std::cout << "Generating terrain..." << std::endl;
    for (int z = 0; z < WORLD_SIZE; z++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            for (int x = 0; x < WORLD_SIZE; x++) {
                chunkManager.generateTerrain(glm::ivec3(x, y, z));
            }
        }
    }

    std::cout << "Building meshes..." << std::endl;
    int count = 0;
    for (auto& [hash, chunk] : chunkManager.chunks) {
        chunkManager.rebuildChunkMesh(chunk.get());
        std::cout << count << "/" << WORLD_SIZE * WORLD_SIZE * WORLD_SIZE << std::endl;
        count+=1;
    }

    std::cout << "Collecting model blocks..." << std::endl;
    chunkManager.collectAllModelBlocks(modelRenderer);
    std::cout << "Total model instances: " << modelRenderer.allInstances.size()
              << std::endl;

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Frustum frustum;
    uint32_t totalChunks = 0, visibleChunks = 0;
    uint32_t totalOpaqueVerts = 0, visibleOpaqueVerts = 0;
    uint32_t totalTransparentVerts = 0, visibleTransparentVerts = 0;

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

        // Bind texture atlas
        g_textureAtlas.bind(0);

        // Setup chunk shader
        glUseProgram(chunkProgram);
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(chunkProgram, "viewPos"), 1,
                     glm::value_ptr(g_manager.camera.pos));
        glUniform1i(glGetUniformLocation(chunkProgram, "textureArray"), 0);

        totalChunks = 0;
        visibleChunks = 0;
        totalOpaqueVerts = 0;
        visibleOpaqueVerts = 0;
        totalTransparentVerts = 0;
        visibleTransparentVerts = 0;

        // Render opaque geometry first
        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded) continue;

            totalChunks++;
            totalOpaqueVerts += chunk->vertexCount;
            totalTransparentVerts += chunk->transparentVertexCount;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (!frustum.isBoxVisible(minPos, maxPos)) continue;

            visibleChunks++;

            if (chunk->vertexCount > 0) {
                visibleOpaqueVerts += chunk->vertexCount;
                glBindVertexArray(chunk->VAO);
                glDrawArrays(GL_TRIANGLES, 0,
                             static_cast<GLsizei>(chunk->vertexCount));
            }
        }

        // Render GPU-culled model blocks
        glDisable(GL_CULL_FACE);
        modelRenderer.cullAndRender(frustum.planes, view, projection,
                                    g_manager.camera.pos);
        glEnable(GL_CULL_FACE);

        // Render transparent geometry last (with blending)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glUseProgram(chunkProgram);

        // Collect chunks with transparent geometry
        struct TransparentJob {
            Chunk* chunk;
            float distanceSq;
        };
        std::vector<TransparentJob> transparentJobs;

        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded || chunk->transparentVertexCount == 0)
                continue;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (frustum.isBoxVisible(minPos, maxPos)) {
                glm::vec3 chunkCenter = minPos + glm::vec3(CHUNK_SIZE / 2.0f);
                float distSq = glm::distance2(g_manager.camera.pos, chunkCenter);
                transparentJobs.push_back({chunk.get(), distSq});
            }
        }

        // Sort Back-to-Front (highest distance first)
        std::sort(transparentJobs.begin(), transparentJobs.end(),
            [](const TransparentJob& a, const TransparentJob& b) {
                return a.distanceSq > b.distanceSq;
            });

        // Draw sorted chunks
        for (auto& job : transparentJobs) {
            visibleTransparentVerts += job.chunk->transparentVertexCount;
            glBindVertexArray(job.chunk->transparentVAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(job.chunk->transparentVertexCount));
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

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
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "Solid Blocks (Textured Greedy Mesh):");
        ImGui::Text("  Chunks: %u / %u visible", visibleChunks, totalChunks);
        ImGui::Text("  Opaque Verts: %u / %u", visibleOpaqueVerts, totalOpaqueVerts);
        ImGui::Text("  Transparent Verts: %u / %u", visibleTransparentVerts,
                    totalTransparentVerts);
        ImGui::Text("  Triangles: %u",
                    (visibleOpaqueVerts + visibleTransparentVerts) / 3);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f),
                           "Model Blocks (GPU Culled):");
        ImGui::Text("  Total: %zu", modelRenderer.allInstances.size());
        ImGui::Text("  Visible: %u", modelRenderer.visibleCount);
        ImGui::Text("  Culled: %.1f%%",
                    modelRenderer.allInstances.size() > 0
                    ? 100.0f * (1.0f - (float)modelRenderer.visibleCount /
                               modelRenderer.allInstances.size())
                    : 0.0f);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "Texture Atlas:");
        ImGui::Text("  Textures: %d", g_textureAtlas.layerCount);
        ImGui::Text("  Tile Size: %d", g_textureAtlas.tileSize);

        ImGui::Separator();
        ImGui::Text("Camera: %.1f, %.1f, %.1f",
                    g_manager.camera.pos.x, g_manager.camera.pos.y,
                    g_manager.camera.pos.z);
        ImGui::SliderFloat("Speed", &g_manager.camera.speed, 1.0f, 200.0f);
        ImGui::SliderFloat("Render Dist", &g_manager.camera.render_distance,
                           50.0f, 1000.0f);
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
    g_textureAtlas.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
